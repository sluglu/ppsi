# classic cross-compilation tool-set
AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

include ../.config
CFLAGS = -Wall -ggdb -I../include -I../arch-$(CONFIG_ARCH)/include

PROGS = ptpdump adjtime jmptime chktime adjrate monotonicClock
LDFLAGS += -lrt

all: $(PROGS)

$(PROGS): $(wildcard *.h) $(wildcard ../include/ppsi/*.h)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

dump-funcs.o: ../lib/dump-funcs.c
	$(CC) $(CFLAGS) -c $< -o $@

ptpdump: dump-main.o dump-funcs.o
	$(CC) $(LDFLAGS) dump-main.o dump-funcs.o -o $@

clean:
	rm -f $(PROGS) *.o *~

