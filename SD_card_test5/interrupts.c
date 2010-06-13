/*************************************************************************
 *
 *   Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2008
 *
 *    File name   : interrupts.c
 *    Description : Include interrupt init, install and handler routines
 *
 *    History :
 *    1. Date        : 28.3.2008 
 *       Author      : Stoyan Choynev
 *       Description : 
 *
 *    $Revision: 22094 $
 **************************************************************************/

/** include files **/
#include <nxp/iolpc2103.h>
/** local definitions **/
//Number of Interrupt Sources
#define INT_NUMBERS   32
//IRQ priority levesl
#define VIC_CHANNELS  16
/** default settings **/

/** external functions **/

/** external data **/

/** internal functions **/
void DefVectISR (void);
/** public data **/

/** private data **/
static void (* fiq_isr)(void);
/** public functions **/
/*************************************************************************
 * Function Name: VIC_Init
 *
 * Parameters: None
 *
 * Return:  None
 *
 * Description: Inits the Vectiored Interrupt Controler
 *              Clears and Disables all interrupts. All interrupts are set
 *              to IRQ mode. 
 *
 *************************************************************************/
void VIC_Init(void)
{
unsigned int * VectAddr = (unsigned int *)&VICVectAddr0;
unsigned int * VectCntl = (unsigned int *)&VICVectCntl0;

  //Disable all interrupts
  VICIntEnClear = 0xFFFFFFFF;
  //Clear Software Interrupts
  VICSoftIntClear = 0xFFFFFFFF;
  //Write to VicVectAddr register
  VICVectAddr = 0;
  //Set all to IRQ
  VICIntSelect = 0;
  //Set Default Vector Address to NULL
  VICDefVectAddr = 0;
  //Set all the vector addresses to NULL
  //Disable all IRQ channels
  for ( unsigned int i = 0; i < VIC_CHANNELS; i++ )
  {
    VectAddr[i] = 0x0;
    VectCntl[i] = 0x0;
  }
}
/*************************************************************************
 * Function Name: Instal_IRQ
 * Parameters:Interrupt Number, ISR pionter, VIC channel
 *
 * Return: None
 *
 * Description: Installs Interrup Serice Routine at selected VIC channel 
 *              Enables the interrupt. This function can be used for enabling
 *              a default interrupt if it is called with channel >= VIC_CHANNELS
 *************************************************************************/
void Install_IRQ(unsigned int IntNumber, void (*ISR)(void), unsigned int channel)
{
unsigned int * VectAddr = (unsigned int *)&VICVectAddr0;
unsigned int * VectCntl = (unsigned int *)&VICVectCntl0;

#ifdef DEBUG
  if (INT_NUMBERS < IntNumber)
  {//Wrong Int Number
    while(1);
  }
#endif
  //Disable Interrupt
  VICIntEnClear = 1<<IntNumber;
  
  if(VIC_CHANNELS > channel)
  {//Vectired IRQ
    //Set interrupt Vector
    VectAddr[channel] = (unsigned int)ISR;
    //Set Int Number and enable the channel
    VectCntl[channel] = IntNumber | (1<<5);
  }
  else
  {//Non-vectired IRQ
    //Install ISR for non vectored IRQ
    VICDefVectAddr = (unsigned int)DefVectISR ;
  }
  //Enable Interrupt
  VICIntEnable = 1 << IntNumber;
}
/*************************************************************************
 * Function Name: Install_FIQ
 * Parameters: Interrupt Number
 *
 * Return:  None
 *
 * Description: Sets Interrup in FIQ mode and enables it
 *
 *************************************************************************/
void Install_FIQ(unsigned int IntNumber,   void (*ISR)(void))
{
  //Disable Interrupt
  VICIntEnClear = 1<<IntNumber;
  //Set FIQ mode
  VICIntSelect |= 1<<IntNumber;
  //Set interrupt Vector
  fiq_isr = ISR;
  //Enable Interrupt
  VICIntEnable = 1 << IntNumber;
}
/*************************************************************************
 * Function Name: IRQ_Handler
 * Parameters:None
 *
 * Return:None
 *
 * Description:The IRQ Handler
 *
 *************************************************************************/
__irq __arm void IRQ_Handler (void)
{
void (* IntVector)(void);

  IntVector = (void (*)(void)) VICVectAddr;    //Read Interrup Vector
  (* IntVector)();                             //Call ISR
  
  VICVectAddr = 0;                 //Dummy write to Vector address register
}
/*************************************************************************
 * Function Name: FIQ_Handler
 * Parameters:None
 *
 * Return:None
 *
 * Description:The FIQ Handler
 *
 *************************************************************************/
__fiq __arm void FIQ_Handler(void)
{
  (* fiq_isr)();                             //Call ISR
}
/*************************************************************************
 * Function Name: DefVectISR
 * Parameters:None
 *
 * Return:None
 *
 * Description:The Non-vectored IRQ ISR
 *
 *************************************************************************/
/** private functions **/
static void DefVectISR (void)
{//Put Code of NON vectored ISR here
  return;
}

