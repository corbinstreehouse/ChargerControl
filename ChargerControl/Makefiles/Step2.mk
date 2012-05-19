#
# embedXcode
# ----------------------------------
# Embedded Computing on Xcode 4.3
#
# © Rei VILO, 2010-2012
# CC = BY NC SA
#
# version Apr. 20, 2012 - 21h20
#

# References and contribution
# ----------------------------------
# See About folder
# 


include $(MAKEFILE_PATH)/Avrdude.mk 

ifndef UPLOADER
    UPLOADER = avrdude
endif


# CORE libraries
# ----------------------------------
#
ifndef CORE_LIB_PATH
    CORE_LIB_PATH = $(APPLICATION_PATH)/hardware/arduino/cores/arduino
endif

CORE_LIBS_LIST    = $(subst .h,,$(subst $(CORE_LIB_PATH)/,,$(wildcard $(CORE_LIB_PATH)/*.h))) # */


# List of sources
# ----------------------------------
#

# CORE sources
#
ifdef CORE_LIB_PATH 
    CORE_C_SRCS     = $(wildcard $(CORE_LIB_PATH)/*.c) # */
    
    ifneq ($(strip $(NO_CORE_MAIN_FUNCTION)),)
        CORE_CPP_SRCS = $(filter-out %main.cpp, $(wildcard $(CORE_LIB_PATH)/*.cpp)) # */
    else
        CORE_CPP_SRCS = $(wildcard $(CORE_LIB_PATH)/*.cpp) # */
    endif        

    CORE_OBJ_FILES  = $(CORE_C_SRCS:.c=.o) $(CORE_CPP_SRCS:.cpp=.o)
    CORE_OBJS       = $(patsubst $(CORE_LIB_PATH)/%,$(OBJDIR)/%,$(CORE_OBJ_FILES))
endif


# APPlication Arduino / chipKIT libraries
#
ifndef APP_LIB_PATH
    APP_LIB_PATH  = $(APPLICATION_PATH)/libraries
endif

ifeq ($(APP_LIBS_LIST),)
    s1         = $(realpath $(sort $(dir $(wildcard $(APP_LIB_PATH)/*/*.h $(APP_LIB_PATH)/*/*/*.h)))) # */
    APP_LIBS_LIST = $(subst $(APP_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(s1)))
endif

ifndef APP_LIBS
ifneq ($(APP_LIBS_LIST),0)
    APP_LIBS      = $(patsubst %,$(APP_LIB_PATH)/%,$(APP_LIBS_LIST))
endif
endif

ifndef APP_LIB_OBJS
    FLAG = 1
    APP_LIB_CPP_SRC   = $(wildcard $(patsubst %,%/*.cpp,$(APP_LIBS))) # */
    APP_LIB_C_SRC     = $(wildcard $(patsubst %,%/*.c,$(APP_LIBS))) # */
    APP_LIB_OBJS      = $(patsubst $(APP_LIB_PATH)/%.cpp,$(OBJDIR)/libs/%.o,$(APP_LIB_CPP_SRC))
    APP_LIB_OBJS     += $(patsubst $(APP_LIB_PATH)/%.c,$(OBJDIR)/libs/%.o,$(APP_LIB_C_SRC))
else
    FLAG = 0
endif 

# USER libraries
#
ifndef USER_LIB_PATH
# wildcard required for ~ management
    USER_LIB_PATH     = $(wildcard $(SKETCHBOOK_DIR)/Libraries)
endif

ifndef USER_LIBS_LIST
    s2             = $(realpath $(sort $(dir $(wildcard $(USER_LIB_PATH)/*/*.h)))) # */
    USER_LIBS_LIST    = $(subst $(USER_LIB_PATH)/,,$(filter-out $(EXCLUDE_LIST),$(s2)))
endif

ifneq ($(USER_LIBS_LIST),0)
    USER_LIBS     = $(patsubst %,$(USER_LIB_PATH)/%,$(USER_LIBS_LIST))
    USER_LIB_CPP_SRC = $(wildcard $(patsubst %,%/*.cpp,$(USER_LIBS))) # */
    USER_LIB_C_SRC   = $(wildcard $(patsubst %,%/*.c,$(USER_LIBS))) # */

    USER_OBJS     = $(patsubst $(USER_LIB_PATH)/%.cpp,$(OBJDIR)/libs/%.o,$(USER_LIB_CPP_SRC))
    USER_OBJS    += $(patsubst $(USER_LIB_PATH)/%.c,$(OBJDIR)/libs/%.o,$(USER_LIB_C_SRC))
endif

# LOCAL sources
#
LOCAL_C_SRCS    = $(wildcard *.c)

ifneq ($(strip $(NO_CORE_MAIN_FUNCTION)),)
    LOCAL_CPP_SRCS = $(wildcard *.cpp)
else
    LOCAL_CPP_SRCS = $(filter-out %main.cpp, $(wildcard *.cpp))
endif

LOCAL_CC_SRCS   = $(wildcard *.cc)
#LOCAL_PDE_SRCS  = $(wildcard *.$(SKETCH_EXTENSION))  
LOCAL_AS_SRCS   = $(wildcard *.S)
LOCAL_OBJ_FILES = $(LOCAL_C_SRCS:.c=.o) $(LOCAL_CPP_SRCS:.cpp=.o) \
		$(LOCAL_PDE_SRCS:.$(SKETCH_EXTENSION)=.o) \
		$(LOCAL_CC_SRCS:.cc=.o) $(LOCAL_AS_SRCS:.S=.o)
LOCAL_OBJS      = $(patsubst %,$(OBJDIR)/%,$(LOCAL_OBJ_FILES))


# All the objects
# ??? Does order matter?
#
OBJS    = $(CORE_OBJS) $(BUILD_CORE_OBJS) $(APP_LIB_OBJS) $(BUILD_APP_LIB_OBJS) $(VARIANT_OBJS) $(USER_OBJS) $(LOCAL_OBJS) 

# Dependency files
#
DEPS   = $(LOCAL_OBJS:.o=.d)


# Rules
# ----------------------------------
#

# Main targets
#
TARGET_HEX = $(OBJDIR)/$(TARGET).hex
TARGET_ELF = $(OBJDIR)/$(TARGET).elf
TARGETS    = $(OBJDIR)/$(TARGET).*

# List of dependencies
#
DEP_FILE   = $(OBJDIR)/depends.mk

# Executables
#
REMOVE  = rm -r
MV      = mv -f
CAT     = cat
ECHO    = echo

# General arguments
#
SYS_INCLUDES  = $(patsubst %,-I%,$(APP_LIBS))
SYS_INCLUDES += $(patsubst %,-I%,$(BUILD_APP_LIBS))
SYS_INCLUDES += $(patsubst %,-I%,$(USER_LIBS))

SYS_OBJS      = $(wildcard $(patsubst %,%/*.o,$(APP_LIBS))) # */
SYS_OBJS     += $(wildcard $(patsubst %,%/*.o,$(BUILD_APP_LIBS))) # */
SYS_OBJS     += $(wildcard $(patsubst %,%/*.o,$(USER_LIBS))) # */

CPPFLAGS      = -$(MCU_FLAG_NAME)=$(MCU) -DF_CPU=$(F_CPU) -I. -I$(CORE_LIB_PATH) \
			$(SYS_INCLUDES) -g -Os -w -Wall -ffunction-sections -fdata-sections $(EXTRA_CPPFLAGS)

ifdef USE_GNU99
CFLAGS        = -std=gnu99
endif

CXXFLAGS      = -fno-exceptions
ASFLAGS       = -mmcu=$(MCU) -I. -x assembler-with-cpp
LDFLAGS       = -$(MCU_FLAG_NAME)=$(MCU) -lm -Wl,--gc-sections -Os $(EXTRA_LDFLAGS)


# Implicit rules for building everything (needed to get everything in
# the right directory)
#
# Rather than mess around with VPATH there are quasi-duplicate rules
# here for building e.g. a system C++ file and a local C++
# file. Besides making things simpler now, this would also make it
# easy to change the build options in future


# APPlication library sources
#
$(OBJDIR)/libs/%.o: $(APP_LIB_PATH)/%.cpp
	@echo "1-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
    
$(OBJDIR)/libs/%.o: $(APP_LIB_PATH)/%.c
	@echo "2-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/libs/%.o: $(BUILD_APP_LIB_PATH)/%.cpp
	@echo "1b-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
    
$(OBJDIR)/libs/%.o: $(BUILD_APP_LIB_PATH)/%.c
	@echo "2b-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@


# USER library sources
#
$(OBJDIR)/libs/%.o: $(USER_LIB_PATH)/%.cpp
	@echo "3-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@
    
$(OBJDIR)/libs/%.o: $(USER_LIB_PATH)/%.c
	@echo "4-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

    
# LOCAL sources
# .o rules are for objects, .d for dependency tracking
# 
$(OBJDIR)/%.o: %.c
	@echo "5-" $<
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: %.cc
	@echo "6-" $<
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.o: 	%.cpp
	@echo "7-" $<
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.o: %.S
	@echo "8-"
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@

$(OBJDIR)/%.o: %.s
	@echo "9-" $<
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@

$(OBJDIR)/%.d: %.c
	@echo "10-" $<
	$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(OBJDIR)/%.d: %.cc
	@echo "11-" $<
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(OBJDIR)/%.d: %.cpp
	@echo "12-" $<
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(OBJDIR)/%.d: %.S
	@echo "13-" $<
	$(CC) -MM $(CPPFLAGS) $(ASFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(OBJDIR)/%.d: %.s
	@echo "14-" $<
	$(CC) -MM $(CPPFLAGS) $(ASFLAGS) $< -MF $@ -MT $(@:.d=.o)

# !!!
# the pde -> cpp -> o file
#
$(OBJDIR)/%.cpp: %.$(SKETCH_EXTENSION)
	@echo "pde-" $<
	$(ECHO) $(PDEHEADER) > $@
	$(CAT)  $< >> $@
	$(ECHO) $(PDEHEADER) > $(OBJDIR)/text.txt
	$(CAT)  $< >> $(OBJDIR)/text.txt


$(OBJDIR)/%.o: $(OBJDIR)/%.cpp
	@echo "15-" $<
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.d: $(OBJDIR)/%.cpp
	@echo "16-" $<
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< -MF $@ -MT $(@:.d=.o)


# CORE files
#
$(OBJDIR)/%.o: $(CORE_LIB_PATH)/%.c
	@echo "17-" $<
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: $(CORE_LIB_PATH)/%.cpp
	@echo "18-" $<
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJDIR)/%.o: $(BUILD_CORE_LIB_PATH)/%.c
	@echo "17b-" $<
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o: $(BUILD_CORE_LIB_PATH)/%.cpp
	@echo "18b-" $<
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJDIR)/libs/%.o: $(VARIANT_PATH)/%.cpp
	@echo "18v-" $<
	mkdir -p $(dir $@)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@


# Other object conversions
#
$(OBJDIR)/%.hex: $(OBJDIR)/%.elf
	@echo "19-" $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(OBJDIR)/%.eep: $(OBJDIR)/%.elf
	@echo "20-" $<
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
		--change-section-lma .eeprom=0 -O ihex $< $@

$(OBJDIR)/%.lss: $(OBJDIR)/%.elf
	@echo "21-" $<
	$(OBJDUMP) -h -S $< > $@

$(OBJDIR)/%.sym: $(OBJDIR)/%.elf
	@echo "22-" $<
	$(NM) -n $< > $@


# Size of file
# ----------------------------------
#
HEXSIZE = $(SIZE) --target=ihex $(CURDIR)/$(TARGET_HEX)
ELFSIZE = $(SIZE) $(CURDIR)/$(TARGET_ELF)


# Serial monitoring
# ----------------------------------
#

# First /dev port
#
ifndef SERIAL_PORT
SERIAL_PORT = $(firstword $(wildcard $(BOARD_PORT)))
endif

ifndef SERIAL_BAUDRATE
SERIAL_BAUDRATE = 9600
endif

ifndef SERIAL_COMMAND
SERIAL_COMMAND   = screen
endif


# Info for debugging
# ----------------------------------
#

# Info
#
$(info .    variant		$(VARIANT)) 
ifneq ($(MAKECMDGOALS),boards)
ifneq ($(MAKECMDGOALS),clean)
$(info  ---- info ----)
$(info Board)
$(info .    name		$(call PARSE_BOARD,$(BOARD_TAG),name))
$(info .    f_cpu		$(F_CPU)) 
$(info .    mcu  		$(MCU))
$(info Ports)
$(info .    uploader 	$(UPLOADER))
ifeq ($(UPLOADER),avrdude)
    $(info .    avrdude 	$(AVRDUDE_PORT))
    $(info .    serial		$(SERIAL_PORT))
endif
$(info  ---- info ----)
$(info Core libraries)
$(info .     $(CORE_LIBS_LIST))
ifneq ($(BUILD_CORE_LIBS_LIST),)
    $(info .     $(BUILD_CORE_LIBS_LIST))
endif
$(info Application Arduino/chipKIT/Wiring/Energia libraries)
$(info .     $(APP_LIBS_LIST))
$(info User libraries)
$(info .     $(USER_LIBS_LIST))
$(info  ---- end ----)
endif
endif


# Rules
# ----------------------------------
#
all: 		clean build upload serial
		@echo " ---- all ---- "

build: 		clean compile

make:		changed compile

compile:	$(OBJDIR) $(TARGET_HEX) 		
		@echo " ---- compile ---- "
		@echo $(BOARD_TAG) > $(NEW_TAG)


$(OBJDIR):
		@echo " ---- build ---- "
		mkdir $(OBJDIR)

$(TARGET_ELF): 	$(OBJS)
		@echo "23-" $<
		$(CC) $(LDFLAGS) -o $@ $(OBJS) $(SYS_OBJS) -lc

$(DEP_FILE):	$(OBJDIR) $(DEPS)
		@echo "24-" $<
		@cat $(DEPS) > $(DEP_FILE)

upload:		reset raw_upload
#upload:		reset size raw_upload


raw_upload:	$(TARGET_HEX)
		@echo " ---- upload ---- "
ifeq ($(UPLOADER),avrdude)
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_OPTS) -Uflash:w:$(TARGET_HEX):i
else ifeq ($(UPLOADER),mspdebug)
		$(MSPDEBUG) $(MSPDEBUG_OPTS) "prog $(TARGET_HEX)"
else
		$(error No valid uploader)
endif

# stty on MacOS likes -F, but on Debian it likes -f redirecting
# stdin/out appears to work but generates a spurious error on MacOS at
# least. Perhaps it would be better to just do it in perl ?
reset:
		@echo "---- reset ---- "
ifeq ($(UPLOADER),avrdude)
		-screen -X kill;
		sleep 1;
endif
#		@if [ -z "$(AVRDUDE_PORT)" ]; then \
#			echo "No Arduino-compatible TTY device found -- exiting"; exit 2; \
#			fi
#		for STTYF in 'stty --file' 'stty -f' 'stty <' ; \
#		  do $$STTYF /dev/tty >/dev/null 2>/dev/null && break ; \
#		done ;\
#		$$STTYF $(AVRDUDE_PORT)  hupcl ;\
#		(sleep 0.1 || sleep 1)     ;\
#		$$STTYF $(AVRDUDE_PORT) -hupcl

ispload:	$(TARGET_HEX)
		@echo "---- ispload ---- "
ifeq ($(UPLOADER),avrdude)
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -e \
			-U lock:w:$(ISP_LOCK_FUSE_PRE):m \
			-U hfuse:w:$(ISP_HIGH_FUSE):m \
			-U lfuse:w:$(ISP_LOW_FUSE):m \
			-U efuse:w:$(ISP_EXT_FUSE):m
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -D \
			-U flash:w:$(TARGET_HEX):i
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
			-U lock:w:$(ISP_LOCK_FUSE_POST):m
endif

serial:		reset
		@echo "---- serial ---- "
		osascript -e 'tell application "Terminal" to do script "$(SERIAL_COMMAND) $(SERIAL_PORT) $(SERIAL_BAUDRATE)"'
		

#		echo "$@"
#		echo "-- "
#		export TERM="vt100"
#		echo "#!/bin/sh" /tmp/arduino.command
#		echo "$(SERIAL_COMMAND) $(SERIAL_PORT) $(SERIAL_BAUDRATE)" > /tmp/arduino.command
#		chmod 0755 /tmp/arduino.command
#		open /tmp/arduino.command


size:
		@echo "---- size ---- "
		@if [ -f $(TARGET_HEX) ]; then $(HEXSIZE); echo; fi
		@if [ -f $(TARGET_ELF) ]; then $(ELFSIZE); echo; fi


clean:
		@if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi
		@echo "nil" > $(OBJDIR)/nil
		@echo "---- clean ---- "
		-@rm -r $(OBJDIR)/* # */

changed:
ifeq ($(CHANGE_FLAG),1)
	-$(REMOVE) $(OBJDIR)/* # */
endif

#@echo "---- changed ---- "

#		if [ $(CHANGE_FLAG) == 1 ]; then -$(REMOVE) $(OBJDIR)/*; fi;

depends:	$(DEPS)
		@echo "---- depends ---- "
		@cat $(DEPS) > $(DEP_FILE)


boards:
		@echo "---- boards ---- "
		@if [ -d $(ARDUINO_APP) ]; then echo "---- $(ARDUINO_APP) ---- "; grep .name $(ARDUINO_PATH)/hardware/arduino/boards.txt; echo; fi
		@if [ -d $(MPIDE_APP) ];   then echo "---- $(MPIDE_APP) ---- ";   grep .name $(MPIDE_PATH)/hardware/pic32/boards.txt;     echo; fi
		@if [ -d $(WIRING_APP) ];  then echo "---- $(WIRING_APP) ---- ";  grep .name $(WIRING_PATH)/hardware/Wiring/boards.txt;   echo; fi
		@if [ -d $(ENERGIA_APP) ]; then echo "---- $(ENERGIA_APP) ---- "; grep .name $(ENERGIA_PATH)/hardware/msp430/boards.txt;  echo; fi
		@echo "---- end ---- "

.PHONY:	all clean depends upload raw_upload reset serial show_boards headers size
