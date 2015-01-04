MCU = atmega1284p

CFLAGS = -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wextra -Wall -Werror -DF_CPU=16000000UL

PROGRAMMER = arduino

PORT = /dev/ttyUSB0

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.c=.o)

TARGET = avr11

all: flash

fuse:
	sudo avrdude -c $(PROGRAMMER) -p $(MCU) -U lfuse:w:0xde:m -U hfuse:w:0x91:m -U efuse:w:0xff:m

build: clean
	avr-gcc $(CFLAGS) -g -mmcu=$(MCU) -c $(SRCS)
	avr-gcc $(CFLAGS) -mmcu=$(MCU) -o $(TARGET).elf $(OBJS)
	avr-objcopy -j .text -j .data -O ihex $(TARGET).elf $(TARGET).hex
	avr-size --mcu=$(MCU) -C $(TARGET).elf

flash: build
	/usr/share/arduino/hardware/tools/avrdude -C/usr/share/arduino/hardware/tools/avrdude.conf -v -p$(MCU) -c$(PROGRAMMER) -P$(PORT) -b115200 -D -Uflash:w:$(TARGET).hex:i

clean:
	rm -f *.o *.elf *.hex

fmt:
	clang-format-3.5 -i $(SRCS)
