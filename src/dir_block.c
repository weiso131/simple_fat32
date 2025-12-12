#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <disk_io.h>
#include <fat32_util.h>
#include <dir_block.h>

#include <picos_memory.h>

dir_block_t *create_dir_block(uint32_t entry_offset, uint32_t clus)
{
    dir_block_t *new = malloc(sizeof(dir_block_t));
    new->file_cnt = 0;
    new->attr = 0;
    new->entry_offset = entry_offset;
    new->clus = clus;
    return new;
}

dir_block_t *load_dir(fat32_t *fs, uint32_t clus)
{
    addr_t dir_buf = picos_memory_alloc(BLOCK_SIZE / 64);
    fat32_dir_t *dir_cache = (fat32_dir_t *)picos_cache;

    addr_t fat_buf = picos_memory_alloc(BLOCK_SIZE / 64);
    uint32_t fat_sec = 0;

    dir_block_t *head = create_dir_block(0, clus);
    dir_block_t *now = head;

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
                        
                        memcpy(now->file_lst[now->file_cnt].name + 10, long_name->name2, 12);
                        memcpy(now->file_lst[now->file_cnt].name, long_name->name1, 10);

                        if (block_entry_start == 65535)
                            block_entry_start = entry_offset - now->entry_offset;
                    }
                    if (dir_cache[k].attr & (DIR_ATTR_VOLUME_ID))
                        continue;
                    now->file_lst[now->file_cnt].block_entry_start = block_entry_start;
                    block_entry_start = 65535;
                    now->file_lst[now->file_cnt].block_entry_end = entry_offset - now->entry_offset;
                    now->file_lst[now->file_cnt].file_size = dir_cache[k].file_size;
                    now->file_lst[now->file_cnt].fst_clus = (((uint32_t)dir_cache[k].fst_clus_hi) << 16) \
                                                            | (uint32_t)dir_cache[k].fst_clus_lo;
                    now->file_lst[now->file_cnt].attr = dir_cache[k].attr;
                    now->file_lst[now->file_cnt].dir = now;

                    now->file_cnt++;
                    if (now->file_cnt == FILE_LST_SZ) {
                        dir_block_t *new_block = create_dir_block(entry_offset, clus);
                        now->next = new_block;
                        now->clus = clus;
                        now = new_block;
                    }
                    
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
    now->next = NULL;
    
    return head;
}

void free_dir_block(dir_block_t **dir)
{
    for (dir_block_t *now = *dir;now;now = now->next) {
        for (size_t i = 0;i < now->file_cnt;i++)
            free(now->file_lst[i].name);
        dir_block_t *trash = now;
        now = now->next;
        free(trash);
    }
    *dir = NULL;
}
