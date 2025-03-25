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
#include <stdint.h>
#include <cstddef>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/fdcan.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/stm32/crc.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/cm3/scb.h>
#include "hwinit.h"

#define FLASH_START         0x08000000
#define SMALLEST_PAGE_WORDS 256
#define PROGRAM_WORDS       256
#define APP_FLASH_START     0x08008000 // this allows for 96K in 128K block 1
#define BOOTLOADER_MAGIC    0xAA
#define DELAY_100           (1 << 17)
#define NODECANID           0x7DE
#define MASTERCANID         0x7DD

enum states
{
   MAGIC, PAGECOUNT, PAGE, CRC, PROGRAM, DONE
};

static volatile states state = MAGIC;
static uint32_t page_buffer[PROGRAM_WORDS];
static bool usartUpdate = false;

static void write_flash(uint32_t addr, uint32_t *pageBuffer)
{
   flash_program(addr, (uint8_t*)pageBuffer, PROGRAM_WORDS*2); // length is in bytes (multiple of 8)                                                                                                
}

static void send_byte(uint8_t b)
{
   fdcan_transmit(CAN1, NODECANID, false, false, false, false, 1,  &b);
   if (usartUpdate) usart_send_blocking(USART3, b);
}

static void send_can_hello()
{
   uint32_t data[] = { '3' | ('1' << 8),  DESIG_UNIQUE_ID2 };
   fdcan_transmit(CAN1, NODECANID, false, false, false, false, 8, (uint8_t*)data);
}

static bool can_recv(uint8_t* data, uint8_t& len)
{
   uint32_t id;
   bool ext, rtr;
   uint8_t length, fmi;
   uint8_t fifo = 0;
   
   if (FDCAN_IR(CAN1) & (FDCAN_IR_RF0N)) fifo = 0;
   if (FDCAN_IR(CAN1) & (FDCAN_IR_RF1N)) fifo = 1;
   
   while (fdcan_available_rx(CAN1, fifo)) 
   {
	  fdcan_receive(CAN1, fifo, true, &id, &ext, &rtr, &fmi, &length, (uint8_t*)data, NULL);
   }
   FDCAN_IR(CAN1) |= ( FDCAN_IR_RF0N | FDCAN_IR_RF1N); // set flags to "1" to clear them!
   return 1;
}

static void wait()
{
   for (volatile uint32_t i = DELAY_100; i > 0; i--);
}


extern "C" int main(void)
{
   uint32_t addr = APP_FLASH_START;

   clock_setup();
   //initialize_pins();
   can_setup(MASTERCANID);
   usart_setup();

   send_can_hello();
   usart_send(USART3, '2'); //advertise version 2 as the protocol is unchanged

   wait();

   if (state == PAGECOUNT || state == PAGE || state == PROGRAM)
   {
      flash_unlock();

      while (state != DONE)
      {
         if (state == PROGRAM)
         {
            write_flash(addr, page_buffer);
            addr += sizeof(page_buffer);
            state = PAGE;
            send_byte('P');
         }
         iwdg_reset();
      }

      //Program the final page
      write_flash(addr, page_buffer);
      flash_lock();
   }

   //We are done lets tell the world this!!
   if (state == DONE)
      send_byte('D');

   wait();

   can_teardown();
   usart_teardown();
   clock_teardown();

   void (*app_main)(void) = (void (*)(void)) *(volatile uint32_t*)(APP_FLASH_START + 4);
   SCB_VTOR = APP_FLASH_START;
   app_main();

   return 0;
}

static void handle_data(uint8_t* data, uint8_t)
{
   uint32_t* words = (uint32_t*)data;
   static uint8_t numPages = 0;
   static uint32_t currentWord = 0;
   static uint32_t crc;

   switch (state)
   {
   case MAGIC:
      if ((usartUpdate && data[0] == BOOTLOADER_MAGIC) || words[0] == DESIG_UNIQUE_ID2)
      {
         send_byte('S');
         state = PAGECOUNT;
      }
      break;
   case PAGECOUNT:
      numPages = data[0];
	  if (numPages > 0)
		{
		flash_unlock();
		// calc start page and erase 2K pages based on numPages of 1K pages
		for (uint8_t page = ((APP_FLASH_START-FLASH_START)/2048); page < ((numPages+1)/2)+((APP_FLASH_START-FLASH_START)/2048); page++)
      {  
         flash_clear_status_flags();
		   flash_erase_page(page); // erase 2K pages based on numPages of 1K pages
         }
      }
      state = PAGE;
      currentWord = 0;
      send_byte('P');
      crc_reset();
      break;
   case PAGE:
      page_buffer[currentWord++] = words[0];
      page_buffer[currentWord++] = words[1];
      crc_calculate(words[0]);
      crc = crc_calculate(words[1]);

      if (currentWord == PROGRAM_WORDS)
      {
         state = CRC;
         send_byte('C');
      }
      else if (!usartUpdate)
      {
         send_byte('P');
      }
      break;
   case CRC:
      currentWord = 0;
      crc_reset();
      if (words[0] == crc)
      {
         numPages--;
         if (numPages == 0)
         {
            state = DONE;
         }
         else
         {
            state = PROGRAM;
         }
      }
      else
      {
         send_byte('E');
         state = PAGE;
      }
      break;
   case PROGRAM:
      //Flash programming done in main()
      break;
   case DONE:
      //Nothing to do!
      break;
   }

}

/* Interrupt service routines */
extern "C" void fdcan1_it0_isr()
{
   uint8_t canData[8], len;

   can_recv(canData, len);
   handle_data(canData, len);
}

extern "C" void usart3_isr()
{
   static uint8_t buffer[8], currentByte = 0;

   uint8_t data = usart_recv(USART3);

   usartUpdate = true;

   switch (state)
   {
   case MAGIC:
   case PAGECOUNT:
      handle_data(&data, 1);
      break;
   case PAGE:
      buffer[currentByte++] = data;
      if (currentByte == 8)
      {
         currentByte = 0;
         handle_data(buffer, 8);
      }
      break;
   case CRC:
      buffer[currentByte++] = data;
      if (currentByte == 4)
      {
         currentByte = 0;
         handle_data(buffer, 4);
      }
      break;
   default:
      //Should never get here
      break;
   }
}
