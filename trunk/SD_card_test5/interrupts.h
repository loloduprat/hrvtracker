/*************************************************************************
 *
 *   Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2008
 *
 *    File name   : interrupts.h
 *    Description : inerrupts header file
 *
 *    History :
 *    1. Date        : 28.3.2008 ã. 
 *       Author      : Stoyan Choynev
 *       Description : initial version
 *
 *    $Revision: 22094 $
 **************************************************************************/
#ifndef   __INTERRUPTS_H
  #define __INTERRUPTS_H
/** include files **/

/**definitions **/

/** default settings **/

/** public data **/

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
void VIC_Init(void);
/*************************************************************************
 * Function Name: Instal_IRQ
 * Parameters:Interrupt Number, ISR pionter, VIC channel
 *
 * Return: None
 *
 * Description: Installs Interrup Serice Routine at selected VIC channel 
 *              Enable interrupt. This function can be use for enabling
 *              a default interrupt if invoke it with channel >= VIC_CHANNELS
 *************************************************************************/
void Install_IRQ(unsigned int IntNumber,  void (*ISR)(void), unsigned int channel);
/*************************************************************************
 * Function Name: Install_FIQ
 * Parameters: Interrupt Number
 *
 * Return:  None
 *
 * Description: Sets Interrup in FIQ mode and enables it
 *
 *************************************************************************/
void Install_FIQ(unsigned int IntNumber,  void (*ISR)(void));

#endif //__INTERRUPTS_H
