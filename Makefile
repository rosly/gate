PROJECT_NAME := gate
OUTPUT_FILENAME := nrf51422_xxac
export OUTPUT_FILENAME
LINKER_SCRIPT=gate.ld
MAKEFILE_NAME := $(MAKEFILE_LIST)
MAKEFILE_DIR := $(dir $(MAKEFILE_NAME) ) 
GNU_INSTALL_ROOT := /home/rad2/workspace/ble/gcc-arm-none-eabi-5_3-2016q1
GNU_VERSION := 5.3.1
GNU_PREFIX := arm-none-eabi
OBJECT_DIRECTORY = build
LISTING_DIRECTORY = $(OBJECT_DIRECTORY)

# Toolchain commands
MK              := mkdir
RM              := rm -rf
CC              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc'
AS              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-as'
AR              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ar' -r
LD              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ld'
NM              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-nm'
OBJDUMP         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objdump'
OBJCOPY         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy'
SIZE            := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size'

#source common to all targets
C_SOURCE_FILES += \
        $(abspath main.c) \
        $(abspath pn532.c) \
        $(abspath uart_log.c) \
        $(abspath external/drivers_nrf/delay/nrf_delay.c) \
        $(abspath external/drivers_nrf/twi_master/nrf_drv_twi.c) \
        $(abspath external/drivers_nrf/common/nrf_drv_common.c) \
        $(abspath external/system_nrf51422.c)

#assembly files common to all targets
ASM_SOURCE_FILES  = \
        $(abspath external/gcc_startup_nrf51.s)

#includes common to all targets
INC_PATHS = \
        -I$(abspath .) \
        -I$(abspath external) \
        -I$(abspath external/drivers_nrf/hal) \
        -I$(abspath external/drivers_nrf/device) \
        -I$(abspath external/drivers_nrf/bsp) \
        -I$(abspath external/drivers_nrf/delay) \
        -I$(abspath external/drivers_nrf/nrf_soc_nosd) \
        -I$(abspath external/drivers_nrf/twi_master) \
        -I$(abspath external/drivers_nrf/config) \
        -I$(abspath external/drivers_nrf/common) \
        -I$(abspath external/util_nrf) \
        -I$(abspath external/CMSIS/Include) \
        -I$(abspath /opt/libsodium/include)

LIBS += \
        -lsodium

LINKER_PATHS += \
        -L$(abspath external) \
        -L$(abspath /opt/libsodium/lib)

DEFINES += \
        NRF51 \
        BOARD_PCA10028

#CFLAGS += -DT2T_PARSER_ENABLE
#CFLAGS += -DSWI_DISABLE0
#CFLAGS += -DNDEF_PARSER_LOG_ENABLE
#CFLAGS += -DENABLE_DEBUG_LOG_SUPPORT
#CFLAGS += -DDEBUG

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))
# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )
OUTPUT_BINARY_DIRECTORY = $(OBJECT_DIRECTORY)

C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )
ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SOURCE_FILES))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SOURCE_FILES) ))
ASM_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(ASM_SOURCE_FILE_NAMES:.s=.o) )
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

#echo suspend
ifeq ("$(VERBOSE)","1")
NO_ECHO := 
else
NO_ECHO := @
endif

vpath %.c $(C_PATHS)
vpath %.s $(ASM_PATHS)

#Compiler flags
CFLAGS += -mcpu=cortex-m0
CFLAGS += -mthumb -mabi=aapcs --std=gnu99
CFLAGS += -Wall -Werror -O0 -g3
CFLAGS += -mfloat-abi=soft
# keep every function in separate section. This will allow linker to dump unused functions
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums 
CFLAGS += $(addprefix "-D", $(DEFINES))

#Linker flags
LDFLAGS += -Xlinker -Map=$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map
LDFLAGS += -mthumb -mabi=aapcs $(LINKER_PATHS) -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m0
# let linker to dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs -lc -lnosys

# Assembler flags
ASMFLAGS += -x assembler-with-cpp

#ASMFLAGS += -DNRF_LOG_USES_UART=1
#ASMFLAGS += -DT2T_PARSER_ENABLE
#ASMFLAGS += -DSWI_DISABLE0
#ASMFLAGS += -DBOARD_PCA10028
#ASMFLAGS += -DNRF51
#ASMFLAGS += -DNDEF_PARSER_LOG_ENABLE
#ASMFLAGS += -DENABLE_DEBUG_LOG_SUPPORT
#ASMFLAGS += -DDEBUG

#default target - first one defined
default: clean nrf51422_xxac

#building all targets
all: nrf51422_xxac

nrf51422_xxac: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -lm -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e finalize

## Create build directories
$(BUILD_DIRECTORIES):
	echo $(MAKEFILE_NAME)
	$(MK) $@

# Create objects from C SRC files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	@echo Assembly file: $(notdir $<)
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<
# Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -lm -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
## Create binary .bin file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

finalize: genbin genhex echosize

genbin:
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
genhex: 
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
echosize:
	-@echo ''
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ''

clean:
	$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(RM) $(BUILD_DIRECTORIES)/*.o

flash: nrf51422_xxac
	@echo Flashing: $(OUTPUT_BINARY_DIRECTORY)/$<.hex
	nrfjprog --program $(OUTPUT_BINARY_DIRECTORY)/$<.hex -f nrf51  --chiperase
	nrfjprog --reset -f nrf51

