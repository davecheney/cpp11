SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

avr11: $(OBJECTS)
	$(LD) $(LDFLAGS) $^ -lm -lc  -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) $<   -c -o $@

clean:
	rm -rf avr11 $(OBJECTS)

