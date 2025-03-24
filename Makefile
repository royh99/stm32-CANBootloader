##
## This file is part of the libopenstm32 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##
BINARY		= stm32_canloader
OUT_DIR     = obj
PREFIX		?= arm-none-eabi
#PREFIX		?= arm-elf
SIZE  		= $(PREFIX)-size
CC			= $(PREFIX)-gcc
CPP			= $(PREFIX)-g++
LD			= $(PREFIX)-gcc
OBJCOPY		= $(PREFIX)-objcopy
OBJDUMP		= $(PREFIX)-objdump
MKDIR_P     = mkdir -p
TERMINAL_DEBUG ?= 0
CFLAGS		= -O0 -g3 -Wall -Wextra -Iinclude/ -Ilibopeninv/include -Ilibopencm3/include \
             -fno-common -fno-builtin -pedantic -DSTM32G4 \
				 -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -std=gnu99 -ffunction-sections -fdata-sections
CPPFLAGS    = -O0 -g3 -Wall -Wextra -Iinclude/ -Ilibopeninv/include -Ilibopencm3/include \
            -fno-common -std=c++17 -pedantic -DSTM32G4 \
				-ffunction-sections -fdata-sections -fno-builtin -fno-rtti -fno-exceptions -fno-unwind-tables -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDSCRIPT	= $(BINARY).ld
LDFLAGS         = -Llibopencm3/lib -T$(LDSCRIPT) -nostartfiles -Wl,--gc-sections,-Map,linker.map -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -lc -lm
OBJSL		= $(BINARY).o hwinit.o
OBJS     	= $(patsubst %.o,$(OUT_DIR)/%.o, $(OBJSL))
vpath %.c src/ libopeninv/src/
vpath %.cpp src/ libopeninv/src/

OPENOCD_BASE	= /usr
OPENOCD			= $(OPENOCD_BASE)/bin/openocd
OPENOCD_SCRIPTS	= $(OPENOCD_BASE)/share/openocd/scripts
OPENOCD_FLASHER	= $(OPENOCD_SCRIPTS)/interface/stlink-v2.cfg
OPENOCD_TARGET	= $(OPENOCD_SCRIPTS)/target/stm32g4x.cfg

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
NULL := 2>/dev/null
endif

all: directories images
Debug:images
Release: images
cleanDebug:clean
images: get-deps $(BINARY)
	@printf "  OBJCOPY $(BINARY).bin\n"
	$(Q)$(OBJCOPY) -Obinary $(BINARY) $(BINARY).bin
	@printf "  OBJCOPY $(BINARY).hex\n"
	$(Q)$(OBJCOPY) -Oihex $(BINARY) $(BINARY).hex
	$(Q)$(SIZE) $(BINARY)

directories: ${OUT_DIR}

${OUT_DIR}:
	$(Q)${MKDIR_P} ${OUT_DIR}

$(BINARY): $(OBJS) $(LDSCRIPT)
	@printf "  LD      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(LD) $(LDFLAGS) -o $(BINARY) $(OBJS) -lopencm3_stm32g4

$(OUT_DIR)/%.o: %.c Makefile
	@printf "  CC      $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(OUT_DIR)/%.o: %.cpp Makefile
	@printf "  CPP     $(subst $(shell pwd)/,,$(@))\n"
	$(Q)$(CPP) $(CPPFLAGS) -o $@ -c $<

clean:
	@printf "  CLEAN   ${OUT_DIR}\n"
	$(Q)rm -rf ${OUT_DIR}
	@printf "  CLEAN   $(BINARY)\n"
	$(Q)rm -f $(BINARY)
	@printf "  CLEAN   $(BINARY).bin\n"
	$(Q)rm -f $(BINARY).bin
	@printf "  CLEAN   $(BINARY).hex\n"
	$(Q)rm -f $(BINARY).hex
	@printf "  CLEAN   $(BINARY).srec\n"
	$(Q)rm -f $(BINARY).srec
	@printf "  CLEAN   $(BINARY).list\n"
	$(Q)rm -f $(BINARY).list

flash: images
	@printf "  FLASH   $(BINARY).bin\n"
	@# IMPORTANT: Don't use "resume", only "reset" will work correctly!
	$(Q)$(OPENOCD) -s $(OPENOCD_SCRIPTS) \
		       -f $(OPENOCD_FLASHER) \
		       -f $(OPENOCD_TARGET) \
		       -c "init" -c "reset halt" \
		       -c "flash write_image erase $(BINARY).hex" \
		       -c "reset" \
		       -c "shutdown" $(NULL)

.PHONY: directories get-deps images clean

get-deps:
ifneq ($(shell test -s libopencm3/lib/libopencm3_stm32g4.a && echo -n yes),yes)
	@printf "  GIT SUBMODULE\n"
	$(Q)git submodule update --init
	@printf "  MAKE libopencm3\n"
	$(Q)${MAKE} -C libopencm3 TARGETS=stm32/g4
endif

Test:
	cd test && $(MAKE)
cleanTest:
	cd test && $(MAKE) clean
