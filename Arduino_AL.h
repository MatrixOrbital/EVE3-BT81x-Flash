// This file acts as a header for the .ino file.  Since the .ino file is the hardware abstraction layer
// in Arduino world, it also contains prototypes for abstracted hardware calls in addition to prototypes
// for abstracted time functions which in other compilers would just be other .c and .h files

#ifndef __ARDUINOAL_H
#define __ARDUINOAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>              // Find integer types like "uint8_t"  
#include <stdbool.h>             // Find type "bool"

// defines related to hardware and relevant only to the hardware abstraction layer (this and .ino files)
#define EveChipSelect_PIN          9  // PB1
#define EveAudioEnable_PIN         1  // PD1
#define EvePDN_PIN                10  // PB2
#define SDChipSelect_PIN           3  // PD3
#define SDCardDetect_PIN           4  // PD4
#define OneWire_PIN                5  // PD5
#define ControlOutput_PIN          8  // PB0

#define SPISpeed            10000000

// Notes:
// In Arduino we lose access to these defines from outside the .ino, so they are redfined here.
// In order to prevent mysteries, these are defining these with hopefully unique names.
#define FILEREAD   0      
#define FILEWRITE  1 
#define FILEAPPEND 2

#define WorkBuffSz 64UL
extern char LogBuf[WorkBuffSz];         // The singular universal data array used for all things including logging

#define Log(...)  { sprintf(LogBuf,__VA_ARGS__); DebugPrint(LogBuf); } // Stuff string and parms via sprintf and output
// #define Log(...) // Liberate (a lot of) RAM by uncommenting this empty definition (remove all serial logging)

void MainLoop(void);
void GlobalInit(void);

// Hardware peripheral abstraction function prototypes
uint8_t ReadPin(uint8_t);
void SetPin(uint8_t, bool);
void SD_Init(void);
void SPI_Enable(void);
void SPI_Disable(void);
void SPI_Write(uint8_t data);
void SPI_WriteByte(uint8_t data);
void SPI_WriteBuffer(uint8_t *Buffer, uint32_t Length);
void SPI_ReadBuffer(uint8_t *Buffer, uint32_t Length);

// These functions encapsulate Arduino library functions
void DebugPrint(char *str);
void MyDelay(uint32_t DLY);
uint32_t MyMillis(void);
void SaveTouchMatrix(void);
bool LoadTouchMatrix(void);
void Eve_Reset_HW(void);

// Function encapsulation for file operations
bool FileExists(char *filename);
void FileOpen(char *filename, uint8_t mode);
void FileClose(void);
uint8_t FileReadByte(void);
void FileReadBuf(uint8_t *data, uint32_t NumBytes);
void FileWrite(uint8_t data);
void FileWriteStr(uint8_t *str, uint16_t MaxChars);
uint32_t FileSize(void);
uint32_t FilePosition(void);
bool FileSeek(uint32_t offset);
bool myFileIsOpen(void);

#ifdef __cplusplus
}
#endif

#endif
