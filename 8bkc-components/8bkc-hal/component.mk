#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default, 
# this will take the sources in this directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_PRIV_INCLUDEDIRS := include-private

COMPONENT_OBJS := hexdump.o kcugui.o vfs-stdout.o

ifdef CONFIG_HW_POCKETSPRITE
COMPONENT_OBJS += io-pksp.o kchal-pksp.o ssd1331.o
else
COMPONENT_OBJS += kchal-fake.o spi_lcd.o 
ifdef CONFIG_HW_INPUT_PSX
COMPONENT_OBJS += psxcontroller.o
endif
endif

