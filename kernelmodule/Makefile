TARGET = kernelboot
OBJS = kernel.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = kernelboot.exp

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR =
LDFLAGS = -mno-crt0 -nostartfiles
LIBS = -lpspctrl_driver


PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak  

all:
	psp-build-exports -s kernelboot.exp
	cp *.S ../
	cp *.prx ../