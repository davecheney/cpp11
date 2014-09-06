SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

CSTD=gnu99
COPT=-O2 -fdata-sections -ffunction-sections
CFLAGS=-std=$(CSTD) $(COPT) -Wall
CFLAGS+=$(addprefix -I,$(INCLUDES))
CFLAGS+=-include "$(SETTINGS)"

CXXSTD=gnu++98
CXXOPT=$(COPT) -fno-exceptions -g
CXXFLAGS=-std=$(CXXSTD) $(CXXOPT) -Wall -Werror -Wno-write-strings -Wno-format-security
CXXFLAGS+=$(addprefix -I,$(INCLUDES))

#LDFLAGS=-lc -t
LD=clang
CXX=clang

avr11:	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ 

%.o: %.cc avr11.h
	$(CXX) $(CXXFLAGS) $<   -c -o $@

clean:
	rm -rf avr11 $(OBJECTS)

