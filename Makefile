PROJECT             = avr11
BUILD_DIR           ?= build
APP_BIN             = $(BUILD_DIR)/$(PROJECT)
APP_SOURCES         = avr11.cc \
                      kl11.cc \
					  kb11.cc \
					  kt11.cc \
					  pc11.cc \
					  lp11.cc \
					  kw11.cc \
					  disasm.cc \
					  rk11.cc \
					  unibus.cc 
APP_OBJS            = $(patsubst %.cc,$(BUILD_DIR)/%.o,$(APP_SOURCES))                      
COMMON_CFLAGS       = -g1 -O2 -W -Wall -MMD -Werror -Wextra
CFLAGS              += $(COMMON_CFLAGS)
CXXFLAGS            += $(COMMON_CFLAGS) -std=c++17
DEPS                = $(APP_OBJS:.o=.d)

all: $(APP_BIN)
.PHONY: all

-include $(DEPS)

$(APP_BIN): $(APP_OBJS)
	$(CXX) -o $@ $(APP_OBJS)

$(BUILD_DIR)/%.o: %.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

fmt:
	clang-format -i *.cc *.h
.PHONY: fmt

clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean
