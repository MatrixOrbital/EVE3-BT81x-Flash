// Process.c is the application layer.  All function calls are hardware ambivalent.
//
#include <stdint.h>                // Find integer types like "uint8_t"  
#include "Eve2_81x.h"              // Matrix Orbital Eve2 Driver
#include "Arduino_AL.h"            // include the hardware specific abstraction layer header for the specific hardware in use.
#include "MatrixEve2Conf.h"        // Header for EVE2 Display configuration settings
#include "process.h"               // Every c file has it's header and this is the one for this file

// The size of the buffer we use for data transfers.  It is a tiny buffer in Arduino Uno
#define COPYBUFSIZE WorkBuffSz
#define NUM_IMAGES                 8
#define LINE_LIMIT                35     // should never exceed 33

uint32_t Time2CheckTouch = 0;            // "Private" variable holding time of next check for user input
uint32_t PressTimeout = 0;               // "Private" variable counting down time you can spend pressing the screen
FileParms FoundFiles[NUM_IMAGES];        // "Private" array of filenames and parameters found in Eve attached flash

void MakeScreen_Bitmap(uint8_t FileIndex)
{
  uint32_t tmp = FoundFiles[FileIndex].FlashAddress;
  Log("Bitmap at %lu\n", tmp);
  
  Send_CMD(CMD_DLSTART);
  Send_CMD(CLEAR(1,1,1)); 
  Send_CMD(CMD_FLASHSOURCE);                        // Tell the next command where to get it's data from in flash
  Send_CMD(RAM_FLASH | tmp);                        // This is the address in Eve attached flash
   
  Cmd_SetBitmap(RAM_FLASH | tmp/32, COMPRESSED_RGBA_ASTC_8x8_KHR, 800, 480); 
  Send_CMD(BEGIN(BITMAPS)); 
  Send_CMD(VERTEX2II(0, 0, 0, 0)); 
  Send_CMD(END()); 
  Send_CMD(DISPLAY());
  Send_CMD(CMD_SWAP);
  UpdateFIFO();                                      
}

bool FlashGetFileParms(void)
{
  // Transfer the first 1024 bytes of flash into RAM_G or you could just transfer enough to get the actual size of the map and then do it again
//  Log("FlashGetFileParms Reading Flash\n");
  Send_CMD(CMD_FLASHREAD);
  Send_CMD(RAM_G);                                                    // Address we are writing to
  Send_CMD(RAM_FLASH_POSTBLOB);                                       // Address we are reading from
  Send_CMD(0x00000400);                                               // It will be 1K bytes - enough for quite a large list but check this
  UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
  Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.

  // File format for the .map is: 
  // Filename = 12 characters padded with spaces
  // a space and a colon and a space
  // beginning address field of 6 characters padded with spaces
  // a space and a colon and a space
  // file size field of variable length - no padding
  // end of line and carriage return (0D 0A)
  // More of the same if needed
  // One or more zeros to pad to 0x10 boundary.  I have no proof that this zero will be provided if the previous data happens to fall on the boundary.

  uint32_t WorkingAddress = RAM_G;
  for(uint8_t i = 0; i < NUM_IMAGES; i++)                             // One must assume that things are as planned and there are NUM_IMAGES images
  {
    uint8_t CharCount = 0; 
    uint8_t tmpBuf[LINE_LIMIT];
    
    while (CharCount < LINE_LIMIT)
    {
      // get bytes and place in buffer.
      tmpBuf[CharCount] = rd8(WorkingAddress++);

      // Before going any further check for a zero.  There are no zeros anywhere until we have run out of map file
      // Since we are done when we read enough lines, the zero should never be seen by us, so any zero encountered is an error
      if (tmpBuf[CharCount] == 0x00)                                  // if we got a zero
        return false;
      
      if (tmpBuf[CharCount] == 0x0D)                                  // if we got the line end
      {
        if (rd8(WorkingAddress++) == 0x0A)                            // then see if we get the carriage return
        {
          // We found an entry and we know where we are so let's monopolize on that and store the rest of the parameters now
          // If Bridgetek also save media parameters in the map file, we could parse and store things like bitmap dimensions
          // or number of frames of an animation and many other such useful information that we are now forced to hardcode.
          // We could make our own map file in a text editor
         
          CharCount = 0;
          while (tmpBuf[CharCount++] != 0x20);                        // run through the tmpbuf again looking for the first space
          tmpBuf[CharCount-1] = 0x00;                                 // null terminate the filename string
          memcpy(FoundFiles[i].FileName, tmpBuf, strlen(tmpBuf)+1);   // Copy the file name

          // the rest of the buffer is still valid - go to start of the start address (index = 15)
          // look for the next occurance of a space and replace that with a zero to make a string of it
          CharCount = 15;
          while (tmpBuf[CharCount++] != 0x20);
          tmpBuf[CharCount-1] = 0x00;                                 // null terminate the start address string

          // I intended to use something like atol() or strtoul(), but could get neither to work properly 
          uint32_t tmp = 0;          
          for (uint8_t i = 15; i < CharCount-1; i++) 
          {
            tmp *= 10;
            tmp += (tmpBuf[i] - '0');
          }
          FoundFiles[i].FlashAddress = tmp;
          Log("%s: %s = %lu\n",FoundFiles[i].FileName, &tmpBuf[15], FoundFiles[i].FlashAddress);
          break;                                                      // Entry complete, leave while loop
        }
      }
      CharCount++;
    }
  }
  return true;
}

bool FlashAttach(void)
{
  Send_CMD(CMD_FLASHATTACH);
  UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
  Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.

  uint8_t FlashStatus = rd8(REG_FLASH_STATUS + RAM_REG);
  if (FlashStatus != FLASH_STATUS_BASIC)
  {
//    Log("FlashAttach: NOT attached\n");
    return false;
  }
  return true;
}

bool FlashDetach(void)
{
  Send_CMD(CMD_FLASHDETACH);
  UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
  Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.

  uint8_t FlashStatus = rd8(REG_FLASH_STATUS + RAM_REG);
  if (FlashStatus != FLASH_STATUS_DETACHED)
  {
//    Log("FlashDetach: NOT detached\n");
    return false;
  }
  return true;
}

bool FlashFast(void)
{
  Cmd_Flash_Fast();
  UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
  Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.
  
  uint8_t FlashStatus = rd8(REG_FLASH_STATUS + RAM_REG);
  if (FlashStatus != FLASH_STATUS_FULL)
  {
//    Log("FlashFast: NOT full mode\n");
    return false;
  }
  return true;
}

bool FlashErase(void)
{
//  Log("FlashErase: Erasing Flash\n");
  Send_CMD(CMD_FLASHERASE);
  UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
  Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.
//  Log("FlashErase: Finished Erase\n");
}

// Load the output.bin file into flash if it exists
// Return indicates whether it was done or not
bool FlashLoad(void)
{
  // Check for the existence of the packed flash file (output.bin)
  if(!FileExists("output.bin"))
    return false; // File does not exist, so we do not need to load it into flash

  // Check for the existence of the completion flag file (transfer.fin)
  if(FileExists("transfer.fin"))
    return false; // File exists indicating that the transfer has already occurred in the past
  
  // upload file to flash
  if( !FileTransfer2Flash("output.bin", RAM_FLASH) )
  {
//    Log("FlashLoad: File Operation Failed\n");
    return false;
  }
  
  // Rename the file to "output.fin" <-- This is what you want, but Arduino has no
  // rename function.  The rename was essentially a "flag" to tell this system
  // whether the file transfer had already taken place or not.  I am going to
  // make a file with a specific name ("transfer.fin") to indicate that it's done.
  FileOpen("transfer.fin", FILEWRITE);
  if(!myFileIsOpen())
  {
//    Log("FlashLoad: No create file\n");
    FileClose();
    return false;
  }
  FileClose();
  
//  Log("output.bin automatically transferred to flash\n");
  return true;
}

// FileTransfer2Flash
// Transfer a file from SD card to Eve attached flash
// FlashAddress should be on a 4K boundary (multiple of 0x1000) - this is really important
bool FileTransfer2Flash(char *filename, uint32_t FlashAddress)
{
  uint8_t FlashStatus;
  uint32_t FlashSize;

  // check for file open - we can only have one file open - fail if already open
  if (myFileIsOpen())
  {
//    Log("FT2F: File already open\n");
    return false;
  }
    
  // Open file
  FileOpen(filename, FILEREAD);
  if(!myFileIsOpen())
  {
//    Log("FT2F: Open File Failed\n");
    FileClose();
    return false;
  }

  FlashAttach();
  FlashFast();
   
  // Read file size
  uint32_t Remaining = FileSize();
  uint32_t FileSectors = Remaining / 0x1000;                        // Number of 4K sectors required to store file

  if(Remaining % 0x1000)                                            // If the file is not on 4K boundary (and when is it?)
    FileSectors++;                                                  // Add a 4K sector for the partial
  
  // Calculate file fitting and number of 4K blocks
  FlashSize = rd32(REG_FLASH_SIZE + RAM_REG) * 0x100000;            // A flash of 8M bytes which will report size = 8
  uint32_t SectorsAvailable = (FlashSize - (FlashAddress - RAM_FLASH)) / 0x1000;    // Number of 4K sectors available in the flash chip from the requested base address
  if ( FileSectors > SectorsAvailable )
  {
//    Log("FT2F: File does not fit\n");
    return false;                                                   // We can not continue without flash space to store file
  }

  uint32_t BufPerSector = 0x1000 / COPYBUFSIZE;
  // Write file in sectors to RAM_G at the working RAM space (RAM_G_WORKING)
  for (uint16_t h = 0; h < FileSectors; h++ )                           // Loop through file data in 4K chunks until all 4K chunks have been processed
  {
    // Read file data COPYBUFSIZE at a time and write to RAM_G_WORKING space until 4K is reached
    // Each execution of this loop will stuff a 4K block into RAM_G
    for (uint16_t i = 0; i < BufPerSector; i++)
    {
      // Check each buffer load to see whether there is still more file than buffer
      // If there is enough data, then get the data into the buffer and subtract that amount from Remaining
      if(Remaining > COPYBUFSIZE)
      {
        FileReadBuf(LogBuf, COPYBUFSIZE);
        Remaining -= COPYBUFSIZE;
      }
      else
      {
        // There is not enough file data to fill buffer. Get what there is (if any) and fill the rest with ones
        // this should result in some number of manually padded buffer-loads being placed into RAM_G
        for ( uint8_t j = 0; j < COPYBUFSIZE; j++ )
        {
          if(Remaining)
          {
            LogBuf[j] = FileReadByte();
            Remaining--;
          }
          else
            LogBuf[j] = 0xFF;
        }
      }

      // Write the buffer to RAM_G
      WriteBlockRAM(RAM_G_WORKING + (i * COPYBUFSIZE), LogBuf, COPYBUFSIZE);
    }

    // Send the 4K buffer of data to flash via FIFO command
    Send_CMD(CMD_FLASHUPDATE);                                          // Transfer data from RAM_G into flash automatically erasing 4K sectors as required
    Send_CMD(FlashAddress + ((uint32_t)h * 0x1000));                    // Destination address in Flash 
    Send_CMD(RAM_G_WORKING);                                            // Source address in RAM_G
    Send_CMD(0x1000);                                                   // Number of bytes to transfer (4096)
    UpdateFIFO();                                                       // Trigger the CoProcessor to start processing commands out of the FIFO
    Wait4CoProFIFOEmpty();                                              // wait here until the coprocessor has read and executed every pending command.
  }

  FileClose(); // We are done with that file so close it
  return true;
}

