## Host-side unit tests (NOT the Haiku makefile-engine build).
##
## These cover the BeAPI-free protocol logic (the OpenVPN management-interface
## parser) and run on any host with g++ -- including during CI on Linux. The
## actual Haiku binaries are built from src/ with the makefile-engine.

CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O0 -g

COMMON = ../src/common
BACKEND = ../src/backend
INCLUDES = -I$(COMMON) -I$(BACKEND)

SRCS = \
	test_openvpn_management.cpp \
	$(BACKEND)/OpenVPNManagement.cpp \
	$(COMMON)/VPNState.cpp

.PHONY: all test clean

all: test

test: test_runner
	./test_runner

test_runner: $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o test_runner

clean:
	rm -f test_runner
