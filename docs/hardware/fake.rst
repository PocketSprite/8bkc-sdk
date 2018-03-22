Development PocketSprite hardware
=================================

If you want to develop PocketSprite apps or games but do not have access to a real PocketSprite
or have other reasons for not wanting to use the real hardware, the SDK has an option to allow
compilation for 'fake' PocketSprite hardware. For this, you need:

- An ESP32 devboard with at least 4MiB of flash
- A display with an ILI9341 or ST7789V controller

(The Espressif ESP32-Wrover-Kit devboards have displays like this.)

Optionally, but highly recommended are:

- A small amplifier plus speaker to amplify the sound signal on GPIO26
- A PlayStation 1/2/PSX controller

To enable 'fake' PocketSprite mode, in a project, run ``make menuconfig``. Here, under ``PocketSprite hardware
config`` -> ``Hardware to compile binary for``, select the applicable option. For custom boards, the GPIOs the
LCD and the controller are connected to are configurable, although it is highly recommended to go with the 
defaults if you can. Analog audio will always be emitted on GPIO26; this cannot be changed.

If you do not have an PlayStation controller to connect, the serial console can also be used to emulate
the PocketSprite keys: in ``make monitor`` (or in any other serial port terminal emulator), you can use the
arrow or JIKL keys to emulate the D-pad, AS to emulate the A and B button, ZX for the start and select button 
and P for the power button. Due to the nature of terminal emulators, the serial port button emulation comes 
with a few issues, however: multiple keys pressed together aren't registered and 'held' keys may not register
as such but as a series of repeated taps instead. It is advised to use a PlayStation controller or real hardware
if precise button input is important.

To flash a PocketSprite configured for the 'fake' PocketSprite into the devboard, just run ``make flash``. The
application will start immediately after reset.

Limitations
-----------

The 'fake' mode has some things that are not implemented:

 - No appfs, so no filesystem to load external files from. The app binary itself is all you have access to. This
   is done to make the development system fit into the 4MiB flash that most devboards come with.
 - No chooser. The devboard will boot immediately into your app. HAL API calls to return to chooser will result in
   a program abort.
 - No powerdown/poweroff features. HAL API calls for this will result in a program abort
 - No battery monitoring features. Battery voltage will always be reported as 3.6V.

(Note: These limitiations also mean that the GB/SMS emulators can be cross-compiled to a devboard, but they will
not be able to load any ROMs due to lack of an AppFs filesystem and as such will most likely crash.)
