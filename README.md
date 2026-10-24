# ESP32 Retro Viewer — animation / Text / Glitch effects

### An experimental screen-based artwork and spatial essay

This project uses an ESP32-S3 microcontroller, a 2-inch LCD screen and an SD card to create a small, self-contained moving-image and text display, designed to fit inside a modified 1960s slide viewer.

The device forms part of an evolving **distributed video essay / installation essay**, in which narrative fragments are encountered across different physical objects, screens and technologies.

The work explores looking, attention, memory, technological obsolescence and the relationship between viewer and viewed.

## What the code does

The Arduino sketch runs an automated sequence of moving images, text and visual glitches:

- **Eye animation:** Plays sequences of an eyeball animation stored on an SD card as individual RAW image frames.
- **Scrambling text:** Displays passages of writing character by character, with ASCII symbols rapidly changing before resolving into readable text.
- **Visual glitches:** Occasional flashes of colour and horizontal interference interrupt the text.
- **Unexpected interruptions:** The eyeball animation sometimes breaks into the middle of a text sequence for approximately one second, as though a signal is attempting to get through.
- **Variable timing:** Eye sequences last randomly between 3 and 10 seconds. Longer text passages remain on screen for longer.
- **Continuous playback:** Following the text sequence, the eye animation plays for 30 seconds before the entire work repeats.

The irregular timing and interruptions introduce an element of unpredictability, making the device appear to be attempting to communicate.

## Hardware

- ESP32-S3 microcontroller
- 2-inch ST7789 TFT LCD (320 × 240 pixels)
- MicroSD card and SPI card reader
- Modified vintage slide viewer
- 5V USB power supply

The LCD and SD card share an SPI connection.

| Connection | ESP32 GPIO |
|---|---|
| SPI SCK | 11 |
| SPI MOSI | 12 |
| SPI MISO (SD) | 13 |
| LCD CS | 10 |
| LCD DC | 46 |
| LCD RST | 8 |
| SD CS | 41 |
| LCD BL | 3.3V |

## Software and image files

The code was developed for the Arduino IDE using the ESP32 board package and the following libraries:

- Adafruit GFX
- Adafruit ST7789
- SPI
- SD

The animation uses 289 numbered RAW image frames (`1000.raw` to `1288.raw`), stored in the root directory of the SD card. Frames are 320 × 240 pixels, little-endian RGB565, played at approximately 10 frames per second.

Text passages are stored directly within the Arduino sketch and can be edited or replaced. Timing, scrambling, glitch frequency and image duration can also be modified through the code.

This version operates locally, without requiring an internet connection or network.

## Artistic context

The project adapts an analogue image-viewing object into a digital device that appears to look back at its audience.

The written passages are part of an ongoing, original essay exploring the material and philosophical relationships between images, objects, memory and technological systems.

Rather than presenting the essay as a single linear film or written document, the work distributes its narrative across multiple devices, encouraging fragmented, spatial and non-linear encounters.

## Development and AI acknowledgement

**Concept, artistic direction, original text, image content, hardware assembly and iterative testing:** Antony Hall.

**Code:** AI-generated with OpenAI ChatGPT through an iterative process of prompting, testing, debugging and refinement directed by Antony Hall.

The software was developed experimentally to explore how generative AI can support creative coding, physical computing and the construction of bespoke media devices.

## Authorship and use

This repository documents an experimental artistic research project.

The original written text and artistic content belong to Antony Hall. The inclusion of this material in the source code does not grant permission for its independent reproduction or reuse.

The code is provided for reference, experimentation and learning. No separate open-source licence has yet been specified.
