//-----------------------------------------------------------------------------
// MMC.H                                                           20060407 CHG
//-----------------------------------------------------------------------------
// Simple read/write interface to MMC/SD cards. 
// Uses either SPI0 or SPI1 (SSP) on a Philips LPC2xxx ARM processor
//-----------------------------------------------------------------------------
#ifndef _MMC_H_
#define _MMC_H_


#define IDLE_STATE_TIMEOUT            1
#define OP_COND_TIMEOUT               2
#define SET_BLOCKLEN_TIMEOUT          3
#define WRITE_BLOCK_TIMEOUT           4
#define WRITE_BLOCK_FAIL              5
#define READ_BLOCK_TIMEOUT            6
#define READ_BLOCK_DATA_TOKEN_MISSING 7

#define MMC_CMD_SIZE 6    // The SPI data is 8 bit long, the MMC use 48 bits, 6 bytes
#define MMC_DATA_SIZE 512 // 512 bytes per block (sector)

#define MAX_TIMEOUT                0xFF // just a raw number used for some of the timeout checks..


//-----------------------------------------------------------------------------
// Define I/O pins for the interface
//-----------------------------------------------------------------------------
#define IO_DIR IODIR // The GPIO registers used for accessing the CS pin to the SD Card
#define IO_SET IOSET
#define IO_CLR IOCLR

//Olimex LPC2103 board
#define CS (1<<7)  // P0.7 is CS

// LPC2148 Small board on prototype board from Embedded Artists
//#define CS (1<<22)  // P0.22 is CS

// Keil MCB2140
//#define CS (1<<20)  // P0.20 is CS



//-----------------------------------------------------------------------------
// Define which SPI port to use
//-----------------------------------------------------------------------------
#define SPI0  // Remove this to use SPI1 (SSP) port instead


//------------------------------------------------------------------------------
// Reads a 512 Byte block from the MMC
// Send READ_SINGLE_BLOCK command first, wait for response 0x00 followed by 0xFE. 
// Then call receiveBlockMMCSPI() to read the data block back followed by the checksum.
//------------------------------------------------------------------------------
int readBlockMMC(unsigned int blocknum, unsigned char *Buffer);

//------------------------------------------------------------------------------
// write a block of data based on the length that has been set
// in the SET_BLOCKLEN command.
// Send the WRITE_SINGLE_BLOCK command out first, check the
// R1 response, then send the data start token(bit 0 to 0) followed by
// the block of data. When the data write finishs, the response should come back
// as 0xX5 bit 3 to 0 as 0101B, then another non-zero value indicating
// that MMC card is in idle state again.
//------------------------------------------------------------------------------
int writeBlockMMC(unsigned int blocknum, unsigned char *Buffer);

//-----------------------------------------------------------------------------
// initMMCSPI()
// Initializes the SPI port for the SD/MMC card
// This function must be called before the initMMC() is called
//-----------------------------------------------------------------------------
void initMMCSPI(void);

//------------------------------------------------------------------------------
// Initialises the MMC into SPI mode and sets block size(512), 
// returns 0 on success (card found and initialized)
//------------------------------------------------------------------------------
int initMMC(void);

#endif

