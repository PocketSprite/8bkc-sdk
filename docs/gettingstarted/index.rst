***************
Getting Started
***************

So, you want to actually develop applications for the PocketSprite? Great choice! (and I'm not only saying that because
I just spent ages typing up the SDK documentation.) This document will allow you to get started with this.

Hardware
--------

First of all, you will need hardware to develop for. Obviously, you want to have a real PocketSprite to run the 
applications on. If you haven't yet, go `buy <http://pocketsprite.com/>`_ one! If you don't have one yet, or don't
want to use the one you have for development, you can also use an ESP32 devboard, a cheap LCD and an audio amp
plus speaker to build a `'fake' <../hardware/fake>`_ PocketSprite you can develop on.

Software
--------

The PocketSprite uses ESP-IDF, as well as the toolchain for the Xtensa processor that is in the ESP32
chip that forms the brains of the small console. While ESP-IDF is included (as a submodule) in the 
PocketSprite SDK, the toolchain needs some setting up; please refer to the ESP-IDF documentation to
`install it <https://esp-idf.readthedocs.io/en/latest/get-started/index.html>`_.

Note that while ESP-IDF supports Windows, Linux and Mac as development platforms, the PocketSprite SDK for now only 
supports Linux (and work on Mac with Brew, see the note below).  Some users have reported success with using Windows Subsystem for Linux.

After you have installed esp-idf using the linked instructions, get the PocketSprite SDK components. Note that the
SDK still uses the old '8bkc' name as an identifier::

    cd ~/esp
    git clone --recursive https://github.com/PocketSprite/8bkc-sdk.git

Now, to let your PocketSprite projects know where the SDK lives, you'll need to add a line to your shell profile,
similar to what you needed to do for the ESP-IDF installation. In case you use Bash as a shell and you have a file
called ``.bash_profile`` in your home directory, edit that file, otherwise open up the ``.profile`` file in your home
directory. Regardless of the file you now are editing, add a line indicating where the 8bkc-sdk can be found::

    export POCKETSPRITE_PATH=~/esp/8bkc-sdk

You can now log out and log in again and check if the change took by doing ``printenv POCKETSPRITE_PATH``. It should
return the path you entered earlier.

Finally, the PocketSprite SDK contains some utilities running on your computer that need to be compiled. The SDK will
automatically take care of this, but it needs to have a compiler (gcc) and the development packages of some
libraries (libgd, libxml2, imagemagick) installed. Please make sure these are available. For instance, under Debian/Ubuntu,
you would do::

    apt-get install gcc libgd-dev libxml2-dev imagemagick

to install them.

For Mac, ``gcc`` should already come with XCode and need to install the dependencies using Brew instead::

    brew install libgd libxml2 imagemagick
    
You can get Brew `here <https://brew.sh/>`_.

