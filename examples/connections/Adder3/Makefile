#
# Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License")
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Must always use these modes for this test, since it is cycle count
# dependent.
override SIM_MODE=1
override RAND_STALL=0

CC = g++

ifneq ($(SYSTEMC_HOME),)
INCDIR += -I$(SYSTEMC_HOME)/include
LIBDIR += -L$(SYSTEMC_HOME)/lib-linux64
endif

INCDIR += -I$(MGC_HOME)/shared/include
LIBDIR += -L$(MGC_HOME)/shared/lib

CFLAGS ?= 
CFLAGS += -Wall -Wno-unknown-pragmas $(INCDIR) $(LIBDIR)

HLS_CATAPULT ?= 1
ifeq ($(HLS_CATAPULT),1)
  CFLAGS += -DHLS_CATAPULT
endif

LIBS ?=
LIBS += -lsystemc -lpthread

# SIM_MODE
# 0 = Synthesis view of Connections port and combinational code. This option can cause failed simulations due to SystemC's timing model.
# 1 = Cycle accurate view of Connections port and channel code, CONNECTOINS_ACCURATE_SIM. (default)
# 1 = Faster TLM view of Connections port and channel code, CONNECTIONS_FAST_SIM.
SIM_MODE ?= 1
ifeq ($(SIM_MODE),0)
# No flags are added, intentionally blank.
endif
ifeq ($(SIM_MODE),1)
	USER_FLAGS += -DCONNECTIONS_ACCURATE_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES
endif
ifeq ($(SIM_MODE),2)
	USER_FLAGS += -DCONNECTIONS_FAST_SIM -DSC_INCLUDE_DYNAMIC_PROCESSES
endif

# RAND_STALL
# 0 = Random stall of ports and channels disabled (default)
# 1 = Random stall of ports and channels enabled
#
# This feature aids in latency insensitive design verication.
# Note: Only valid if SIM_MODE = 1 (accurate) or 2 (fast)
ifeq ($(RAND_STALL),1)
	$(error Random stall cannot be used with back annotated timing tests)
endif

.PHONY: Build
Build: all

clean:
	rm -rf *.o sim_* dump.vcd *.output.json

all: sim_adder3

run:
	./sim_adder3

sim_adder3: $(wildcard *.h) $(wildcard *.cpp)
	$(CC) -o sim_adder3 $(CFLAGS) $(USER_FLAGS) $(wildcard *.cpp) $(LIBS)

sim_clean:
	rm -rf *.o sim_*

