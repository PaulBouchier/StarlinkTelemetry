# StarlinkTelemetry

The StarlinkTelemetry code runs on an M5StickC on the Starlink battery subsystem that is part of
Paul Bouchier's Starlink system. See the design documentation for a description of the system, and
StarlinkTelemetry's place in it.

## Design Documentation

System design documentation and requirements are at:
https://sites.google.com/site/paulbouchier/home/projects/starlink

## Wifi passwords

This repo contains a file: secrets.template.h. Follow the instructions in that file to set
your SSID and Wifi password, and rename it to secrets.h to enable building the project in
the Arduino environment.

## Build

### Prerequisites

You must have the M5Stack board support installed in your Arduino environment - follow
these instructions: https://docs.m5stack.com/en/arduino/arduino_ide

Install required libraries using these instructions: https://roboticsbackend.com/install-arduino-library-from-github/

Required libraries:
- ESP32Ping library from  https://github.com/marian-craciunescu/ESP32Ping
- M5StickC from https://github.com/m5stack/M5StickC

