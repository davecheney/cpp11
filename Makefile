PROJECT             = avr11
BUILD_DIR           ?= build
APP_BIN             = $(BUILD_DIR)/$(PROJECT)
APP_SOURCES         = avr11.cc \
                      cons.cc \
					  cpu.cc \
					  disasm.cc \
					  mmu.cc \
					  rk11.cc \
					  unibus.cc 
APP_OBJS            = $(patsubst %.cc,$(BUILD_DIR)/%.o,$(APP_SOURCES))                      
COMMON_CFLAGS       = -g -O2 -Wall -MMD -Werror -Wextra
CFLAGS              += $(COMMON_CFLAGS)
CXXFLAGS            += $(COMMON_CFLAGS) -std=c++17
DEPS                = $(APP_OBJS:.o=.d)
# MAKEFLAGS           += -j

all: $(APP_BIN)
.PHONY: all

-include $(DEPS)

$(APP_BIN): $(APP_OBJS)
	$(CXX) -o $@ $(APP_OBJS)

$(BUILD_DIR)/%.o: %.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean
