#include <fat32_util.h>
#include <disk_io.h>

void update_next_clus(fat32_t *fs, uint32_t n, uint32_t ent_val)
{
    uint32_t offset = get_clus_fat_offset(fs, n);
    uint32_t sec_num = get_clus_fat_sec(fs, n);
    uint8_t sector[BLOCK_SIZE];
    read_sector(sec_num, sector);
    *((uint32_t *)(sector + offset)) = \
        (*((uint32_t *)(sector + offset)) & 0xF0000000) \
            | (ent_val & 0x0FFFFFFF);
    write_sector(sec_num, sector);
}
