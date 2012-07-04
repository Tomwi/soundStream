#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

ifeq ($(strip $(FEOSSDK)),)
$(error "Please set FEOSSDK in your environment. export FEOSSDK=<path to>FeOS/sdk")
endif

FEOSMK = $(FEOSSDK)/mk
export THIS_MAKEFILE := Makefile

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
TARGET        := sndStreamStub
BUILD         := build
SOURCES       := source ../common/source
DATA          := data
INCLUDES      := include ../common/include

CONF_DEFINES =
CONF_ARM7 = 1

include $(FEOSMK)/app.mk

install: all
	@cp $(TARGET).fx2 $(FEOSDEST)/data/FeOS/arm7/$(TARGET).fx2 || exit 1
