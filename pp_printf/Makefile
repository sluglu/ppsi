# Alessandro Rubini for CERN, 2011 -- public domain

AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

CFLAGS += -I. $(CFLAGS_OPTIMIZATION) -Wall

obj-$(CONFIG_PRINTF_FULL) += vsprintf-full.o
obj-$(CONFIG_PRINTF_MINI) += vsprintf-mini.o
obj-$(CONFIG_PRINTF_NONE) += vsprintf-none.o
obj-$(CONFIG_PRINTF_XINT) += vsprintf-xint.o

# set full as a default if nothing is selected
obj-y ?= vsprintf-full.o

# If you want to pick the local div64.c, define this as "y"
obj-$(CONFIG_PRINTF_LOCAL_DIV64) += div64.o

obj-y += printf.o

# There is a static variable in pp-printf.c to accumulate stuff
CONFIG_PRINT_BUFSIZE ?= 256

CFLAGS += -DCONFIG_PRINT_BUFSIZE=$(CONFIG_PRINT_BUFSIZE)
ifdef CONFIG_PRINTF_64BIT
  CFLAGS += -DCONFIG_PRINTF_64BIT
endif

# Targets. You may want to make them different in your package

all: pp-printf.o example-printf

pp-printf.o: $(obj-y)
	$(LD) -r $(obj-y) -o $@

example-printf: example-printf.c pp-printf.o
	$(CC) $(CFLAGS) $^ -o $@

# build a special example/test for 64-bit prints (not built by default)
example-printf64: example-printf64.o pp-printf.o div64.o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o *~ example-printf