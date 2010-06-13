//------------------------------------------------------------------------------
// MMC.C                                                            20060406 CHG
//------------------------------------------------------------------------------
// Simple read/write interface to MMC/SD cards. 
// Uses either SPI0 or SPI1 (SSP) on a Philips LPC2xxx ARM processor
//------------------------------------------------------------------------------
#include <iolpc2103.h>
#include "mmc.h"
#include "stdio.h"
#include "uart.h"


// Returncodes not currently used/implemented
//#define DATA_TOKEN_TIMEOUT         8
//#define SELECT_CARD_TIMEOUT        9
//#define SET_RELATIVE_ADDR_TIMEOUT 10


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Basic SPI functions to transport data to/from the card
// 
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//------------------------------------------------------------------------------
// Send a buffer to the card
//------------------------------------------------------------------------------
static void sendMMCSPI(unsigned char *buf, unsigned int Length) {
  unsigned char Dummy;
  if ( Length == 0 )
    return;
#ifdef SPI0  
  while ( Length != 0 ) {
    S0SPDR = *buf;
    while ( !(S0SPSR & 0x80) );
    Length--;
    buf++;
  }
  Dummy=S0SPDR;
#else
  while ( Length != 0 ) {
    // as long as TNF bit is set, TxFIFO is not full, I can write
    while (!(SSPSR & 0x02));
    SSPDR = *buf;
    // Wait until the Busy bit is cleared
    while (!(SSPSR & 0x04));
    Dummy = SSPDR; // Flush the RxFIFO
    Length--;
    buf++;
  }
#endif
  return;
}


//------------------------------------------------------------------------------
// SPI Receive Byte, receive one byte only, return Data byte 
// (used a lot to check the status from the card)
//------------------------------------------------------------------------------
static unsigned char receiveMMCSPI( void ) {
  unsigned char data;
#ifdef SPI0
  // write dummy byte out to generate clock, then read data from MISO
  S0SPDR = 0xFF;
  // Wait until the Busy bit is cleared
  while (!(S0SPSR & 0x80));
  // Grab the received data
  data = S0SPDR;
#else
  // write dummy byte out to generate clock, then read data from MISO
  SSPDR = 0xFF;
  // Wait until the Busy bit is cleared
  while (SSPSR & 0x10);
  // Grab the received data
  data = SSPDR;
#endif
  return (data);
}


//------------------------------------------------------------------------------
// SPI Receive, receive a number of bytes
//------------------------------------------------------------------------------
static void receiveBlockMMCSPI(unsigned char *buf, unsigned int Length) {
  unsigned int i;
  for (i = 0; i < Length; i++) {
    *buf = receiveMMCSPI();
    buf++;
  }
  return;
}


//-----------------------------------------------------------------------------
// initMMCSPI()
// Initializes the SPI port for the SD/MMC card
// This function must be called before the initMMC() is called
//-----------------------------------------------------------------------------
void initMMCSPI(void ) {
  unsigned char i, Dummy;
  // Configure PIN connect block */
  // SSEL0/1 is NOT set to SSEL0/1, so enable/disable of the card is total manually done !
  
#ifdef SPI0  
  // Using SPI0 which has no FIFO
  PINSEL0|= 0x1500;     //Enable SPI0 pins (except SSEL)
  IO_DIR |= CS;  
  IO_SET  = CS;

  S0SPCR  =	0x00000020;	// Configure as SPI Master 
  S0SPCCR	=	0x00000008;	// Set bit timing
  Dummy   = S0SPDR;     // clear the RxFIFO
#else
  // Using SPI1 (SSP port) which has a 8 byte FIFO
  SSPCR1     = 0x00;      // SSP master (off) in normal mode */
  PINSEL1   |= 0x00A8;    // Enable SPI1 pins (except SSEL)
  IO_DIR |= CS;  
  IO_SET  = CS;
  // Set PCLK 1/2 of CCLK
  VPBDIV = 0x02;
  // Set data to 8-bit, Frame format SPI, CPOL = 0, CPHA = 0, and SCR is 15
  SSPCR0 = 0x0707;
  // SSPCPSR clock prescale register, master mode, minimum divisor is 0x02
  SSPCPSR = 0x2;
  // Device select as master, SSP Enabled, normal operational mode
  SSPCR1 = 0x02;
  // clear the RxFIFO 
  for ( i = 0; i < 8; i++ ) Dummy = SSPDR; 
#endif
  return;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// SD/MMC functions
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


//------------------------------------------------------------------------------
// Repeatedly reads the MMC until we get the response we want or timeout
//------------------------------------------------------------------------------
static int mmc_response( unsigned char response) {
  unsigned int count = 0xFFF;
  while((receiveMMCSPI() != response) && count) count--;
  if (count == 0)
    return 1; // Failure, loop was exited due to timeout
  else
    return 0; // Normal, loop was exited before timeout
}


//------------------------------------------------------------------------------
// Repeatedly reads the MMC until we get a non-zero value (after
// a zero value) indicating the write has finished and card is no
// longer busy.
//------------------------------------------------------------------------------
static int mmc_wait_for_write_finish(void) {
  unsigned int count = 0xFFFF; // The delay is set to maximum considering the longest data block length to handle
  unsigned char result = 0;
  while((result == 0) && count) {
    result = receiveMMCSPI();
    count--;
  }
  if (count == 0)
    return 1; // Failure, loop was exited due to timeout
  else
    return 0; // Normal, loop was exited before timeout
}


//------------------------------------------------------------------------------
// write a block of data based on the length that has been set
// in the SET_BLOCKLEN command.
// Send the WRITE_SINGLE_BLOCK command out first, check the
// R1 response, then send the data start token(bit 0 to 0) followed by
// the block of data. When the data write finishs, the response should come back
// as 0xX5 bit 3 to 0 as 0101B, then another non-zero value indicating
// that MMC card is in idle state again.
//------------------------------------------------------------------------------
int writeBlockMMC(unsigned int blocknum, unsigned char *Buffer) {
  unsigned char Status;
 	unsigned char CMD[] = {0x58,0x00,0x00,0x00,0x00,0xFF}; 
  unsigned char MMCStatus = 0;

  IO_CLR  = CS;
  // As we need to send a 32 bit address, we adjust the block number to 512 bytes alignment
  // and uses that as the address..
  blocknum <<= 9; 
	CMD[1] = ((blocknum & 0xFF000000) >>24 );
	CMD[2] = ((blocknum & 0x00FF0000) >>16 );
	CMD[3] = ((blocknum & 0x0000FF00) >>8 );
   uart0Puts("Sending MMC write single block...\r\n");
  // send mmc CMD24(WRITE_SINGLE_BLOCK) to write the data to MMC card
  sendMMCSPI(CMD, MMC_CMD_SIZE );
  // if mmc_response returns 1 then we failed to get a 0x00 response
  if((mmc_response(0x00))==1) {
    uart0Puts("MMC write timeout\r\n");
    MMCStatus = WRITE_BLOCK_TIMEOUT;
    IO_SET  = CS;
    return MMCStatus;
  }
  uart0Puts("Sending data...\r\n");
  // Set bit 0 to 0 which indicates the beginning of the data block
  CMD[0] = 0xFE;
  sendMMCSPI(CMD, 1);
  // send data
  sendMMCSPI(Buffer, MMC_DATA_SIZE );
  //Send dummy checksum
  // when the last check sum is sent, the response should come back
  // immediately. So, check the SPI FIFO MISO and make sure the status
  // return 0xX5, the bit 3 through 0 should be 0x05
  uart0Puts("Checking write success...\r\n");
  CMD[0] = 0xFF;
  CMD[1] = 0xFF;
  sendMMCSPI( CMD, 2 );
  Status = receiveMMCSPI();
  if ((Status & 0x0F) != 0x05) {
    uart0Puts("Write failed\r\n");
    MMCStatus = WRITE_BLOCK_FAIL;
    IO_SET  = CS;
    return MMCStatus;
  }

  // if the status is already zero, the write hasn't finished yet and card is busy
  if(mmc_wait_for_write_finish()==1) {
    MMCStatus = WRITE_BLOCK_FAIL;
    IO_SET  = CS;
    return MMCStatus;
  }
  
  IO_SET  = CS;
  receiveMMCSPI();
  return 0;
}


//------------------------------------------------------------------------------
// Reads a 512 Byte block from the MMC
// Send READ_SINGLE_BLOCK command first, wait for response 0x00 followed by 0xFE. 
// Then call receiveBlockMMCSPI() to read the data block back followed by the checksum.
//------------------------------------------------------------------------------
int readBlockMMC(unsigned int blocknum, unsigned char *Buffer) {
	unsigned char CMD[] = {0x51,0x00,0x00,0x00,0x00,0xFF}; 
  unsigned char MMCStatus = 0;

  IO_CLR  = CS;
  // As we need to send a 32 bit address, we adjust the block number to 512 bytes alignment
  // and uses that as the address..
  blocknum <<= 9;
	CMD[1] = ((blocknum & 0xFF000000) >>24 );
	CMD[2] = ((blocknum & 0x00FF0000) >>16 );
	CMD[3] = ((blocknum & 0x0000FF00) >>8 );
  if (CMD[1]==0x20) {
//    printf("Hit, CMD[1]=%i\n", CMD[1]);
  }
  
  // send MMC CMD17(READ_SINGLE_BLOCK) to read the data from MMC card
  sendMMCSPI(CMD, MMC_CMD_SIZE );
  // if mmc_response returns 1 then we failed to get a 0x00 response
  if((mmc_response(0x00))==1) {
    MMCStatus = READ_BLOCK_TIMEOUT;
    IO_SET  = CS;
    return MMCStatus;
  }
  // wait for data token
  if((mmc_response(0xFE))==1) {
    MMCStatus = READ_BLOCK_DATA_TOKEN_MISSING;
    IO_SET  = CS;
 //   printf("%i::readBlockMMC, blocknum=%i, CMD[1]=%i, CMD[2]=%i, CMD[3]=%i\n", __LINE__, blocknum, CMD[1],CMD[2],CMD[3]);
    return MMCStatus;
  }

  // Get the block of data based on the length
  receiveBlockMMCSPI(Buffer, MMC_DATA_SIZE);
  // CRC bytes that are not needed
  receiveMMCSPI();
  receiveMMCSPI();
  IO_SET  = CS;
  receiveMMCSPI();
  return 0;
}



//------------------------------------------------------------------------------
// Initialises the MMC into SPI mode and sets block size(512), 
// returns 0 on success (card found and initialized)
//------------------------------------------------------------------------------
int initMMC() {
	unsigned char CMD[] = {0x40,0x00,0x00,0x00,0x00,0x95};
  unsigned int i;
  unsigned char dummy=0xFF;
  unsigned char MMCStatus = 0;

  uart0Puts("Start MMC init...\r\n");
  
  IO_SET  = CS;
  // initialise the MMC card into SPI mode by sending 80 clks
  for(i=0; i<10; i++) sendMMCSPI( &dummy, 1 );
  IO_CLR  = CS;

  uart0Puts("Reset MMC...\r\n");
  
  // send CMD0(RESET or GO_IDLE_STATE) command, all the arguments are 0x00 for the reset command, precalculated checksum
  sendMMCSPI( CMD, MMC_CMD_SIZE );
  // if = 1 then there was a timeout waiting for 0x01 from the MMC
  if(mmc_response(0x01) == 1) {
      uart0Puts("Timeout Reset MMC...\r\n");
    MMCStatus = IDLE_STATE_TIMEOUT;
    IO_SET  = CS;
    return MMCStatus;
  }

  uart0Puts("Waiting for 0x00 from MMC...\r\n");  
  // Send some dummy clocks after GO_IDLE_STATE
  IO_SET  = CS;
  receiveMMCSPI();

  IO_CLR  = CS;
  // must keep sending command until zero response
  i = MAX_TIMEOUT;
  do {
    // send mmc CMD1(SEND_OP_COND) to bring out of idle state
    // all the arguments are 0x00 for command one
  	CMD[0] = 0x41;
	  CMD[5] = 0xFF;
    sendMMCSPI( CMD, MMC_CMD_SIZE );
    i--;
  } while ((mmc_response(0x00) != 0) && (i>0));
  // timeout waiting for 0x00 from the MMC
  if (i == 0) {
    uart0Puts("ERROR: Timeout when waiting for MMC...\r\n");
    MMCStatus = OP_COND_TIMEOUT;
    IO_SET  = CS;
    return MMCStatus;
  }
  // Send some dummy clocks after SEND_OP_COND
  IO_SET  = CS;
  receiveMMCSPI();

  IO_CLR  = CS;
 
  
  // send MMC CMD16(SET_BLOCKLEN) to set the block length
  CMD[0] = 0x50;
  CMD[1] = 0x00; // 4 bytes from here is the block length
  // LSB is first
  // 00 00 00 10 set to 16 bytes
  // 00 00 02 00 set to 512 bytes
  CMD[2] = 0x00;
  // high block length bits - 512 bytes
  CMD[3] = 0x02;
  // low block length bits
  CMD[4] = 0x00;
  // checksum is no longer required but we always send 0xFF
  CMD[5] = 0xFF;

  uart0Puts("Setting blocklength...\r\n");
  
  sendMMCSPI(CMD, MMC_CMD_SIZE);
  if((mmc_response(0x00))==1) {
      uart0Puts("ERROR in set blocklength...\r\n");
    MMCStatus = SET_BLOCKLEN_TIMEOUT;
    IO_SET  = CS;
    return MMCStatus;
  }
  IO_SET  = CS;
  receiveMMCSPI();
  return 0;
}

