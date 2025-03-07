# MiniMP3: An ESP32 MP3 & WAV Audio Library
A lightweight minimal MP3 and WAV audio library for ESP32 microcontrollers, optimized for low memory usage.

## Features
- **MP3 Support**: Decodes MPEG Layer III (MP3) files, the most commonly used format.
- **WAV Support**: Plays PCM 16-bit Little Endian WAV files.
- **Multiple Audio Sources**: Reads audio from SD cards or web streams.
- **DAC Output**: Outputs 8-bit stereo audio at 44.1 kHz using the ESP32 DAC pins.
- **Optimized for ESP32**: Designed for low-memory environments.

## Hardware Compatibility
- Tested on **ESP32-WROOM-32**.
- Note: ensure that the clock is set to 240MHz, otherwise MP3 decoding is maybe too slow and the sound begin falter.

## Getting Started
1: Connect an SD card or use WiFi as an audio Source.

2: Use the ESP32 DAC pins to connect speakers or an amplifier, for ground i highly recomment a voltage divider.

## License
This project is licensed under the **GNU General Public License v3.0**. See the `LICENSE` file for details.
