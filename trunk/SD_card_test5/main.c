/*************************************************************************
 *
 *   Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2008
 *
 *    File name   : main.c
 *    Description : This example project shows how to use the IAR Embedded Workbench for ARM
 *    to develop code for NXP LPC family microcontrollers. It is developed for 
 *    Olimex LPC-P2103 board.
 *
 *    This project implements sine modulated PWM using the Timer 2 module of the LPC2103
 *    microcontroller. The PWM period is 20kHz. Three sine wave singals with same frequancy
 *    but different phase(120) can be seen if simple Low Pass filters are connected to each of the outputs.
 *    The frequency and the level of the sine waves can be changed with commands from UART0
 *    The UART configuration is: 115200bps, 8-data bits, no parity(), 1 - stop bit (115200-8-N-1)
 *    Use "Help" Command for details
 *      
 *    The project also shows how the FIQ can be used. The PWM Interrup Service Routine
 *    is installed as FIQ and resides in the RAM for faster execution.
 *
 *    History :
 *    1. Date        : 28.3.2008 
 *       Author      : Stoyan Choynev
 *       Description : Initial Revision
 *
 *    $Revision: 22094 $
 **************************************************************************/

/** include files **/
#include <NXP\iolpc2103.h>
#include <intrinsics.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"     //system-wide options (processor speed/etc)
#include "interrupts.h"   //timers/interrupt functions
#include "uart.h"         //UART serial communication functions
#include "spi.h"          //SPI interface functions
#include "mmc.h"          //SD card read/write functions
#include "gps.h"          //GPS message parsing functions

/** local definitions **/

/** default settings **/


/** internal functions **/
//System initialization
void Init(void);

/** public data **/
volatile char led_on = 0;
volatile unsigned short int timer1_counter = 0;
volatile int led_counter = 0;

/** private data **/
//buffer for command and history
unsigned char SD_buffer[MMC_DATA_SIZE];
unsigned char GPS_buffer[GPS_BUFFER_SIZE];
int gps_msg_length = 0;
unsigned char ch0;
char gps_message_complete = 0;

/** external functions **/
__interwork extern void disk_timerproc (void);


void Timer1_init(void);
void Timer1_ISR(void);

unsigned int processorClockFrequency(void)
{
  //return real processor clock speed
  return FOSC * ( (PLLSTAT_bit.PLLE && PLLSTAT_bit.PLLC) ? (PLLCFG & 0xF) + 1 : 1);
}


unsigned int peripheralClockFrequency(void)
{
  //APBDIV - determines the relationship between the processor clock (cclk)
  //and the clock used by peripheral devices (pclk).
  unsigned int divider;
  switch (APBDIV & 3)
  {
    case 0: divider = 4;  break;
    case 1: divider = 1;  break;
    case 2: divider = 2;  break;
  }
  return processorClockFrequency() / divider;
}



void Timer1_init(void) {
      //Initialise the timer(s)
    //In this example, Timer1 is used
    T1TCR = 0x0;  //Stop the timer (in case it's already running)
    T1TC = 0x0;  //Reset timer counter to zero
    T1PR = 0x0;   //Set the prescale to zero (ie: when zero 
                  //the timer increments at the same rate as the *peripheral* clock, 1 would make timer 1/2 speed of processor)
    T1PC = 0x0;   //Reset prescale counter to zero
    
    //Set the prescale timer so millisecond values can be specified in the match register
    T1PR = peripheralClockFrequency()/1000;
    T1MR0 = 10;  //Interrupt generation time in ms
    T1MCR |= (1 << 0) | (1 << 1); //set the match control to reset the timer counter and generate an interrupt when timer value is reached
    
    Install_IRQ(VIC_TIMER1, Timer1_ISR, 5);
    
    //Initialise settings for the Vector Interrupt Controller    
 //   VICIntSelect = 0x0; //Set the timer in the IRQ interrupt category (not Fast interrupt)
 //   VICIntEnable = (1 << 5);  //Enable interrupts for Timer 1 (on bit 5 of this register)
 //   VICVectCntl0 = (5 << 0) | (1 << 5); //Allocate this vectored IRQ slot to interrupt #5 (timer1), and enable (bit 5)
 //   VICVectAddr0 = (unsigned long)IRQHandler; //assign the IRQHandler function as the callback for this timer  
}

void Timer1_ISR(void) {
  T1IR = (1<<0); 
   
  if (led_counter == 0) {
    if(led_on == 0) {
      led_on = 1;
    } else {
      led_on = 0;
    }
    led_counter = 50;
  } else {
    led_counter--;  
  }
}

void Uart1_ISR(void) {
  int dummy;
  dummy = U1IIR;    //read IIR to clear interrupt
  if(U1LSR_bit.DR == 1) {
    ch0 = U1RBR;
    //Log data from GPS into a buffer, and detect if end of a message has been received (denoted by '\n')
    if (gps_message_complete == 0 && gps_msg_length < GPS_BUFFER_SIZE) {
      GPS_buffer[gps_msg_length] = ch0;
      gps_msg_length++;
      if (ch0 == '\n') {
        gps_message_complete = 1;  
      }
    }
  }
}


/** private functions **/
//System initialization
void Init(void)
{
  //Disabel Memory Accelerator Module
  MAMCR_bit.MODECTRL = 0;
  //Set fetch cycles
#if CCLK < 20000000
  MAMTIM_bit.CYCLES = 1;
#elif CCLK < 40000000
  MAMTIM_bit.CYCLES = 2;
#else
  MAMTIM_bit.CYCLES = 3;
#endif
  //Here MAM can be enabled
  MAMCR_bit.MODECTRL = 0; //MAM is disabled
  //Disable PLL
  PLLCON = 0;
  // Write Feed
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  //Set MSEL and PSEL
  PLLCFG = PLL_M_VAL | (PLL_P_VAL<<5);  //PLL 14,7456*4 = 58,9824MHz
  // Write Feed
  PLLFEED = 0xaa;
  PLLFEED = 0x55;
  // Enable PLL, disconnected
  PLLCON = 1;       
  // Write Feed
  PLLFEED = 0xaa;
  PLLFEED = 0x55;
  //Set periferial clock
  APBDIV = APBDIV_VAL;
  //Wait PLL Lock
#ifndef SIM
  while(!PLLSTAT_bit.PLOCK);
#endif
  // connect PLL
  PLLCON = 3;       
  // Write Feed
  PLLFEED = 0xaa;
  PLLFEED = 0x55; 
}

/** public functions **/
void main(void)
{

  const char GPS_msg[] = {0xa0, 0xa1, 0x00, 0x09, 0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x0d, 0x0a};  
  __disable_interrupt();
  
  //System Init
  Init();
  //Init VIC
  VIC_Init();
  
  //Initialise the UART communications
  uart0Init(UART0_BAUD);
  uart1Init(UART1_BAUD);
  Install_IRQ(VIC_UART1, Uart1_ISR, 7);
  U1IER = 0x1;    //start interrupts when characters received on UART1
        
  //Initialise the timer
  uart0Puts("Initialising timer1...\r\n");
  Timer1_init();
  
  //Initialise the SPI interface
  uart0Puts("Initialising SPI...\r\n");
  spi_init();
  
  //Initialise the SD card interface
  uart0Puts("Initialising SD card...\r\n");
  initMMCSPI();
  if(initMMC() == 0) {
    uart0Puts("SD card found\r\n");
  } else {
     uart0Puts("SD card NOT found\r\n");
  }
  
  //Set the GPS receiver to only output RMC messages
  //Ref: Application note AN0003, "Binary messages of SkyTraq venus 6 GPS receiver"
  //Send this message down UART1 to the GPS card
  uart1Puts(GPS_msg);
  
  //Write some data to the MMC card
//  writeBlockMMC(10, SD_buffer);
  //Read some data from the MMC card
//  readBlockMMC(10, SD_buffer);
//  uart0Puts(SD_buffer);
  
   __enable_interrupt();
  //Enable Fast GPIO
  SCS = 1;
  //LED pin as ouput
  FIODIR |= (1<<26);
  FIOSET = (1<<26);
   // FIOCLR = (1<<26);
  
  //enable timer1
  T1TCR = 0x1;
  
  
  uart0Puts("Initialisation complete\r\n");
  
  while(1)
  {

   // if(U1LSR_bit.DR == 1) { //There is data waiting from the GPS to send to the PC
   //     while (U1LSR_bit.DR == 1) {
   //      
   //     }
   // }
    if(gps_message_complete) {
      //parse the GPS message
      if (parseGPS(GPS_buffer)) {
        if(GPS_Fix == 'A') {
          uart0Puts("GPS fix, valid position\r\n"); 
          FIOCLR = (1 << 26);
        } else {
          uart0Puts("No GPS fix\r\n");          
        }
      }
      //U0THR = GPS_Fix;
      //reset flags to indicate this message has been handled
      gps_msg_length = 0;
      gps_message_complete = 0;
      
    }
  }
  
}

