#---------------------------------------------------------------------------------
# Switch IPTV (Xtream Codes client)
# Based on the standard devkitPro libnx application Makefile template.
#
# Toolchain setup (one-time, on your dev machine):
#   1. Install devkitPro (devkitA64) + libnx: https://devkitpro.org/wiki/Getting_Started
#   2. Install portlibs used here:
#        sudo dkp-pacman -S switch-curl switch-mbedtls switch-json-c switch-zlib
#
#   NOTE: this Makefile builds the menu/API-client shell using libnx's built-in
#   console (text UI) so it has a small, verifiable dependency list. Actual
#   video playback (decoding the stream URLs this app produces) is NOT wired up
#   yet - see the TODO block in source/main.c for what that requires (FFmpeg +
#   a renderer, e.g. SDL2 or deko3d). That's a separate, much larger piece.
#
# Build:
#   export DEVKITPRO=/opt/devkitpro
#   make
#
# Output: switch-iptv.nro  -> copy to /switch/switch-iptv/ on your SD card
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET   : final output name (without extension)
# BUILD    : intermediate build files directory
# SOURCES  : list of directories containing source code
# DATA     : list of directories containing data files
# INCLUDES : list of directories containing header files
# ROMFS    : directory containing data to be added to romfs (fonts, default config, etc.)
#---------------------------------------------------------------------------------
TARGET      :=  switch-iptv
BUILD       :=  build
SOURCES     :=  source
DATA        :=  data
INCLUDES    :=  source
ROMFS       :=  romfs

APP_TITLE   :=  Switch IPTV
APP_AUTHOR  :=  you
APP_VERSION :=  0.1.0

#---------------------------------------------------------------------------------
ARCH    :=  -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS  :=  -g -Wall -O2 -ffunction-sections -D__SWITCH__ $(ARCH)

CFLAGS  +=  $(INCLUDE) -D_GNU_SOURCE

CXXFLAGS :=  $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS :=  -g $(ARCH)
LDFLAGS  =  -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    :=  -lcurl -ljson-c -lmbedtls -lmbedx509 -lmbedcrypto -lz -lnx

#---------------------------------------------------------------------------------
# portlibs + libnx paths
#---------------------------------------------------------------------------------
LIBDIRS :=  $(PORTLIBS_PATH)/switch $(LIBNX)

#---------------------------------------------------------------------------------
# no need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   :=  $(CURDIR)/$(TARGET)
export TOPDIR   :=  $(CURDIR)

export VPATH    :=  $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                    $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  :=  $(CURDIR)/$(BUILD)

CFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD  :=  $(CC)
else
	export LD  :=  $(CXX)
endif

export OFILES_BIN  :=  $(addsuffix .o,$(BINFILES))
export OFILES_SRC  :=  $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES      :=  $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN  :=  $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  :=  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                    -I$(CURDIR)/$(BUILD)

export LIBPATHS :=  $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
	export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile.new

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).pfs0 $(TARGET).nso $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else

DEPENDS :=  $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
all : $(OUTPUT).nro

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro : $(OUTPUT).elf $(OUTPUT).nacp
else
$(OUTPUT).nro : $(OUTPUT).elf
endif

$(OUTPUT).nro : $(if $(wildcard $(TOPDIR)/$(ROMFS)/*),$(TOPDIR)/$(ROMFS))

$(OUTPUT).elf : $(OFILES)

$(OFILES_SRC) : $(HFILES_BIN)

-include $(DEPENDS)

endif
