D := proto-ext-common

INC_WRH_SERVO=$(shell test ".$(CONFIG_HAS_EXT_WR)"     = ".1"  -o  \
                           ".$(CONFIG_HAS_EXT_L1SYNC)" = ".1"  \
                           &&  echo -n "true")
                   
ifeq ($(INC_WRH_SERVO),true)    
OBJ-y += $D/wrh-servo.o
endif

