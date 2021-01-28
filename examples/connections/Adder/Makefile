# Makefile for example Adder

CXXFLAGS += -g -std=c++11 -Wall -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-label

# SIM_MODE
# 0 = Synthesis view of Connections port and combinational code. This option can cause failed simulations due to SystemC's timing model.
# 1 = Cycle accurate view of Connections port and channel code, CONNECTOINS_ACCURATE_SIM. (default)
# 1 = Faster TLM view of Connections port and channel code, CONNECTIONS_FAST_SIM.
SIM_MODE ?= 1
ifeq ($(SIM_MODE),0)
# No flags are added, intentionally blank.
endif
ifeq ($(SIM_MODE),1)
	USER_FLAGS += -DCONNECTIONS_ACCURATE_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES -DCONNECTIONS_NAMING_ORIGINAL
endif
ifeq ($(SIM_MODE),2)
	USER_FLAGS += -DCONNECTIONS_FAST_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES -DCONNECTIONS_NAMING_ORIGINAL
endif

# RAND_STALL
# 0 = Random stall of ports and channels disabled (default)
# 1 = Random stall of ports and channels enabled
#
# This feature aids in latency insensitive design verication.
# Note: Only valid if SIM_MODE = 1 (accurate) or 2 (fast)
ifeq ($(RAND_STALL),1)
	USER_FLAGS += -DCONN_RAND_STALL
endif

# =====================================================================
# ENVIRONMENT VARIABLES
#
# The following environment variables will specify paths
# to open-source repositories that are also included in
# a Catapult install tree.
# If you are using Catapult (i.e. if CATAPULT_HOME or MGC_HOME is set)
# then you do not need to define these environment variables.
# If, however, you wish to point to your own github clone
# of any of these repositories, then define the appropriate
# environment variable.

# If CATAPULT_HOME not set, use value of MGC_HOME for backward compatibility.
CATAPULT_HOME ?= $(MGC_HOME)

ifneq "$(CATAPULT_HOME)" ""

# Pick up SystemC via "SYSTEMC_HOME"
SYSTEMC_HOME ?= $(CATAPULT_HOME)/shared

# Pick up Connections via "CONNECTIONS_HOME"
CONNECTIONS_HOME ?= $(CATAPULT_HOME)/shared

# Pick up AC Simutils via "AC_SIMUTILS"
AC_SIMUTILS ?= $(CATAPULT_HOME)/shared

# Pick up C++ compiler
CXX := $(CATAPULT_HOME)/bin/g++
LD_LIBRARY_PATH := $(if $(LD_LIBRARY_PATH),$(LD_LIBRARY_PATH):)$(CATAPULT_HOME)/lib

else

# CATAPULT_HOME appears to not be set. Make sure required variables are defined

ifndef SYSTEMC_HOME
$(error - Environment variable SYSTEMC_HOME must be defined)
endif
ifndef CONNECTIONS_HOME
$(error - Environment variable CONNECTIONS_HOME must be defined)
endif
ifndef AC_SIMUTILS
$(error - Environment variable AC_SIMUTILS must be defined)
endif

endif

# ---------------------------------------------------------------------

# Check: $(SYSTEMC_HOME)/include/systemc.h must exist
checkvar_SYSTEMC_HOME: $(SYSTEMC_HOME)/include/systemc.h

# Check: $(CONNECTIONS_HOME)/include/connections/connections.h must exist
checkvar_CONNECTIONS_HOME: $(CONNECTIONS_HOME)/include/connections/connections.h

# Check: $(AC_SIMUTILS)/include/mc_scverify.h
checkvar_AC_SIMUTILS: $(AC_SIMUTILS)/include/mc_scverify.h

# Rule to check that environment variables are set correctly
checkvars: checkvar_SYSTEMC_HOME checkvar_CONNECTIONS_HOME checkvar_AC_SIMUTILS
# =====================================================================

# Determine the director containing the source files from the path to this Makefile
SOURCE_DIR = $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

INCDIRS := -I$(SOURCE_DIR)
INCDIRS += -I$(SYSTEMC_HOME)/include
INCDIRS += -I$(CONNECTIONS_HOME)/include
INCDIRS += -I$(AC_SIMUTILS)/include

CPPFLAGS += $(INCDIRS)
CPPFLAGS += $(USER_FLAGS)

SYSC_LIBDIRS := $(strip $(foreach ldir,lib-linux64 lib-linux lib,$(wildcard $(SYSTEMC_HOME)/$(ldir))))
LIBDIRS += $(foreach ldir,$(SYSC_LIBDIRS),-L$(ldir))
LIBS += -lsystemc -lpthread
LD_LIBRARY_PATH := $(if $(LD_LIBRARY_PATH),$(LD_LIBRARY_PATH):)$(subst $(eval) ,:,$(SYSC_LIBDIRS))
export LD_LIBRARY_PATH

.PHONY: all build run clean sim_clean help
.DEFAULT_GOAL := all
all: run

build: checkvars sim_sc

run: build
	./sim_sc

sim_sc: $(wildcard $(SOURCE_DIR)*.h) $(wildcard $(SOURCE_DIR)*.cpp)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LIBDIRS) $(wildcard $(SOURCE_DIR)*.cpp) -o $@ $(LIBS)

clean:
	rm -rf *.o sim_* dump.vcd

help:
	-@echo "Makefile targets:"
	-@echo "  clean     - Clean up from previous make runs"
	-@echo "  all       - Perform all of the targets below"
	-@echo "  build     - Compile SystemC design"
	-@echo "  run       - Execute SystemC design"
	-@echo ""
	-@echo "  SOURCE_DIR         = $(SOURCE_DIR)"
	-@echo ""
	-@echo "Environment/Makefile Variables:"
	-@echo "  CATAPULT_HOME      = $(CATAPULT_HOME)"
	-@echo "  SYSTEMC_HOME       = $(SYSTEMC_HOME)"
	-@echo "  CONNECTIONS_HOME   = $(CONNECTIONS_HOME)"
	-@echo "  AC_SIMUTILS        = $(AC_SIMUTILS)"
	-@echo "  CXX                = $(CXX)"
	-@echo "  LIBDIRS            = $(LIBDIRS)"
	-@echo "  LD_LIBRARY_PATH    = $(LD_LIBRARY_PATH)"
	-@echo ""

