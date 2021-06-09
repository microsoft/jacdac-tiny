RAM_SIZE = 8
FLASH_SIZE ?= 32
BL_SIZE ?= 4
PAGE_SIZE = 2048
DEFINES += -DSTM32G030xx
STARTUP_FILE = $(PLATFORM)/cmsis_device_g0/Source/Templates/gcc/startup_stm32g031xx.s
