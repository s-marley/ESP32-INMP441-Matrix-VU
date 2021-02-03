# ESP32-INMP441-Matrix-VU
This is the repository for a 3D-printed, (optionally) battery-powered, WS2812B LED matrix that produces pretty patterns using audio input from an INMP441 digital microphone and running on an ESP32. Watch the explanatory video at the YouTube link below:

## Hardware
At a minimum, you will need:
* A 3D printer to produce the case, STLs are available.
* An ESP32 dev board, the exact type doesn't matter.
* An 8x8 (80x80 mm) or 16x16 (160x160 mm) WS2812B LED matrix.
  * My code should work with any matrix providing the width is 8 or a multiple of 16. The height isn't important. If you're using WLED, you can have whatever size you like!
  * The data input for my matrix is at the bottom-left, and is wired in a horizontal zig-zag pattern.
* An INMP441 I2S microphone.
* A power switch
* A DC input socket and 5V power supply if you are not powering from a battery.
* The usual electronics tools and equipment to build it.

![No battery schematic](/Wiring2_bb.png)

If you want to battery power your creation, you will also need:
* A LiPo / LiIon battery - exact specs depend on the run time you need and what solution you are using for your charge / boost circuits.
* A charge / boost board to connect to your battery and output 5V for the LEDs.
  * Alternatively you could use seperate modules for these functions, this might be a good idea for the higher power 16x16 matrix.

![Battery schematic](/Wiring_bb.png)

## Software
Regarding software you have two main options, the software given in this repository or [WLED sound reactive fork](https://github.com/atuline/WLED). The hardware connections are the same for either option. There are many advatages of using WLED over this software, it's a much more complete solution with better support for syncing multiple controllers setting up playlists of patterns and customising the patterns that exist. The only realy advantage of my software is that I wanted to write my own patterns and be able to completely customise how things worked. The choice is yours. If you want to use my software, read on...

**Software setup**
1. Download the code, and make sure you have all of the required libraries installed. They are listed at the top of the .ino file.
2. Make sure your matrix is set up correctly in the software for input position, and wiring style. See notes at the top of the .ino file for more details.
3. Change `M_WIDTH` and `M_HEIGHT` to the correct number of LEDs for your matrix.
4. Go into web_server.h and change `ssid`, `password`, `ssid2`, and `password2` to your primary and backup network credentials.
5. Upload the code to your ESP32 and hope for the best!

## Using the matrix
Every time you power the matrix, it will attempt to connect to the primary WiFi network for 5 seconds. If this fails, it will try the secondary credentials for 5 seconds. If this also fails, it will fall back to whatever its previous setting were, but you will be unable to make any changes. I have the secondary credentials set to the hotspot on my phone, so I can use the device outside of my home network.
If it succeeds in connecting to a network, the local IP of the ESP will scroll across the screen. Go to this address in any browser to change the settings. The settings that can be altered are:
* Next pattern - Self explanatory.
* Auto-change pattern - Will cycle through the patterns, changing them once every 10s by default.
* Brightness - Be carerful here, at max brightness the larger panels can consume many amps. Make sure your power supply is up to the job!
* Gain - Increases the sensitivity of the microphone to adjust for louder or quieter environoments.
* Squelch - Increasing this value puts a limit on the quietest sounds that will be picked up. Useful for if you have some background noise to remove.
The settings are saved in the EEPROM so they should remain the same across power cycles.
