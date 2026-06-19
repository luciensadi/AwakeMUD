# Revised from the Awake 2062 Makefile by Chris and Khepri with edits from many others. Thank you all for your hard work over the years!

SHELL = /bin/sh
.DELETE_ON_ERROR:
.PHONY: all clean unittest

# Allow overriding the compiler (e.g., make CXX=g++)
CXX ?= clang++
IS_CLANG = $(shell $(CXX) --version | grep -qi clang && echo yes || echo no)

# These flags are set for all builds.
BASE_CPPFLAGS = -isystem external/ -isystem /usr/local/include/ -std=c++23 -D_GNU_SOURCE -DLOG_COMMANDS -DIDLEDELETE_DRYRUN
BASE_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-vla -Wno-char-subscripts -Wno-format-truncation -ggdb
BASE_LDFLAGS  = -L/usr/local/lib
BASE_LDLIBS   = -lmysqlclient -lz -lm

# Modes: 2 is local dev, 1 is buildport, 0 is mainport. Change:  make DEPLOYMENT_MODE=1
DEPLOYMENT_MODE ?= 2
ifeq ($(DEPLOYMENT_MODE),2)
  BASE_CPPFLAGS += -DIS_BUILDPORT -DSELFADVANCE -DSUPPRESS_MOB_SKILL_ERRORS -DDEBUG -D_GLIBCXX_DEBUG
  JUST_FUCK_ME_UP_WITH_WARNINGS ?= 1
  USE_FSAN ?= 1  # FSAN in address,undefined mode
else ifeq ($(DEPLOYMENT_MODE),1)
  BASE_CPPFLAGS += -DIS_BUILDPORT -DSELFADVANCE -DUSE_MEMORY_CANARIES -DDEBUG -D_GLIBCXX_DEBUG
  JUST_FUCK_ME_UP_WITH_WARNINGS ?= 1
  USE_FSAN ?= 1  # FSAN in address,undefined mode
else
  BASE_CPPFLAGS += -DSUPPRESS_MOB_SKILL_ERRORS
  USE_FSAN ?= 0
endif

# FSAN has various defaults (set above). Force it via command line: make USE_FSAN=1
ifeq ($(USE_FSAN),1)
  FSAN_FLAGS = -fsanitize=address,undefined
  BASE_CXXFLAGS += $(FSAN_FLAGS)
  BASE_LDFLAGS  += $(FSAN_FLAGS)
else ifeq ($(USE_FSAN),2)
  # -fsanitize=memory is only available in Clang, so force that compiler.
  ifneq ($(IS_CLANG), yes)
    echo "Overriding compiler to Clang++ due to -fsanitize=memory being selected."
    CXX = clang++
  endif
  FSAN_FLAGS = -fsanitize=memory,undefined -fsanitize-memory-track-origins
  BASE_CXXFLAGS += $(FSAN_FLAGS)
  BASE_LDFLAGS  += $(FSAN_FLAGS)
endif

# Only set this if you have access to Awake CE's extended world files (not part of the repo)
USE_PRIVATE_WORLD ?= 0
ifeq ($(USE_PRIVATE_WORLD),1)
  BASE_CPPFLAGS += -DUSE_PRIVATE_CE_WORLD
endif

# Crank up the pain. Defaults to true on dev/buildport, false on mainport.
JUST_FUCK_ME_UP_WITH_WARNINGS ?= 0
ifeq ($(JUST_FUCK_ME_UP_WITH_WARNINGS),1)
  # Clang++ lets us use lots of spicy flags, g++ not so much.
  ifeq ($(IS_CLANG),yes)
    BASE_CXXFLAGS += -Wfor-loop-analysis -Wfloat-equal -Wflexible-array-extensions -Wextra-semi \
      -Wexpansion-to-defined -Wenum-compare-conditional -Wempty-init-stmt -Wembedded-directive \
      -Wduplicate-method-arg -Wduplicate-method-match -Wduplicate-enum -Wdouble-promotion -Wdeprecated-redundant-constexpr-static-def \
      -Wdeprecated-implementations -Wdeprecated-dynamic-exception-spec -Wdeprecated -Wdefault-const-init -Wdate-time -Wcovered-switch-default \
      -Wconsumed -Wconditional-uninitialized -Wcast-qual -Wcast-function-type -Wc++-unterminated-string-initialization \
      -Wc++-compat -Wbool-operation -Wbitwise-op-parentheses -Wbitwise-instead-of-logical -Wbad-function-cast \
      -Watomic-properties -Watomic-access -Wassign-enum -Warray-parameter -Warray-bounds-pointer-arithmetic \
      -Waddress-of-temporary -Wabstract-vbase-init \
      -Wno-double-promotion
  else
    BASE_CXXFLAGS += -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict -Wfloat-equal -Wextra-semi -Wexpansion-to-defined -Wdouble-promotion \
      -Wdeprecated -Wdate-time -Wcast-qual -Wcast-function-type -Wbool-operation -Warray-parameter \
      -Wno-double-promotion
  endif
endif

# Auto-detect the OS and apply the unique flags for that config
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)  # OSX
  OS_CPPFLAGS = -Dosx -isystem /opt/homebrew/include/ -DACCELERATE_FOR_TESTING
  OS_LDFLAGS  = -L/opt/homebrew/lib/
  OS_LDLIBS   = -lcurl -lsodium
else ifeq ($(UNAME_S),Linux)  # Linux
  ifeq ($(CI),true)
    OS_CPPFLAGS    = -Dlinux -DNOCRYPT -DADD_SECRET_STUBS
    OS_LDFLAGS     = 
    OS_LDLIBS      = -lcrypt -lnsl -lcurl
		# Suppress all warnings during the CI build process.
    BASE_CXXFLAGS  = -w
  else
    OS_CPPFLAGS = -Dlinux -DDEBUG_CRYPTO -DREIMPLEMENT_STRLCPY_STRLCAT -DNO_CLS -DUSE_MYSQL_8
    OS_LDFLAGS  = 
    OS_LDLIBS   = -lcrypt -lnsl -lcurl -lsodium
  endif
else  # Windows (Cygwin/MinGW)
  OS_CPPFLAGS = -DWIN32 -DREIMPLEMENT_STRLCPY_STRLCAT -D_XOPEN_SOURCE=700 -D_REENTRANT
  OS_LDFLAGS  = 
  OS_LDLIBS   = -lcrypt -lsodium
endif

# Combine base flags with OS-specific flags
CPPFLAGS = $(BASE_CPPFLAGS) $(OS_CPPFLAGS)
CXXFLAGS = $(BASE_CXXFLAGS)
LDFLAGS  = $(BASE_LDFLAGS) $(OS_LDFLAGS)
LDLIBS   = $(BASE_LDLIBS) $(OS_LDLIBS)

# Further setup (identify source files etc)
sourcefiles   = $(wildcard activities/*.cpp *.cpp)

# If secrets.cpp exists, exclude secrets_stub.cpp from the build
ifneq ($(wildcard secrets.cpp),)
  sourcefiles := $(filter-out secrets_stub.cpp,$(sourcefiles))
endif

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
