# LED Roulette

This project is an LED roulette that lights up 8 LEDs when the button is pressed.

Download or clone this repository and open it in PlatformIO.

## Components

- ESP32-C3 Mini
- 8 LEDs
- 8 resistors (220 Ω)
- Push button
- Active buzzer

## Wiring

- GPIO0 to GPIO7 -> Resistor (220 Ω) -> LED -> GND
- GPIO10  -> Push button -> GND
- GPIO20 -> Active buzzer -> GND

## Project Structure

```text
src/
└── main.cpp
```

The `platformio.ini` file is also included.