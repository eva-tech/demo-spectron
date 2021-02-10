# Parameters
# SRC_CS: The source C files to compie
# SRC_CPPS: The source CPP files to compile
# EXEC: The executable name

SRC_CPPS = main.cpp

EXEC = demo-spectron

CC                  ?= gcc
CXX                 ?= g++

PUREGEV_ROOT        = /opt/pleora/ebus_sdk/Ubuntu-14.04-x86_64
PV_LIBRARY_PATH      =$(PUREGEV_ROOT)/lib

CFLAGS              += -I$(PUREGEV_ROOT)/include -I.
CPPFLAGS            += -I$(PUREGEV_ROOT)/include -I. 
ifdef _DEBUG
    CFLAGS    += -g -D_DEBUG
    CPPFLAGS  += -g -D_DEBUG
else
    CFLAGS    += -O3
    CPPFLAGS  += -O3
endif
CFLAGS    += -D_UNIX_ -D_LINUX_
CPPFLAGS  += -D_UNIX_ -D_LINUX_

LDFLAGS             += -L$(PUREGEV_ROOT)/lib         \
                        -lPvBase                     \
                        -lPvDevice                   \
                        -lPvBuffer                   \
                        -lPvGenICam                  \
                        -lPvStream                   \
                        -lPvTransmitter              \
                        -lPvVirtualDevice	         \
		                -lPvAppUtils                 \
                        -lPvPersistence              \
                        -lPvSerial


CFLAGS    += -DPV_GUI_NOT_AVAILABLE
CPPFLAGS  += -DPV_GUI_NOT_AVAILABLE

# Add simple imaging lib to the linker options only if available
ifneq ($(wildcard $(PUREGEV_ROOT)/lib/libSimpleImagingLib.so),)
    LDFLAGS   += -lSimpleImagingLib
endif 

# Configure Genicam
GEN_LIB_PATH = $(PUREGEV_ROOT)/lib/genicam/bin/Linux64_x64
LDFLAGS      += -L$(GEN_LIB_PATH)

LD_LIBRARY_PATH       = $(PV_LIBRARY_PATH):$(GEN_LIB_PATH)
export LD_LIBRARY_PATH

OBJS      += $(SRC_CPPS:%.cpp=%.o)
OBJS      += $(SRC_CS:%.c=%.o)

all: $(EXEC)

clean:
	rm -rf $(OBJS) $(EXEC) $(SRC_MOC)

%.o: %.cxx
	$(CXX) -c $(CPPFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) 

.PHONY: all clean
