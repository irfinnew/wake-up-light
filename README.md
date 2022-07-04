# About

This is a dawn simulation lamp I made for my personal use.

A dawn simulator wakes you up by simulating sunrise in the bedroom; the increasing amount of light starts preparing your body for awakening.
The end result is that you naturally wake up on your own, instead of being jarred awake by the usual alarm clock sounds.

When it receives power, the wake-up light

 - Very gently increases from darkness to full brightness over the course of 30 minutes,
 - Remains at full brightness for 45 minutes,
 - Then turns off fairly quickly.

The wake-up light is switched on by a plug-in timer that activates 45 minutes before my intended rise time.

It also includes a button to toggle the light between off and full brightness, so I can turn it off after I get up.



# Design

The light controller is a classic [phase-cutting](https://en.wikipedia.org/wiki/Phase-fired_controller) triac dimmer, with zero-cross detection and triac control performed by a small microcontroller.
The dimmer drives a 150W halogen floodlight, which gives a nice orange glow at low brightness, but provides reasonably white full-spectrum light at high brightness.

The design was also a bit of an engineering exercise; I wanted to end up with a fairly minimal design without compromising stability or reliability.



## Isolation

The controller is not isolated from mains; it uses a capacitive dropper power supply, and the MCU is connected directly to the triac.
As a result, the entire circuit may be at mains potential, it must be installed so that users can't touch any of it.
This also applies to the button, which must provide adequate isolation.

The MCU also connects directly to mains for zero-cross sensing, as inspired by [Atmel application note AN2508](https://ww1.microchip.com/downloads/en/Appnotes/Atmel-2508-Zero-Cross-Detector_ApplicationNote_AVR182.pdf).


## Mains voltage and frequency

The controller was designed to run on 230VAC @ 50Hz.
If you want to run it on 120VAC, the power supply caps (`C1/C2`) probably should be doubled in value.
The firmware should be able to deal with 60Hz without problems, but I haven't tested this.


# Sources

The PCB layout is provided as a [KiCad](https://www.kicad.org/) project in `pcb/`.
The project uses a handful of custom footprints in `pcb/WakeUpLight.pretty/`, you'll need to import them into KiCad.

Gerber files in `pcb/gerber/`.

## Firmware

The firmware for the ATTiny45P lives in `mcu/`.
It is written in C, for `avr-gcc` / `avr-libc`.
If you have a `stk500v1` compatible programmer on USB and `avrdude`, just `SERIAL=/dev/ttyUSB0 make upload` should do the trick.
