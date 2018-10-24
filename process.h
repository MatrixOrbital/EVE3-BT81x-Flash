#ifndef PROCESS_H
#define PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#define PressTimoutInterval     4000  // in mS
#define CheckTouchInterval        15  // in mS
#define CheckSwipeInterval        60  // in mS
#define ScreenUpdateInterval     200  // in mS
#define PressTimoutInterval     4000  // in mS

// The following typedef struct is not currently fully used
typedef struct {
  char     FileName[16];        // Filenames must be 8.3
  uint16_t SizeX; 
  uint16_t SizeY; 
  uint32_t Format;
  uint32_t FlashAddress;
}FileParms;

void MakeScreen_Bitmap(uint8_t FileIndex);

bool FileTransfer2Flash(char *filename, uint32_t FlashAddress);
bool FlashGetFileParms(void);
bool FlashAttach(void);
bool FlashDetach(void);
bool FlashFast(void);
bool FlashLoad(void);
bool FlashErase(void);
bool FlashLoad(void);

#ifdef __cplusplus
}
#endif

#endif
