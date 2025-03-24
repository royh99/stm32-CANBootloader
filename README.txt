--------------------------------------------
-- STM32 Bootloader by CANBus
--
-- Johannes Hübner <dev@johanneshuebner.com>
-- EV_BUILDER transformed to CAN BOOTLOADER;
--------------------------------------------


This boot loader does not interact with the built-in STM32 boot loader. It 
uses an interface of your choice, in this version its CANBUS CAN1 and
USART3.
The interface flexibility and the independence from the BOOT pins are the 
main reasons for implementing this boot loader.

New update protocol
- 500KBaud Canbus on CAN1


1 Send '2' indicating version 2 bootloader, wait about 500ms for magic byte 0xAA*
2 If no reply start firmware
3 Send an 'S' indicating that it is awaiting an update size in pages
4 If no reply within about 500ms go to step 7
4.1 otherwise send a 'P' indicating that it is awaiting the actual page 
first 8 bytes, this goes on (bit banging) until complete page is received.
4.2 When page not received within about 1s, print 'T' and keep waiting
5 When page received send a 'C' indicating that it is awaiting the pages 
checksum
6 When checksum is correct and more pages need to be received, go to 
step 4.1
6.1 if all pages have been received go to step 5
6.2 When checksum isn't correct print an 'E' then go to step 4.1
7 When done print a 'D' and start main firmware

* When updating via CAN you may specify the 3rd word (DESIG_UNIQUE_ID2)
  in bytes 4-7 in order to only have the matching node enter update mode

Notes:
- By checksum I mean the one calculated by the STMs integrated CRC32 unit.
- The actual firmware has a reset command the cycle through the bootloader
- The main firmware must be linked to start at address 0x08008000
- The bootloader starts at address 0x08000000 and can be 16k in size 

--------------------------------------------
-- STM32 CAN Bootloader Updater
--
-- EV_BUILDER OPENINVERTER.ORG
--------------------------------------------

For both interfaces a python script is included. uart-update.py updates via
serial port, can-update.py updates via CAN (SocketCAN in linux)

python3 can-updater.py -d can2 -f stm32_sine.bin -i 87215032
The -i argument is optional and specifies the processor id to be updated
python3 uart-updater.py -d /dev/ttyUSB0 -f stm32_sine.bin

updates via UART

--------------------------------------------
-- Compiling
--------------------------------------------
You will need the arm-none-eabi toolchain: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
The only external depedency is libopencm3 which I forked. You can download and build this dependency by typing

make get-deps

Now you can compile stm32-canloader

make

And upload it to your board using a JTAG/SWD adapter.

