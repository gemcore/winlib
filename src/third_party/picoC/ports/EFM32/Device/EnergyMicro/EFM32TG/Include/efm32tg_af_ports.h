/**************************************************************************//**
 * @file
 * @brief efm32tg_af_ports Register and Bit Field definitions
 * @author Energy Micro AS
 * @version 3.0.2
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2012 Energy Micro AS, http://www.energymicro.com</b>
 ******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 *****************************************************************************/
/**************************************************************************//**
 * @defgroup EFM32TG_AF_Ports
 * @{
 *****************************************************************************/

/** AF port for function f */
#define AF_CMU_CLK0_PORT(f)          ((f) == 0 ? 0 : (f) == 1 ? 2 : (f) == 2 ? 3 :  -1)
#define AF_CMU_CLK1_PORT(f)          ((f) == 0 ? 0 : (f) == 1 ? 3 : (f) == 2 ? 4 :  -1)
#define AF_LESENSE_CH0_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH1_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH2_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH3_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH4_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH5_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH6_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH7_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH8_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH9_PORT(f)       ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH10_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH11_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH12_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH13_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH14_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_CH15_PORT(f)      ((f) == 0 ? 2 :  -1)
#define AF_LESENSE_ALTEX0_PORT(f)    ((f) == 0 ? 3 :  -1)
#define AF_LESENSE_ALTEX1_PORT(f)    ((f) == 0 ? 3 :  -1)
#define AF_LESENSE_ALTEX2_PORT(f)    ((f) == 0 ? 0 :  -1)
#define AF_LESENSE_ALTEX3_PORT(f)    ((f) == 0 ? 0 :  -1)
#define AF_LESENSE_ALTEX4_PORT(f)    ((f) == 0 ? 0 :  -1)
#define AF_LESENSE_ALTEX5_PORT(f)    ((f) == 0 ? 4 :  -1)
#define AF_LESENSE_ALTEX6_PORT(f)    ((f) == 0 ? 4 :  -1)
#define AF_LESENSE_ALTEX7_PORT(f)    ((f) == 0 ? 4 :  -1)
#define AF_PRS_CH0_PORT(f)           ((f) == 0 ? 0 : (f) == 1 ? 5 :  -1)
#define AF_PRS_CH1_PORT(f)           ((f) == 0 ? 0 : (f) == 1 ? 5 :  -1)
#define AF_PRS_CH2_PORT(f)           ((f) == 0 ? 2 : (f) == 1 ? 5 :  -1)
#define AF_PRS_CH3_PORT(f)           ((f) == 0 ? 2 : (f) == 1 ? 4 :  -1)
#define AF_TIMER0_CC0_PORT(f)        ((f) == 0 ? 0 : (f) == 1 ? 0 : (f) == 2 ?  : (f) == 3 ? 3 : (f) == 4 ? 0 : (f) == 5 ? 5 :  -1)
#define AF_TIMER0_CC1_PORT(f)        ((f) == 0 ? 0 : (f) == 1 ? 0 : (f) == 2 ?  : (f) == 3 ? 3 : (f) == 4 ? 2 : (f) == 5 ? 5 :  -1)
#define AF_TIMER0_CC2_PORT(f)        ((f) == 0 ? 0 : (f) == 1 ? 0 : (f) == 2 ?  : (f) == 3 ? 3 : (f) == 4 ? 2 : (f) == 5 ? 5 :  -1)
#define AF_TIMER0_CDTI0_PORT(f)      (-1)
#define AF_TIMER0_CDTI1_PORT(f)      (-1)
#define AF_TIMER0_CDTI2_PORT(f)      (-1)
#define AF_TIMER1_CC0_PORT(f)        ((f) == 0 ? 2 : (f) == 1 ? 4 : (f) == 2 ?  : (f) == 3 ? 1 : (f) == 4 ? 3 :  -1)
#define AF_TIMER1_CC1_PORT(f)        ((f) == 0 ? 2 : (f) == 1 ? 4 : (f) == 2 ?  : (f) == 3 ? 1 : (f) == 4 ? 3 :  -1)
#define AF_TIMER1_CC2_PORT(f)        ((f) == 0 ? 2 : (f) == 1 ? 4 : (f) == 2 ?  : (f) == 3 ? 1 : (f) == 4 ? 2 :  -1)
#define AF_TIMER1_CDTI0_PORT(f)      (-1)
#define AF_TIMER1_CDTI1_PORT(f)      (-1)
#define AF_TIMER1_CDTI2_PORT(f)      (-1)
#define AF_USART0_TX_PORT(f)         ((f) == 0 ? 4 : (f) == 1 ? 4 : (f) == 2 ? 2 : (f) == 3 ? 4 : (f) == 4 ? 1 : (f) == 5 ? 2 :  -1)
#define AF_USART0_RX_PORT(f)         ((f) == 0 ? 4 : (f) == 1 ? 4 : (f) == 2 ? 2 : (f) == 3 ? 4 : (f) == 4 ? 1 : (f) == 5 ? 2 :  -1)
#define AF_USART0_CLK_PORT(f)        ((f) == 0 ? 4 : (f) == 1 ? 4 : (f) == 2 ? 2 : (f) == 3 ? 2 : (f) == 4 ? 1 : (f) == 5 ? 1 :  -1)
#define AF_USART0_CS_PORT(f)         ((f) == 0 ? 4 : (f) == 1 ? 4 : (f) == 2 ? 2 : (f) == 3 ? 2 : (f) == 4 ? 1 : (f) == 5 ? 1 :  -1)
#define AF_USART1_TX_PORT(f)         ((f) == 0 ? 2 : (f) == 1 ? 3 : (f) == 2 ? 3 :  -1)
#define AF_USART1_RX_PORT(f)         ((f) == 0 ? 2 : (f) == 1 ? 3 : (f) == 2 ? 3 :  -1)
#define AF_USART1_CLK_PORT(f)        ((f) == 0 ? 1 : (f) == 1 ? 3 : (f) == 2 ? 5 :  -1)
#define AF_USART1_CS_PORT(f)         ((f) == 0 ? 1 : (f) == 1 ? 3 : (f) == 2 ? 5 :  -1)
#define AF_LEUART0_TX_PORT(f)        ((f) == 0 ? 3 : (f) == 1 ? 1 : (f) == 2 ? 4 : (f) == 3 ? 5 : (f) == 4 ? 5 :  -1)
#define AF_LEUART0_RX_PORT(f)        ((f) == 0 ? 3 : (f) == 1 ? 1 : (f) == 2 ? 4 : (f) == 3 ? 5 : (f) == 4 ? 0 :  -1)
#define AF_LETIMER0_OUT0_PORT(f)     ((f) == 0 ? 3 : (f) == 1 ? 1 : (f) == 2 ? 5 : (f) == 3 ? 2 :  -1)
#define AF_LETIMER0_OUT1_PORT(f)     ((f) == 0 ? 3 : (f) == 1 ? 1 : (f) == 2 ? 5 : (f) == 3 ? 2 :  -1)
#define AF_PCNT0_S0IN_PORT(f)        ((f) == 0 ? 2 : (f) == 1 ?  : (f) == 2 ? 2 : (f) == 3 ? 3 :  -1)
#define AF_PCNT0_S1IN_PORT(f)        ((f) == 0 ? 2 : (f) == 1 ?  : (f) == 2 ? 2 : (f) == 3 ? 3 :  -1)
#define AF_I2C0_SDA_PORT(f)          ((f) == 0 ? 0 : (f) == 1 ? 3 : (f) == 2 ? 2 : (f) == 3 ?  : (f) == 4 ? 2 : (f) == 5 ? 5 : (f) == 6 ? 4 :  -1)
#define AF_I2C0_SCL_PORT(f)          ((f) == 0 ? 0 : (f) == 1 ? 3 : (f) == 2 ? 2 : (f) == 3 ?  : (f) == 4 ? 2 : (f) == 5 ? 5 : (f) == 6 ? 4 :  -1)
#define AF_ACMP0_OUT_PORT(f)         ((f) == 0 ? 4 : (f) == 1 ?  : (f) == 2 ? 3 :  -1)
#define AF_ACMP1_OUT_PORT(f)         ((f) == 0 ? 5 : (f) == 1 ?  : (f) == 2 ? 3 :  -1)
#define AF_DBG_SWO_PORT(f)           ((f) == 0 ? 5 : (f) == 1 ? 2 :  -1)
#define AF_DBG_SWDIO_PORT(f)         ((f) == 0 ? 5 : (f) == 1 ? 5 :  -1)
#define AF_DBG_SWCLK_PORT(f)         ((f) == 0 ? 5 : (f) == 1 ? 5 :  -1)

/** @} End of group EFM32TG_AF_Ports */


