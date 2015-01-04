SRCS=$(wildcard *.cc)
OBJECTS=$(SRCS:.cc=.o)

CSTD=gnu99
COPT=-O2 -fdata-sections -ffunction-sections -Wall -Werror -Wextra
CFLAGS=-std=$(CSTD) $(COPT) 
CFLAGS+=$(addprefix -I,$(INCLUDES))
CFLAGS+=-include "$(SETTINGS)"

CXXSTD=c++11
CXXOPT=$(COPT) -fno-exceptions -g
CXXFLAGS=-std=$(CXXSTD) $(CXXOPT) -Wno-format-security
CXXFLAGS+=$(addprefix -I,$(INCLUDES))

LD=clang
CXX=$(LD)

avr11:	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ 

%.o: %.cc avr11.h
	$(CXX) $(CXXFLAGS) $<   -c -o $@

clean:
	rm -rf avr11 $(OBJECTS)

fmt: 
	clang-format-3.5 -i $(SRCS)
