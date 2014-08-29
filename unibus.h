
enum {
	MEMSIZE = 1<<18
};

namespace unibus {
  
    // operations on uint32_t types are insanely expensive
    union addr {
     uint8_t  bytes[4];
     uint32_t value;
    };
    void init();
  
    uint16_t read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    void write8(uint32_t a, uint16_t v);
    void write16(uint32_t a, uint16_t v);
};

