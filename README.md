# Custom Portable Weigh Scale

## Overview

Building a custom weigh scale to measure up to 20 kg with high precision. Using a load cell to measure weight and an ESP32C3 development board to send data wirelessly to connected devices and display on LCD module. Modeling the frame of weigh scale in CAD software to 3D print. Designing a PCB to integrate components and optimize space.

## Repository Contents

- `src/` — Source code
- `include/` - Header files for all submodules
- `CAD_Files/` — All CAD models and printable files
- `3D_Prints/` - STL files for physical parts
- `PCB/` - PCB schematic and layout file (in progress)

## Hardware and Software

Hardware: HX711 Amplifier Board, XIAO ESP32C3, Aluminum Bar-type Load Cell, USB Debug Cable, Jumper Wires, 2x M4 and 2x M5 hex screws
Software: ESP-IDF in preferred configuration. Example: Setup through PlatformIO extension in VS Code

## Building and Running

Solder load cell wires to HX711 board and use jumper wires to connect to ESP. 3D print test bed and force plate, and fasten them to the load cell with screws. Flash the source code to the ESP and inspect through serial monitor.

## Results

At present, the load cell reliably measures weight up to 10 kg and can send data wirelessly via Bluetooth Low Energy (BLE). Next steps are integrating LCD module, modeling full frame, and finishing PCB layout for fabrication.
