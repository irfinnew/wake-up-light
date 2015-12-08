all: main.hex

# Empty rule to force other rules to be updated.
FORCE:

main.elf: main.c
	avr-gcc -mmcu=attiny45 -Wall -Os -o main.elf main.c

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex	

upload: main.hex
	avrdude -qq -p t45 -c stk500v1 -e -P /dev/ttyUSB0 -b 19200 -U flash:w:main.hex

clean:
	rm -f *.hex *.elf
