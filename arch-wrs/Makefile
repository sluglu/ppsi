# All files are under A (short for ARCH): I'm lazy
A := arch-$(ARCH)

# classic cross-compilation tool-set
SIZE =		$(CROSS_COMPILE)size

CFLAGS += -Itools
# Prevent a warning for a missing prototype in pp_printf, bug since ever
ARCH_PP_PRINTF_CFLAGS += -include ../$A/include/strnlen.h

OBJ-y += $A/wrs-startup.o \
	$A/hal_minirpc.o \
	$A/main-loop.o \
	$A/wrs-io.o \
	$A/wrs-conf.o \
	$A/wrs-calibration.o \
	$A/wrs-ipcserver.o \
	$A/shmem.o \
	$A/util.o \
	lib/cmdline.o \
	lib/conf.o \
	lib/libc-functions.o \
	lib/dump-funcs.o \
	lib/drop.o \
	lib/assert.o \
	lib/div64.o \
	lib/time-arith.o

export size_db_file:=size_wrs_db.txt
export size_info_file:=size_wrs_info.txt

# We only support "wrs" time operations
TIME = wrs
include time-$(TIME)/Makefile

# Unix time operations are always included as a fallback
include time-unix/Makefile
CFLAGS += -Iproto-ext-whiterabbit -Iproto-standard

# mini-rpc directory contains minipc library
export CROSS_COMPILE
MINIPC_DIR := $A/mini-rpc
MINIPC_LIB := $(MINIPC_DIR)/libminipc.a
CFLAGS += -I$(MINIPC_DIR)
.PHONY: $(MINIPC_LIB)
$(MINIPC_LIB):
	$(MAKE) -C $(MINIPC_DIR) CFLAGS_OPTIMIZATION="$(CFLAGS_OPTIMIZATION)"

OBJ-y += $(MINIPC_LIB)

all: $(TARGET)

# to build the target, we need -lstd again, in case we call functions that
# were not selected yet (e.g., pp_init_globals() ).
$(TARGET): $(TARGET).o
	$(CC) -Wl,-Map,$(TARGET).map2 -o $@ $(TARGET).o -lrt
	$(SIZE) $@
	./arch-wrs/save_size.sh $(SIZE) $@
	cat $(size_info_file)


