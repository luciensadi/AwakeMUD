# Revised from the Awake 2062 Makefile by Chris and Khepri with edits from many others. Thank you all for your hard work over the years!

SHELL = /bin/sh
.DELETE_ON_ERROR:
.PHONY: all clean unittest

# Allow overriding the compiler (e.g., make CXX=g++)
CXX ?= clang++

# These flags are set for all builds.
BASE_CPPFLAGS = -I/usr/local/include/ -std=c++20 -DLOG_COMMANDS
BASE_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-vla -ggdb
BASE_LDFLAGS  = -L/usr/local/lib
BASE_LDLIBS   = -lmysqlclient -lz -lm -lboost_filesystem

# Default FSAN to off. Enable via command line: make USE_FSAN=1
USE_FSAN ?= 0
ifeq ($(USE_FSAN),1)
	FSAN_FLAGS = -fsanitize=address -fsanitize=undefined
	BASE_CXXFLAGS += $(FSAN_FLAGS)
	BASE_LDFLAGS  += $(FSAN_FLAGS)
endif

# Default buildport mode to on. Disable via command line: make IS_BUILDPORT=0
IS_BUILDPORT ?= 1
ifeq ($(IS_BUILDPORT),1)
	BASE_CXXFLAGS += -DIS_BUILDPORT -DSELFADVANCE -DUSE_MEMORY_CANARIES
else
	BASE_CXXFLAGS += -DSUPPRESS_MOB_SKILL_ERRORS
endif

# Auto-detect the OS and apply the unique flags for that config
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)  # OSX
	OS_CPPFLAGS = -Dosx -I/opt/homebrew/include/ -DACCELERATE_FOR_TESTING -D_GLIBCXX_DEBUG -DDEBUG -DSUPPRESS_MOB_SKILL_ERRORS
	OS_LDFLAGS  = -L/opt/homebrew/lib/
	OS_LDLIBS   = -lcurl -lsodium
else ifeq ($(UNAME_S),Linux)  # Linux
	ifeq ($(CI),true)
		OS_CPPFLAGS    = -Dlinux -DNOCRYPT
		OS_LDFLAGS     = 
		OS_LDLIBS      = -lcrypt -lnsl -lcurl
		BASE_LDFLAGS  += -lboost_system
		# BASE_CXXFLAGS := $(filter-out -ggdb,$(BASE_CXXFLAGS))
		# We clear the BASE_CXXFLAGS var because we don't actually care about warnings during the CI build process.
		BASE_CXXFLAGS = 
	else
		OS_CPPFLAGS = -Dlinux -DDEBUG_CRYPTO -DIDLEDELETE_DRYRUN
		OS_LDFLAGS  = 
		OS_LDLIBS   = -lcrypt -lnsl -lcurl -lsodium
	endif
else  # Windows (Cygwin/MinGW)
	OS_CPPFLAGS = -DWIN32 -DIDLEDELETE_DRYRUN
	OS_LDFLAGS  = 
	OS_LDLIBS   = -lcrypt -lsodium
endif

# Combine base flags with OS-specific flags
CPPFLAGS = $(BASE_CPPFLAGS) $(OS_CPPFLAGS)
CXXFLAGS = $(BASE_CXXFLAGS)
LDFLAGS  = $(BASE_LDFLAGS) $(OS_LDFLAGS)
LDLIBS   = $(BASE_LDLIBS) $(OS_LDLIBS)

# Further setup (identify source files etc)
sourcefiles   = $(wildcard *.cpp activities/*.cpp)
objfiles      = $(sourcefiles:.cpp=.o)
test_objfiles = $(sourcefiles:.cpp=.test.o) # Separate objects for testing
depfiles      = $(sourcefiles:.cpp=.d) $(sourcefiles:.cpp=.test.d)

# Unittest specific files
unittest_srcs = $(wildcard ../test/*.cpp)
unittest_objs = $(unittest_srcs:.cpp=.o)

# Define build targets etc
all: ../bin/awake

../bin/awake: $(objfiles)
	@mkdir -p ../bin
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(objfiles) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

unittest: ../bin/unittest
	../bin/unittest

../bin/unittest: $(test_objfiles) $(unittest_objs)
	@mkdir -p ../bin
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(test_objfiles) $(unittest_objs) $(LDLIBS)

%.test.o: %.cpp
	$(CXX) $(CPPFLAGS) -DUNITTEST $(CXXFLAGS) -MMD -MP -c $< -o $@

../test/%.o: ../test/%.cpp
	$(CXX) $(CPPFLAGS) -DUNITTEST $(CXXFLAGS) -I../3rdparty/catch2 -MMD -MP -c $< -o $@

clean:
	rm -f $(objfiles) $(test_objfiles) $(depfiles) ../test/*.o ../test/*.d ../bin/awake ../bin/unittest

-include $(depfiles)
-include $(unittest_srcs:.cpp=.d)