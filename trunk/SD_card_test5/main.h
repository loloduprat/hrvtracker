/*************************************************************************
 *
 *   Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2008
 *
 *    File name   : Main.h
 *    Description : 
 *
 *    History :
 *    1. Date        : 28.3.2008 
 *       Author      : Stoyan Choynev
 *       Description : 
 *
 *    $Revision: 22094 $
 **************************************************************************/
#ifndef   __MAIN_H
  #define __MAIN_H
/** include files **/

/**definitions **/
//Oscilator Frequency[Hz]
#define FOSC    14745600UL
//System Clock[Hz]
#define CCLK    58982400UL
//PLL MSEL
#define PLL_M_VAL 4
//PLL PSEL
#define PLL_P_VAL 2
//APB divider
#define APBDIV_VAL 0  //25% of system clock   (~15 MHz)
//Periferial Clock[Hz]
//#define PCLK    29491200UL   //Peripheral clock = CCLK / APBDIV_VAL 
//UART0 baud setting
#define UART0_BAUD 115200
#define UART1_BAUD 38400

/** default settings **/

/** public data **/

extern volatile unsigned short int timer1_counter;

/** public functions **/

unsigned int processorClockFrequency(void);
unsigned int peripheralClockFrequency(void);

#define GPS_BUFFER_SIZE 100  //max. 64 bytes expected in a GPS message...?


#endif //__MAIN_H