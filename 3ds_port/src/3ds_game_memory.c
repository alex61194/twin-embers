/* FireRed's original linker script provides gHeap; ARM11 needs real storage. */
#include <stdint.h>

uint8_t gHeap[0x1c000] __attribute__((aligned(4)));
