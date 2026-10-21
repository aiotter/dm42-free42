# Give each suite its own target so make -j can build and run suites concurrently.
.DEFAULT_GOAL := check
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD_DIR ?= /tmp/free42-tests
override BUILD_DIR := $(abspath $(BUILD_DIR))
DECIMAL_LIBRARY ?= $(ROOT)/gtk/gcc111libbid.a
DECIMAL_INCLUDE ?= $(ROOT)/inteldecimal/LIBRARY/src
SANITIZE ?= 0
SUITES ?= $(basename $(notdir $(wildcard $(ROOT)/tests/*_test.cc)))

TEST_CPPFLAGS := -DBCD_MATH -DDECIMAL_CALL_BY_REFERENCE=1 \
  -DDECIMAL_GLOBAL_ROUNDING=1 -DDECIMAL_GLOBAL_ROUNDING_ACCESS_FUNCTIONS=1 \
  -DDECIMAL_GLOBAL_EXCEPTION_FLAGS=1 -DDECIMAL_GLOBAL_EXCEPTION_FLAGS_ACCESS_FUNCTIONS=1 \
  -D_WCHAR_T_DEFINED -I$(ROOT)/common -I$(ROOT)/tests -I$(DECIMAL_INCLUDE)
TEST_CXXFLAGS := -std=c++11 -O1 -g -UNDEBUG
ifeq ($(SANITIZE),1)
TEST_CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
endif
ASAN_OPTIONS ?= halt_on_error=1:abort_on_error=1:detect_stack_use_after_return=1

CORE_SOURCES := $(wildcard $(ROOT)/common/core_*.cc) $(ROOT)/common/shell_spool.cc
CORE_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(CORE_SOURCES:.cc=.o)))
TEST_BINARIES := $(addprefix $(BUILD_DIR)/,$(SUITES))
CHECK_TARGETS := $(addprefix check-,$(SUITES))

.PHONY: all check clean $(CHECK_TARGETS)
.SECONDARY: $(CORE_OBJECTS)
all: $(TEST_BINARIES)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/%.o: $(ROOT)/common/%.cc $(ROOT)/tests/Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(TEST_CPPFLAGS) $(CXXFLAGS) $(TEST_CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/%: $(ROOT)/tests/%.cc $(CORE_OBJECTS)
	$(CXX) $(CPPFLAGS) $(TEST_CPPFLAGS) $(CXXFLAGS) $(TEST_CXXFLAGS) "$<" $(CORE_OBJECTS) $(LDFLAGS) "$(DECIMAL_LIBRARY)" -o "$@"

check: $(CHECK_TARGETS)

$(CHECK_TARGETS): check-%: $(BUILD_DIR)/%
	@echo "RUN $*"
	@cd "$(BUILD_DIR)" && ASAN_OPTIONS="$(ASAN_OPTIONS)" "./$*"

clean:
	rm -f $(CORE_OBJECTS) $(CORE_OBJECTS:.o=.d) $(TEST_BINARIES)

-include $(CORE_OBJECTS:.o=.d)
