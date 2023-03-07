# use compressed instructions for RISCV
USE-COMP-INSTR-$(CONFIG_RISCV_COMP_INSTR) = c

CFLAGS-CPU-LM32  = -mmultiply-enabled -mbarrel-shift-enabled
CFLAGS-CPU-RISCV = -march=rv32im$(USE-COMP-INSTR-y) -mabi=ilp32

# use lm32 if not defined
CPU_ARCH ?= LM32

CFLAGS += -ffreestanding \
	-ffunction-sections -fdata-sections \
	-Itools -Iproto-standard \
	$(CFLAGS-CPU-$(CPU_ARCH))

# WRPCSW_ROOT shall be defined in the env by WRPC's Makefile
CFLAGS += -I$(WRPCSW_ROOT)/include -I$(WRPCSW_ROOT)/include/std -I$(WRPCSW_ROOT)/softpll

# TODOA: remove --no-gc-sections
PPSI_O_LDFLAGS = --no-gc-sections -u wrc_ptp_init

# Let's use the pp_printf we already have in wrpc-sw
CONFIG_NO_PRINTF = y

# All files are under A (short for ARCH): I'm lazy
A := arch-$(ARCH)

OBJ-y += \
	$A/wrpc-io.o \
	$A/wrpc-spll.o \
	$A/wrpc-calibration.o \
	$A/wrc_ptp_ppsi.o \
	lib/dump-funcs.o \
	lib/drop.o \
	lib/div64.o \
	lib/time-arith.o \

OBJ-$(CONFIG_WRPC_FAULTS) += $A/faults.o

# wrpc/Makefile invokes this makefile with the ppsi.a target,
# if you can also call it directly.
all: $(TARGET).a

# We only support "wrpc" time operations
TIME := wrpc
include time-wrpc/Makefile

fsm.o: wrpcsw_root_check

wrpcsw_root_check:
	@if ! [ -e "$(WRPCSW_ROOT)/include/wrc.h" ]; then\
		echo "Error:  can't find $(WRPCSW_ROOT)/include/wrc.h" >&2; \
		echo "  please set WRPCSW_ROOT in your environment" >&2; \
		exit 1;\
	fi
