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
#ifndef HWINIT_H_INCLUDED
#define HWINIT_H_INCLUDED


#ifdef __cplusplus
extern "C"
{
#endif

#define PINDEF_BLKNUM    3  //3rd to last flash page
#define PINDEF_BLKSIZE   2048
#define NUM_PIN_COMMANDS 10
#define PIN_IN 0
#define PIN_OUT 1

struct pindef
{
   uint32_t port;
   uint16_t pin;
   uint8_t inout;
   uint8_t level;
};

struct pincommands
{
   struct pindef pindef[NUM_PIN_COMMANDS];
   uint32_t crc;
};

#define PINDEF_NUMWORDS (sizeof(struct pindef) * NUM_PIN_COMMANDS / 4)

void clock_setup();
void clock_teardown();
void can_setup(int masterCANID);
void can_teardown();
void usart_setup();
void usart_teardown();
void initialize_pins();

#ifdef __cplusplus
}
#endif

#endif // HWINIT_H_INCLUDED
