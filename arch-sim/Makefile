# All files are under A (short for ARCH): I'm lazy
A := arch-$(ARCH)

CFLAGS += -Itools -Iproto-standard -fno-tree-loop-distribute-patterns

OBJ-y += $A/sim-startup.o \
	$A/main-loop.o \
	$A/sim-io.o \
	$A/sim-conf.o \
	lib/cmdline.o \
	lib/conf.o \
	lib/dump-funcs.o \
	lib/libc-functions.o \
	lib/assert.o \
	lib/div64.o \
	lib/time-arith.o

# Support only "sim" time operations
TIME := sim
include time-$(TIME)/Makefile

all: $(TARGET)

# to build the target, we need -lstd again, in case we call functions that
# were not selected yet (e.g., pp_open_globals() ).
$(TARGET): $(TARGET).a
	$(CC) -Wl,-Map,$(TARGET).map2 -o $@ $(TARGET).a -lrt
