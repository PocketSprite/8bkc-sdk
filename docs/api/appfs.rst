AppFS
-----

The PocketSprite includes a flash filesystem that allows storing multiple ESP32 applications, as well as other 
files, in the ESP32 flash. This filesystem is called appfs. Because this filesystem has to be compatible with the
way the ESP32 loads applications and maps memory, it is somewhat different from other filesystems: while files
have names and sizes as you'd expect, accessing them is more akin to accessing the raw flash underneath it and the
file access functions mirror the `ESP-IDF flash functions <file:///home/jeroen/esp8266/esp32/esp-idf/docs/_build/html/api-reference/storage/spi_flash.html>`_
more than they look like standard POSIX file access methods.

An appfs can only be stored in a partition in the SPI flash the ESP32 boots from, and thus has a maximum size of 
16MiB. The file system has a sector size of 64KiB which lines up with the 64KiB pages of the ESP32 MMU. This,
however, also implies that any file stored on the file system occupies at least 64KiB of flash. With this in mind,
please store your data in one large file instead of many smaller ones whenever possible

Some quirks of this approach are:
 - Files cannot be expanded or shrunk. The filesize has to be known on creation and cannot be changed afterwards.
 - A write to a file can only reset bits (1->0). In order to overwrite existing data, you need to make sure the region
   you write in is erased, and if not, erase it first. Note that erasing can only happen on a 4K block.
 - The appfs on the PocketSprite is 15.3 MiB in size. With a minimum file size of 64K, this means at maximum there
   can be 245 files stored in the flash.

Note: PocketSprite software `not running on PocketSprite hardware <../hardware/fake>`_ does not have an AppFS 
filesystem available.

.. include:: /_build/inc/appfs.inc
