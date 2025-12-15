#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <disk_io.h>
#include <fat32_util.h>
#include <dir_block.h>

dir_block_t *load_dir(fat32_t *fs, uint32_t clus)
{
    addr_t dir_buf = picos_memory_alloc(BLOCK_SIZE / 64);
    fat32_dir_t *dir_cache = (fat32_dir_t *)picos_cache;

    addr_t fat_buf = picos_memory_alloc(BLOCK_SIZE / 64);
    uint32_t fat_sec = 0;

    /* create new dir_block */
    addr_t now_addr = picos_memory_alloc(1);// this is gerneral memory region
    extern_memory_read(now_addr, dir_block_cache);
    dir_block_t *now = (dir_block_t *)dir_block_cache;
    now->entry_offset = 0;
    now->clus = clus;

    addr_t head = now_addr;

    uint32_t entry_offset = 0;

    while (1) {
        uint32_t first_sec = get_clus_first_sec(fs, clus);
        for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
            picos_read_sector(first_sec + i, dir_buf);
            uint16_t block_entry_start = 65535;
            for (uint8_t j = 0;j < BLOCK_SIZE / sizeof(fat32_dir_t);j += 2) {

                extern_memory_read(dir_buf + j * 32, picos_cache);

                for (uint8_t k = 0;k < 2;k++, entry_offset++) {
                    if (dir_cache[k].short_name[0] == 0x00)
                        goto ls_end;
                    if (dir_cache[k].short_name[0] == 0xE5)
                        continue;
                    if (dir_cache[k].attr == DIR_ATTR_LONG_NAME) {
                        fat32_long_name_t *long_name = (fat32_long_name_t *)(dir_cache + k);
                        
                        uint8_t x = 0;
                        for (uint8_t name_idx = 0;name_idx < 10;name_idx++) {
                            if (is_valid_ascii(long_name->name1[name_idx])) {
                                now->file.name[x] = long_name->name1[name_idx];
                                x++;
                            }
                        }
                        for (uint8_t name_idx = 0;name_idx < 12;name_idx++) {
                            if (is_valid_ascii(long_name->name2[name_idx])) {
                                now->file.name[x] = long_name->name2[name_idx];
                                x++;
                            }
                        }

                        if (block_entry_start == 65535)
                            block_entry_start = entry_offset - now->entry_offset;
                    }
                    if (dir_cache[k].attr & (DIR_ATTR_VOLUME_ID))
                        continue;
                    now->file.block_entry_start = block_entry_start;
                    block_entry_start = 65535;
                    now->file.block_entry_end = entry_offset - now->entry_offset;
                    now->file.file_size = dir_cache[k].file_size;
                    now->file.fst_clus = (((uint32_t)dir_cache[k].fst_clus_hi) << 16) \
                                                            | (uint32_t)dir_cache[k].fst_clus_lo;
                    now->file.attr = dir_cache[k].attr;
                    now->clus = clus;

                    addr_t new_block = picos_memory_alloc(1);// this is gerneral memory region
                    now->next = (dir_block_t *)new_block;

                    extern_memory_write(now_addr, (uint8_t *)now);

                    extern_memory_read(new_block, (uint8_t *)now);
                    now_addr = new_block;
                    now->entry_offset = entry_offset;
                    now->clus = clus;
                    
                }
            }
        }
        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            fat_sec = get_clus_fat_sec(fs, clus);
            picos_read_sector(fat_sec, fat_buf);
        }

        uint32_t clus_offset = (clus * 4) % (fs)->byte_per_sec;// can expand

        extern_memory_read(fat_buf + ((clus_offset / 64) * 64), picos_fat_cache); //maybe can reduce memory read


        clus = *((uint32_t *)(picos_fat_cache + (clus_offset % 64))) & 0x0FFFFFFF;
    }  
ls_end:
    picos_memory_release(dir_buf);
    picos_memory_release(fat_buf);
    now->next = EXTERN_NULL;
    extern_memory_write(now_addr, (uint8_t *)now);
    
    return (dir_block_t *)head;
}

void free_dir_block(addr_t dir)
{
    while (1) {
        if (dir == EXTERN_NULL)
            break;
        extern_memory_read(dir, dir_block_cache);
        picos_memory_release(dir);
        dir = (addr_t)(((dir_block_t *)dir_block_cache)->next);
        
    }
}
