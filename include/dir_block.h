#include <type.h>
#include <picos_memory.h>

addr_t load_dir(fat32_t *fs, uint32_t clus);

void free_dir_block(addr_t dir);
