/*
 * This file is part of the CANBootloader project.
 *
 * Copyright (C) 2022 WDR Automatisering https://wdrautomatisering.nl/
 * Copyright (C) 2023 Johannes Huebner https://openinverter.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/fdcan.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/cm3/nvic.h>
#include "hwinit.h"


/**
* Start clocks of all needed peripherals
*/
void clock_setup()
{
 	rcc_osc_on(RCC_HSI16);
	rcc_wait_for_osc_ready(RCC_HSI16);
	rcc_set_sysclk_source(RCC_CFGR_SWx_HSI16);
	rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
	rcc_set_ppre1(RCC_CFGR_PPREx_NODIV);
	rcc_set_ppre2(RCC_CFGR_PPREx_NODIV);
   
	rcc_osc_off(RCC_PLL);
	rcc_set_main_pll(RCC_PLLCFGR_PLLSRC_HSI16, 2, 12, 0, 4, 4);// Pll src, m, n, p, q, r  clk = 16MHz /2 *12 /4 = 24MHz
	rcc_osc_on(RCC_PLL);
	rcc_wait_for_osc_ready(RCC_PLL);
   
   rcc_osc_on(RCC_LSI);
   
   rcc_apb1_frequency = 24000000;
   rcc_apb2_frequency = 24000000;
   
   rcc_periph_clock_enable(RCC_GPIOA);
   rcc_periph_clock_enable(RCC_GPIOB);
   rcc_periph_clock_enable(RCC_GPIOC);
   rcc_periph_clock_enable(RCC_CRC);
   RCC_CCIPR |= RCC_CCIPR_FDCANSEL_PCLK <<RCC_CCIPR_FDCANSEL_SHIFT; // select PCLK1 for FDCAN
   rcc_periph_clock_enable(RCC_FDCAN);
   rcc_periph_clock_enable(RCC_USART3);
   
   rcc_wait_for_osc_ready(RCC_LSI);
   iwdg_set_period_ms(2000);
   iwdg_start();
	
   rcc_wait_for_osc_ready(RCC_LSI);
   iwdg_set_period_ms(2000);
   iwdg_start();
} 

void clock_teardown()
{
   rcc_set_sysclk_source(RCC_CFGR_SWx_HSI16);
   rcc_osc_off(RCC_PLL);
}

void can_setup(int masterId)
{
   fdcan_init(CAN1, 100);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
   gpio_set_af(GPIOA, GPIO_AF9, GPIO11 | GPIO12);
   
   // CAN cell init.
   // Setting the bitrate to 500KBit. PCLK = 24MHz,
   // prescaler = 6 -> 4MHz time quanta frequency.
   // 1tq sync + 6tq bit segment1 (TS1) + 1tq bit segment2 (TS2) =
   // 8time quanto per bit period, therefore 4MHz/16 = 500kHz
   //
	fdcan_set_can(CAN1,
		false,	// auto_retry_disable?
		false,	// receive FIFO locked mode?
		false,	// tx_queue_mode, false = use fifo
		false,	// silent?
		0,		// n_sjw-1
		5,  	// n_ts1-1
		0,  	// n_ts2-1	
      5); 	// n_br_presc-1 : Baud rate prescaler
   
   fdcan_init_filter(CAN1, 1, 0); // set 1 std, 0 ext filters
   //register master ID
   //can_filter_id_list_16bit_init(0, masterId << 5, 0, 0, 0, 0, true);  
   fdcan_set_std_filter(CAN1, 0, FDCAN_SFT_DUAL, masterId, masterId, FDCAN_SFEC_FIFO0); // enable for FIFO0
   
   FDCAN_ILS(CAN1) &= ~FDCAN_ILS_RxFIFO0; // select sources for int0 RX 
   FDCAN_IE(CAN1)  |= FDCAN_IE_RF0NE; 	// enable interupt source for rx fifo 0 new message
   nvic_enable_irq(NVIC_FDCAN1_IT0_IRQ); 	// CAN RX ints
   fdcan_start(CAN1, 100);
   fdcan_enable_irq(CAN1, FDCAN_ILE_INT0);
}

void can_teardown()
{
   //can_reset(CAN1);
   nvic_disable_irq(NVIC_FDCAN1_IT0_IRQ);
}

void usart_setup()
{
   gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10 | GPIO11);
   gpio_set_af(GPIOB, GPIO_AF7, GPIO10 | GPIO11);

    /* Setup UART parameters. */
    usart_set_baudrate(USART3, 115200);
    usart_set_databits(USART3, 8);
    usart_set_mode(USART3, USART_MODE_TX_RX);
    usart_enable_rx_interrupt(USART3);

    /* Finally enable the USART. */
    usart_enable(USART3);
    nvic_enable_irq(NVIC_USART3_IRQ);
}

void usart_teardown()
{
    nvic_disable_irq(NVIC_USART3_IRQ);
    rcc_periph_reset_pulse(RST_USART3);
}

//Left over for future usage in an inverter style device..
// Todo: read an application configuration based on fingerprint?
//
void initialize_pins()
{
/*    uint32_t flashSize = desig_get_flash_size();
   uint32_t pindefAddr = FLASH_BASE + flashSize * 1024 - PINDEF_BLKNUM * PINDEF_BLKSIZE;
   const struct pincommands* pincommands = (struct pincommands*)pindefAddr;

   uint32_t crc = crc_calculate_block(((uint32_t*)pincommands), PINDEF_NUMWORDS);

   gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);

   if (crc == pincommands->crc)
   {
      for (int idx = 0; idx < NUM_PIN_COMMANDS && pincommands->pindef[idx].port > 0; idx++)
      {
         uint8_t cnf = pincommands->pindef[idx].inout ? GPIO_CNF_OUTPUT_PUSHPULL : GPIO_CNF_INPUT_PULL_UPDOWN;
         uint8_t mode = pincommands->pindef[idx].inout ? GPIO_MODE_OUTPUT_50_MHZ : GPIO_MODE_INPUT;
         gpio_set_mode(pincommands->pindef[idx].port, mode, cnf, pincommands->pindef[idx].pin);

         if (pincommands->pindef[idx].level)
         {
            gpio_set(pincommands->pindef[idx].port, pincommands->pindef[idx].pin);
         }
         else
         {
            gpio_clear(pincommands->pindef[idx].port, pincommands->pindef[idx].pin);
         }
      }
   } */
}
