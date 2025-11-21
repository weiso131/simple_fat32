#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <disk_io.h>
#include <fat32_util.h>
#include <dir_block.h>

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
    fat32_dir_t *dir_buf = malloc(BLOCK_SIZE);
    uint8_t *name_stack = (uint8_t *)malloc(LONGEST_NAME_SZ);
    uint8_t *fat_buf = malloc(BLOCK_SIZE);
    uint32_t fat_sec = 0;
    uint16_t nsp = LONGEST_NAME_SZ;

    dir_block_t *head = create_dir_block(0, clus);
    dir_block_t *now = head;

    uint32_t entry_offset = 0;

    while (1) {
        uint32_t first_sec = get_clus_first_sec(fs, clus);
        for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
            read_sector(first_sec + i, dir_buf);
            uint16_t block_entry_start = 65535;
            for (uint32_t j = 0;j < BLOCK_SIZE / sizeof(fat32_dir_t);j++, entry_offset++) {
                
                if (dir_buf[j].short_name[0] == 0x00)
                    goto ls_end;
                if (dir_buf[j].short_name[0] == 0xE5)
                    continue;
                if (dir_buf[j].attr == DIR_ATTR_LONG_NAME) {
                    fat32_long_name_t *long_name = (fat32_long_name_t *)(dir_buf + j);
                    
                    memcpy(name_stack + (nsp - 4), long_name->name3, 4);
                    memcpy(name_stack + (nsp - 16), long_name->name2, 12);
                    memcpy(name_stack + (nsp - 26), long_name->name1, 10);
                    nsp -= 26;
                    if (block_entry_start == 65535)
                        block_entry_start = entry_offset - now->entry_offset;
                }
                if (dir_buf[j].attr & (DIR_ATTR_VOLUME_ID))
                    continue;
                
                
                now->file_lst[now->file_cnt].name = malloc(LONGEST_NAME_SZ - nsp);
                now->file_lst[now->file_cnt].name_sz = LONGEST_NAME_SZ - nsp;
                now->file_lst[now->file_cnt].block_entry_start = block_entry_start;
                block_entry_start = 65535;
                now->file_lst[now->file_cnt].block_entry_end = entry_offset - now->entry_offset;
                now->file_lst[now->file_cnt].file_size = dir_buf[j].file_size;
                now->file_lst[now->file_cnt].fst_clus = (((uint32_t)dir_buf[j].fst_clus_hi) << 16) \
                                                        | (uint32_t)dir_buf[j].fst_clus_lo;
                now->file_lst[now->file_cnt].attr = dir_buf[j].attr;
                now->file_lst[now->file_cnt].dir = now;

                memcpy(now->file_lst[now->file_cnt].name, name_stack + nsp, LONGEST_NAME_SZ - nsp);



                nsp = LONGEST_NAME_SZ;
                now->file_cnt++;
                if (now->file_cnt == FILE_LST_SZ) {
                    printf("clus: %d\n", clus);
                    dir_block_t *new_block = create_dir_block(entry_offset, clus);
                    now->next = new_block;
                    now->clus = clus;
                    now = new_block;
                }
            }
        }
        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            fat_sec = get_clus_fat_sec(fs, clus);
            read_sector(fat_sec, fat_buf);
        }

        clus = get_next_clus(fs, clus, fat_buf);
    }  
ls_end:
    free(dir_buf);
    free(name_stack);
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
