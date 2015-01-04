SRCS=$(wildcard *.cc)
OBJECTS=$(SRCS:.cc=.o)

CSTD=gnu99
COPT=-O2 -fdata-sections -ffunction-sections -Wall -Werror -Wextra
CFLAGS=-std=$(CSTD) $(COPT) 
CFLAGS+=$(addprefix -I,$(INCLUDES))
CFLAGS+=-include "$(SETTINGS)"

CXXSTD=gnu++98
CXXOPT=$(COPT) -fno-exceptions -g
CXXFLAGS=-std=$(CXXSTD) $(CXXOPT) -Wno-format-security -Wno-write-strings
CXXFLAGS+=$(addprefix -I,$(INCLUDES))

#LDFLAGS=-lc -t
LD=clang
#LD=arm-none-eabi-ld
CXX=clang
#CXX=arm-none-eabi-g++

avr11:	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ 

%.o: %.cc avr11.h
	$(CXX) $(CXXFLAGS) $<   -c -o $@

clean:
	rm -rf avr11 $(OBJECTS)

fmt: 
	clang-format-3.5 -i $(SRCS)
