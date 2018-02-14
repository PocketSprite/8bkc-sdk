# esptool_py component is special, because it doesn't contain any
# IDF source files. It only adds steps via Makefile.projbuild &
# Kconfig.projbuild

COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_PRIV_INCLUDEDIRS := ibxm

COMPONENT_SRCDIRS := . ibxm
