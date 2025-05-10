# VeeDuino
## MK2 Volkswagen Arduino-powered dash cluster mod
This is for the turbodiesel variant of the car

## Materials Used
Everything here was purchased on Aliexpress

Arduino Uno Clone

- Failure rate is high for these clones, it may be a good idea to use a real Arduino

Type K thermocouple and MAX6675 thermocouple module

- For monitoring exaust gas temperature
- Thermocouple should be mounted as close to exaust port as possible

GM 3-Bar MAP sensor

- For monitoring boost pressure
- This was chosen because it had good documentation and seems to be widely used in the aftermarket space

Monochrome OLED 128X64 1.3 inch (SH1106)

- This fits as a good replacement of the clock module in the dash, I sacrificed the clock as my stereo has one

## TO DO

- Improve tachometer algorithm, it is somewhat accurate but has issues with missing peaks in higher engine speeds, leading to a reported RPM of some integer multiple of the physical value
- Improve OLED communication, the current hack fix is to reset the display periodically to prevent persistent artifacts and freezing

## Compile and Upload

    arduino-cli compile --fqbn arduino:avr:uno VeeDuino

    arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:uno VeeDuino
