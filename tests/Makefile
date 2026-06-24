## Host-side unit tests (NOT the Haiku makefile-engine build).
##
## Two suites:
##   * test_runner_management: covers OpenVPNManagement (BeAPI-free; runs on
##     any host with g++, including Linux CI).
##   * test_runner_config: covers OpenVPNConfigParser; touches VPNProfile,
##     which pulls in BString -- Haiku-only.
## The actual Haiku binaries are built from src/ with the makefile-engine.

CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O0 -g

COMMON = ../src/common
BACKEND = ../src/backend
INCLUDES = -I$(COMMON) -I$(BACKEND)

MANAGEMENT_SRCS = \
	test_openvpn_management.cpp \
	$(BACKEND)/OpenVPNManagement.cpp \
	$(COMMON)/VPNState.cpp

CONFIG_SRCS = \
	test_openvpn_config_parser.cpp \
	$(COMMON)/OpenVPNConfigParser.cpp \
	$(COMMON)/VPNProfile.cpp \
	$(COMMON)/VPNState.cpp

CONFIG_LIBS = -lbe

.PHONY: all test clean

all: test

test: test_runner_management test_runner_config
	./test_runner_management
	./test_runner_config

test_runner_management: $(MANAGEMENT_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(MANAGEMENT_SRCS) -o $@

test_runner_config: $(CONFIG_SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CONFIG_SRCS) $(CONFIG_LIBS) -o $@

clean:
	rm -f test_runner_management test_runner_config
