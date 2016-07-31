all: main.hex

# Empty rule to force other rules to be updated.
FORCE:

main.elf: main.c
	avr-gcc -mmcu=attiny45 -std=c99 -Wall -Os -o main.elf main.c

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex	

upload: check-tty main.hex
	avrdude -qq -p t45 -c stk500v1 -e -P $(SERIAL) -b 19200 -U flash:w:main.hex

clean:
	rm -f *.elf

check-tty:
ifndef SERIAL
	$(error SERIAL is undefined)
endif
