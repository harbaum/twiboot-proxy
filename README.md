# twiboot-proxy.ino

This sketch allows a arduino leonardo or a ftDuino to act as a regular
arduino bootloader on USB side and as a twiboot host on i2c side. It
can be used to flash a Atmega328 based twiboot device from within the
Arduino IDE using a Arduino Leonardo as a proxy.  The Arduino IDE
needs to be setup for a Arduino Uno/Nano or similar.
 
This needs a Leonardo (or other 32u4 based device) as the leonardo
itself does not activate its own bootloader when the Arduino IDE tries
to access it as a Nano Uno or similar.
  
UART side: regular STK500 protocol as used by Arduino IDE for the Nano/Uno/ATmega328P

I2C side: twiboot https://github.com/orempel/twiboot
  