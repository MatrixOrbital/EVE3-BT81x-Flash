#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <stdlib.h>
#include "Eve2_81x.h"           
#include "MatrixEve2Conf.h"      // Header for EVE2 Display configuration settings
#include "process.h"
#include "Arduino_AL.h"

File myFile;
char LogBuf[WorkBuffSz];

void setup()
{
  uint8_t FlashStatus;

//  MyDelay(3000);  // This is just here to give us time to power cycle after a code update

  // Initializations.  Order is important
  GlobalInit();
  FT81x_Init();
  SD_Init();

  FlashAttach();                           // Attach flash
  FlashLoad();                             // Conditionally copy output.bin to Eve attached flash
  
  if(!FlashFast())                         // Set flash to fast mode - QSPI
  {
    // Let's say that if fast mode fails, then probably the flash is corrupted so let's reload flash  
    if(SD.exists("transfer.fin"))
    {
      SD.remove("transfer.fin");
      MyDelay(50);
    }
    FlashLoad();                           // Copy output.bin to Eve attached flash
  }
  
  wr8(REG_PWM_DUTY + RAM_REG, 128);        // set backlight
  if ( FlashGetFileParms() )               // Stuff the file list buffer from flash
  {  
    uint8_t Index = 2;                     // The first two files that we parse are always the blob and the map
    while(1)                               // main loop which actually loops unlike loop() which does not.
    {
      MakeScreen_Bitmap(Index++);
      if(Index > 7)
        Index = 2; 
      MyDelay(5000);
    }
  }
  else
  {
    Log("Bad flash parsing");
    while(1);                              // Can not continue if the flash is not right (unless this is a thing and you could reload here potentially)   
  }               
}

// ************************************************************************************
// Following are wrapper functions for C++ Arduino functions so that they may be      *
// called from outside of C++ files.  These are also your opportunity to use a common *
// name for your hardware functions - no matter the hardware.  In Arduino-world you   *
// interact with hardware using Arduino built-in functions which are all C++ and so   *
// your "abstraction layer" must live in this xxx.ino file where C++ works.           *
//                                                                                    *
// This is also an alternative to ifdef-elif hell.  A different target                *
// processor or compiler will include different files for hardware abstraction, but   *
// the core "library" files remain unaltered - and clean.  Applications built on top  *
// of the libraries need not know which processor or compiler they are running /      *
// compiling on (in general and within reason)                                        *
// ************************************************************************************

void GlobalInit(void)
{
  Wire.begin();                          // Setup I2C bus

  Serial.begin(115200);                  // Setup serial port for debug
  while (!Serial) {;}                    // wait for serial port to connect.
  
  // Matrix Orbital Eve display interface initialization
  pinMode(EvePDN_PIN, OUTPUT);            // Pin setup as output for Eve PDN pin.
  SetPin(EvePDN_PIN, 0);                  // Apply a resetish condition on Eve
  pinMode(EveChipSelect_PIN, OUTPUT);     // SPI CS Initialization
  SetPin(EveChipSelect_PIN, 1);           // Deselect Eve
  pinMode(EveAudioEnable_PIN, OUTPUT);    // Audio Enable PIN
  SetPin(EveAudioEnable_PIN, 0);          // Disable Audio

  SPI.begin();                            // Enable SPI
//  Log("Startup\n");
}

// Send a single byte through SPI
void SPI_WriteByte(uint8_t data)
{
  SPI.beginTransaction(SPISettings(SPISpeed, MSBFIRST, SPI_MODE0));
  digitalWrite(EveChipSelect_PIN, LOW);

  SPI.transfer(data);
      
  digitalWrite(EveChipSelect_PIN, HIGH);
  SPI.endTransaction();
}

// Send a series of bytes (contents of a buffer) through SPI
void SPI_WriteBuffer(uint8_t *Buffer, uint32_t Length)
{
  SPI.beginTransaction(SPISettings(SPISpeed, MSBFIRST, SPI_MODE0));
  digitalWrite(EveChipSelect_PIN, LOW);

  SPI.transfer(Buffer, Length);
      
  digitalWrite(EveChipSelect_PIN, HIGH);
  SPI.endTransaction();
}

// Send a byte through SPI as part of a larger transmission.  Does not enable/disable SPI CS
void SPI_Write(uint8_t data)
{
//  Log("W-0x%02x\n", data);
  SPI.transfer(data);
}

// Read a series of bytes from SPI and store them in a buffer
void SPI_ReadBuffer(uint8_t *Buffer, uint32_t Length)
{
  uint8_t a = SPI.transfer(0x00); // dummy read

  while (Length--)
  {
    *(Buffer++) = SPI.transfer(0x00);
  }
}

// Enable SPI by activating chip select line
void SPI_Enable(void)
{
  SPI.beginTransaction(SPISettings(SPISpeed, MSBFIRST, SPI_MODE0));
  digitalWrite(EveChipSelect_PIN, LOW);
}

// Disable SPI by deasserting the chip select line
void SPI_Disable(void)
{
  digitalWrite(EveChipSelect_PIN, HIGH);
  SPI.endTransaction();
}

void Eve_Reset_HW(void)
{
  // Reset Eve
  SetPin(EvePDN_PIN, 0);                    // Set the Eve PDN pin low
  MyDelay(50);                              // delay
  SetPin(EvePDN_PIN, 1);                    // Set the Eve PDN pin high
  MyDelay(100);                             // delay
}

void DebugPrint(char *str)
{
  Serial.print(str);
}

// A millisecond delay wrapper for the Arduino function
void MyDelay(uint32_t DLY)
{
  uint32_t wait;
  wait = millis() + DLY; while(millis() < wait);
}

// Externally accessible abstraction for millis()
uint32_t MyMillis(void)
{
  return millis();
}

// An abstracted pin write that may be called from outside this file.
void SetPin(uint8_t pin, bool state)
{
  digitalWrite(pin, state); 
}

// An abstracted pin read that may be called from outside this file.
uint8_t ReadPin(uint8_t pin)
{
  return(digitalRead(pin));
}

//================================== SD Card Functions ====================================
void SD_Init(void)
{
//  Log("Initializing SD card...\n");
  if (!SD.begin(SDChipSelect_PIN)) 
  {
    Log("SD initialization failed!\n");
    return;
  }
//  Log("SD initialization done\n");
}

// Read the touch digitizer calibration matrix values from the Eve and write them to a file
void SaveTouchMatrix(void)
{
  uint8_t count = 0;
  uint32_t data;
  uint32_t address = REG_TOUCH_TRANSFORM_A + RAM_REG;
  
//  Log("Enter SaveTouchMatrix\n");
  
  // If the file exists already from previous run, then delete it.
  if(SD.exists("tmatrix.txt"))
  {
    SD.remove("tmatrix.txt");
    MyDelay(50);
  }
  
  FileOpen("tmatrix.txt", FILEWRITE);
  if(!myFileIsOpen())
  {
//    Log("No create file\n");
    FileClose();
    return false;
  }
  
  do
  {
    data = rd32(address + (count * 4));
//    Log("TM%dw: 0x%08lx\n", count, data);
    FileWrite(data & 0xff);                // Little endian file storage to match Eve
    FileWrite((data >> 8) & 0xff);
    FileWrite((data >> 16) & 0xff);
    FileWrite((data >> 24) & 0xff);
    count++;
  }while(count < 6);
  FileClose();
//  Log("Matrix Saved\n\n");
}

// Read the touch digitizer calibration matrix values from a file and write them to the Eve.
bool LoadTouchMatrix(void)
{
  uint8_t count = 0;
  uint32_t data;
  uint32_t address = REG_TOUCH_TRANSFORM_A + RAM_REG;
  
  FileOpen("tmatrix.txt", FILEREAD);
  if(!myFileIsOpen())
  {
//    Log("tmatrix.txt not open\n");
    FileClose();
    return false;
  }
  
  do
  {
    data = FileReadByte() +  ((uint32_t)FileReadByte() << 8) + ((uint32_t)FileReadByte() << 16) + ((uint32_t)FileReadByte() << 24);
//    Log("TM%dr: 0x%08lx\n", count, data);
    wr32(address + (count * 4), data);
    count++;
  }while(count < 6);
  
  FileClose();
//  Log("Matrix Loaded \n\n");
  return true;
}

// ************************************************************************************
// Following are abstracted file operations for Arduino.  This is possible by using a * 
// global pointer to a single file.  It is enough for our needs and it hides file     *
// handling details within the abstraction.                                           *
// ************************************************************************************
bool FileExists(char *filename)
{
  if(SD.exists(filename))
    return true;
  else
    return false;
}

void FileOpen(char *filename, uint8_t mode)
{
  // Since one also loses access to defined values like FILE_READ from outside the .ino
  // I have been forced to make up values and pass them here (mode) where I can use the 
  // Arduino defines.
  switch(mode)
  {
  case FILEREAD:
    myFile = SD.open(filename, FILE_READ);
    break;
  case FILEWRITE:
    myFile = SD.open(filename, FILE_WRITE);
    break;
  default:;
  }
}

void FileClose(void)
{
  myFile.close();
//  if(myFileIsOpen())
//  {
//    Log("Failed to close file\n");
//  }
}

// Read a single byte from a file
uint8_t FileReadByte(void)
{
  return(myFile.read());
}

// Read bytes from a file into a provided buffer
void FileReadBuf(uint8_t *data, uint32_t NumBytes)
{
  myFile.read(data, NumBytes);
}

void FileWrite(uint8_t data)
{
  myFile.write(data);
}

// Write a string of characters to a file
// MaxChars does not include the null terminator of the source string.
// We make no attempt to detect the usage of MaxChars and simply truncate the output
void FileWriteStr(uint8_t *str, uint16_t MaxChars)
{
  int16_t count = 0; 

  FileOpen("pidlog.txt", FILEWRITE);
  if(!myFileIsOpen())
  {
    Log("No file\n");
    FileClose();
    return;
  }
  
  // Write out the string until the terminator or until we've written the max
  while( (count < MaxChars) && str[count] )
  {
    FileWrite(str[count]);
    count++;
  }
  FileClose();
}

uint32_t FileSize(void)
{
  return(myFile.size());
}

uint32_t FilePosition(void)
{
  return(myFile.position());
}

bool FileSeek(uint32_t offset)
{
  return(myFile.seek(offset));
}

bool myFileIsOpen(void)
{
  if(myFile)
    return true;
  else
    return false;
}

