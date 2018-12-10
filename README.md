This code contains functions to perform the following operations:

- Read a file off SD card and write it to Eve connected flash.
- Parse Eve flash and store file offsets - flash addresses - of files.
- An example of displaying bitmaps (ASTC format) directly out of flash.

To be used in conjuction with Eve Asset Builder software from Bridgetek.

Eve Asset Builder will take any number of files and pack them into a single file.  This file will also contain 
the "blob" file provided by Bridgetek which allows the Eve BT81x to use fast mode (QSPI) in it's interactions
with the onboard flash chip.

In order for this code to function, the location of the file list and offsets (output.map) must be known.  
In order to place this offset table at a known address (offset 4096), the following procedure is provided:

Eve Asset Builder provides no method of ordering files within the "blob" so some dance must be performed.
1) select your converted media files and run "Generate Flash".  
2) rename output.map as aaoutput.map
3) Add the same files to "Generate Flash" as well as aaoutput.map and generate flash again.
  -- This includes the old map file at the first file location in flash - offset 4096
  -- The included map file does not include itself and so all offsets are wrong.
4) Delete aaoutput.map and rename output.map to aaoutput.map
5) Include the same files again including aaoutput.map and generate flash a third time.
  -- Now, the file aaoutput.map will be found at 4096 and that file now includes itself with correct offsets.

- Designed for Matrix Orbital EVE2 SPI TFT Displays incorporating BT81x chips and QSPI flash

  https://www.matrixorbital.com/ftdi-eve/eve-bt815

- This code makes use of the Matrix Orbital EVE2 Library found here: 

  https://github.com/MatrixOrbital/EVE2-Library

  - While a copy of the library files (Eve2_81x.c and Eve2_81x.h) is included here, you may look for updated
    files if you wish.  

- Matrix Orbital EVE2 SPI TFT display information can be found at: https://www.matrixorbital.com/ftdi-eve

- An Arduino shield with a connector for Matrix Orbital EVE2 displays is used to interface the Arduino to Eve.  
  This shield includes:
  - 20 contact FFC connector for Matrix Orbital EVE2 displays
  - 3 pushbuttons for application control without requiring a touchscreen (useful for initial calibration)
  - Audio amplifier and speaker for audio feedback
  - SD card holder
  - Additionally, the shield board is automatically level shifted for 5V Arduino and works with 3.3V Parallax Propeller ASC+ 
  
  https://www.matrixorbital.com/accessories/interface-module/eve2-shield
  
  Support Forums
  
  http://www.lcdforums.com/forums/viewforum.php?f=45
