PocketSprite Hardware Abstraction Layer
---------------------------------------

The PocketSprite has a fair amount of hardware, driven by various internal peripherals
and GPIOs. It also has some internal communication with the bootloader to boot the various 
applications that may be installed on it. The HAL manages all this and more. For compatibility
and upgradability, it is highly recommended to use the HAL to do low-level things instead of
poking the hardware directly.



.. include:: /_build/inc/8bkc-hal.inc
