#include <stdint.h>
void sha256_process_arm(uint32_t state[8], const uint8_t data[], uint32_t length);