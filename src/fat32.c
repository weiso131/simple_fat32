#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <disk_io.h>
#include <type.h>
#include <fat32_util.h>
#include <dir_block.h>
#include <picos_memory.h>

unsigned char picos_fat_cache[64];
unsigned char picos_cache[64];
unsigned char dir_block_cache[64];
unsigned char file_cache[64];

fat32_t __fs;

fat32_t *create_fat32()
{
    fat32_t *fs = NULL;
    addr_t sec_buf = picos_memory_alloc(BLOCK_SIZE  / 64);
    picos_read_sector(0, sec_buf);

    extern_memory_read(sec_buf + (512 - 64), picos_cache);
    uint16_t boot_sec_sig = *((uint16_t *)(picos_cache + 62));

    extern_memory_read(sec_buf, picos_cache);
    uint32_t fat_sz32 = *((uint32_t *)(picos_cache + 36));
    
    if (boot_sec_sig != 0xAA55 || fat_sz32 == 0)
        goto NOT_FAT32;

    fs = &__fs;
    fs->byte_per_sec = *((uint16_t *)(picos_cache + 11));
    fs->sec_per_clus = picos_cache[13];
    fs->first_fat_sec = *((uint16_t *)(picos_cache + 14));
    uint8_t num_fats = picos_cache[16];
    
    fs->first_data_sec = fs->first_fat_sec + num_fats * fat_sz32;

    fs->root_clus = *((uint32_t *)(picos_cache + 44));
    fs->fs_info = *((uint16_t *)(picos_cache + 48));


    picos_read_sector(fs->fs_info, sec_buf);

    extern_memory_read(sec_buf + 448, picos_cache);

    fs->fsi_free_cnt = *((uint32_t *)(picos_cache + 40));
    fs->fsi_nxt_free = *((uint32_t *)(picos_cache + 44));

NOT_FAT32:
    picos_memory_release(sec_buf);
    return fs;
}

#define print_file_name(j, file) \
    do {\
        for (uint16_t j = 0;j < LONGEST_NAME_SZ && (file).name[j] != 0xFF;j++)\
            if (is_valid_ascii((file).name[j]))\
                printf("%c", (file).name[j]);\
        printf("\n");\
    } while(0)
    

void ls_dir(addr_t dir)
{
    while (1) {
        if (dir == EXTERN_NULL)
            break;
        extern_memory_read(dir, dir_block_cache);
        print_file_name(j, ((dir_block_t *)dir_block_cache)->file);
        dir = (addr_t)(((dir_block_t *)dir_block_cache)->next);
    }
        
}

addr_t find_file(addr_t dir, const char *file_name, uint8_t name_size)
{
    addr_t target = 0;
    while (1) {
        if (dir == EXTERN_NULL)
            break;
        extern_memory_read(dir, dir_block_cache);
        int result = memcmp(((dir_block_t *)dir_block_cache)->file.name, file_name, name_size);
        if (!result) {
            target = dir;
            goto find_file;
        }
        dir = (addr_t)(((dir_block_t *)dir_block_cache)->next);
    }

find_file:
    return target;
}

#define min(x, y) ((x < y) ? x : y)

void read_file(fat32_t *fs, file_t *file, addr_t read_extern_buf, size_t cnt)
{
    if (file->file_size == 0)
        return;
    if ((cnt & 0x1ff) != 0)
        return;

    addr_t fat_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
    addr_t tmp_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
    uint32_t clus = file->fst_clus;
    uint32_t fat_sec = 0;

    while (clus < END_OF_CLUS) {
        uint32_t first_sec = get_clus_first_sec(fs, clus);
        
        for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
            picos_read_sector(first_sec + i, tmp_extern_buf);
            for (uint16_t j = 0;j < BLOCK_SIZE;j += 64, read_extern_buf += 64) {
                extern_memory_read(tmp_extern_buf + j, picos_cache);
                extern_memory_write(read_extern_buf, picos_cache);
            }
            cnt -= BLOCK_SIZE;
            if (cnt == 0) 
                goto end_read;
        }
        
        if (((fs)->first_fat_sec + (clus * 4) / (fs)->byte_per_sec) != fat_sec) {
            fat_sec = ((fs)->first_fat_sec + (clus * 4) / (fs)->byte_per_sec);
            picos_read_sector(fat_sec, fat_extern_buf);
        }

        extern_memory_read(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache); //maybe can reduce memory read

        clus = *((uint32_t *)(picos_fat_cache + (((clus * 4) % (fs)->byte_per_sec) % 64))) & 0x0FFFFFFF;

    }
end_read:
    picos_memory_release(tmp_extern_buf);
    picos_memory_release(fat_extern_buf);
    return;
}

/* check if fsi_nxt_free is used, if still free do nothing, is used find new fsi_nxt_free */
void update_new_fsi_nxt_free(fat32_t *fs, addr_t fat_extern_buf, uint32_t fat_sec)
{
    char flag = 0;
    // if has fat_buf, use it

    if ((fat_extern_buf != EXTERN_NULL) || (get_clus_fat_sec(fs, fs->fsi_nxt_free) != fat_sec)) {
        fat_sec = get_clus_fat_sec(fs, fs->fsi_nxt_free);
        fat_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
        picos_read_sector(fat_sec, fat_extern_buf);
        flag = 1;
    }

    extern_memory_read(fat_extern_buf + ((((fs->fsi_nxt_free * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);

    if (get_next_clus(fs, fs->fsi_nxt_free, picos_fat_cache) == 0) 
        goto fsi_nxt_free_still_free;

    fs->fsi_nxt_free++;

    while (get_next_clus(fs, fs->fsi_nxt_free, picos_fat_cache) != 0) {
        fs->fsi_nxt_free++;
        if (((fs)->first_fat_sec + (fs->fsi_nxt_free * 4) / (fs)->byte_per_sec) != fat_sec) {
            fat_sec = ((fs)->first_fat_sec + (fs->fsi_nxt_free * 4) / (fs)->byte_per_sec);
            picos_read_sector(fat_sec, fat_extern_buf);
        }
        extern_memory_read(fat_extern_buf + ((((fs->fsi_nxt_free * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
    }
    fs->fsi_free_cnt--;

fsi_nxt_free_still_free:
    if (flag)
        picos_memory_release(fat_extern_buf);

}

void write_file(fat32_t *fs, file_t *file, const void *buf, size_t size)
{
    uint32_t clus = file->fst_clus, last_clus = file->fst_clus;
    addr_t fat_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
    addr_t tmp_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
    uint32_t fat_sec = get_clus_fat_sec(fs, clus);
    picos_read_sector(fat_sec, fat_extern_buf);
    extern_memory_read(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
    uint32_t write_cnt = 0;

    file->attr |= DIR_ATTR_DIRTY;

    file->file_size = size;

    while (clus < END_OF_CLUS) {
        uint32_t next_clus = get_next_clus(fs, clus, picos_fat_cache);
        if (write_cnt < size) {
            uint32_t first_sec = get_clus_first_sec(fs, clus);
            for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
                for (uint16_t j = write_cnt;j < (write_cnt + BLOCK_SIZE) && j < size;j += 64) {
                    memcpy(picos_cache, buf + j, min(64, size - j));
                    extern_memory_write(tmp_extern_buf + j - write_cnt, picos_cache);
                }
                picos_write_sector(first_sec + i, tmp_extern_buf);
                write_cnt += BLOCK_SIZE;
                if (write_cnt >= size) {
                    // update fat buffer to END_OF_CLUS
                    set_next_clus(fs, clus, picos_fat_cache, END_OF_CLUS);
                    break;
                }
            }
        } else {
            set_next_clus(fs, clus, picos_fat_cache, 0);
            fs->fsi_free_cnt++;
            fs->fsi_nxt_free = min(fs->fsi_nxt_free, clus);
        }

        // maybe can reduce write
        extern_memory_write(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            // write back the FAT
            if (write_cnt >= size)
                picos_write_sector(fat_sec, fat_extern_buf);
            fat_sec = get_clus_fat_sec(fs, clus);
            picos_read_sector(fat_sec, fat_extern_buf);
            extern_memory_read(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
        }
        last_clus = clus;
        clus = next_clus;

    }

    clus = last_clus;
    while (write_cnt < size) {
        // read/write maybe can reduce
        extern_memory_read(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
        set_next_clus(fs, clus, picos_fat_cache, fs->fsi_nxt_free);
        set_next_clus(fs, fs->fsi_nxt_free, picos_fat_cache, END_OF_CLUS);
        extern_memory_write(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
        clus = fs->fsi_nxt_free;
        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            // write back the FAT
            picos_write_sector(fat_sec, fat_extern_buf);
            fat_sec = get_clus_fat_sec(fs, clus);
            picos_read_sector(fat_sec, fat_extern_buf);
        }
        update_new_fsi_nxt_free(fs, fat_extern_buf, fat_sec);
        uint32_t first_sec = get_clus_first_sec(fs, clus);
        for (uint8_t i = 0;i < fs->sec_per_clus && write_cnt < size;i++, write_cnt += BLOCK_SIZE) {
            for (uint16_t j = write_cnt;j < (write_cnt + BLOCK_SIZE) && j < size;j += 64) {
                memcpy(picos_cache, buf + j, min(64, size - j));
                extern_memory_write(tmp_extern_buf + j - write_cnt, picos_cache);
            }
            picos_write_sector(first_sec + i, tmp_extern_buf);
            
                
        }
    }
    picos_write_sector(fat_sec, fat_extern_buf);

    picos_memory_release(fat_extern_buf);
    picos_memory_release(tmp_extern_buf);
    return;
}

void dir_update(fat32_t *fs, addr_t dir)
{
    addr_t fat_extern_buf = picos_memory_alloc(BLOCK_SIZE >> 6);
    addr_t fat32_dir_buf = picos_memory_alloc(BLOCK_SIZE >> 6);

    uint32_t fat_sec = -1;
    uint32_t dir_sec = 0;
    while (1) {
        if (dir == EXTERN_NULL)
            break;
        extern_memory_read(dir, dir_block_cache);
        
        if (!(((dir_block_t *)dir_block_cache)->file.attr & DIR_ATTR_DIRTY))
            goto next_dir;
        if (!(((dir_block_t *)dir_block_cache)->file.attr & DIR_ATTR_NAME_CHANGE)) {
            uint32_t x = ((dir_block_t *)dir_block_cache)->file.block_entry_end;
            uint32_t clus_num = x / (BLOCK_SIZE / 32 * fs->sec_per_clus);
            uint8_t sec = (x / (BLOCK_SIZE / 32)) % fs->sec_per_clus;
            uint16_t dir_num = (x / fs->sec_per_clus) % (BLOCK_SIZE / 32);
            uint32_t clus = ((dir_block_t *)dir_block_cache)->clus;

            for (uint32_t j = 0;j < clus_num;j++) {
                if (((fs)->first_fat_sec + (clus * 4) / (fs)->byte_per_sec) != fat_sec) {
                    fat_sec = ((fs)->first_fat_sec + (clus * 4) / (fs)->byte_per_sec);
                    picos_read_sector(fat_sec, fat_extern_buf);
                }
                extern_memory_read(fat_extern_buf + ((((clus * 4) % (fs)->byte_per_sec) / 64) * 64), picos_fat_cache);
                clus = get_next_clus(fs, clus, picos_fat_cache);
            }
            if (dir_sec != get_clus_first_sec(fs, clus) + sec) {
                if (dir_sec)
                    picos_write_sector(dir_sec, fat32_dir_buf);
                dir_sec = get_clus_first_sec(fs, clus) + sec;
                picos_read_sector(dir_sec, fat32_dir_buf);

                /* update dir block by 64 byte access */
                extern_memory_read(fat32_dir_buf + ((dir_num & 0xFFFE) << 5), picos_cache);
                ((fat32_dir_t *)picos_cache)[dir_num & 1].file_size = ((dir_block_t *)dir_block_cache)->file.file_size;
                extern_memory_write(fat32_dir_buf + ((dir_num & 0xFFFE) << 5), picos_cache);
            }
        }
next_dir:
        dir = (addr_t)(((dir_block_t *)dir_block_cache)->next);
    }
    if (dir_sec)
        picos_write_sector(dir_sec, fat32_dir_buf);
    picos_memory_release(fat32_dir_buf);
    picos_memory_release(fat_extern_buf);
}

void release_fat32(fat32_t **fs)
{
    // write back fsinfo
    uint8_t *sec_buf = malloc(BLOCK_SIZE);
    *((uint32_t *)(sec_buf + 488)) = (*fs)->fsi_free_cnt;
    *((uint32_t *)(sec_buf + 492)) = (*fs)->fsi_nxt_free;
    write_sector((*fs)->fs_info, sec_buf);
    free(sec_buf);
}

const char text[] = "A teddy bear, or simply a teddy, is a stuffed toy in the form of a bear. \n"
                  "The teddy bear was named by Morris Michtom after the 26th president of the United States, \n"
                  "Theodore Roosevelt; it was developed apparently simultaneously in the first decade of the 20th \n"
                  "century by two toymakers: Richard Steiff in Germany and Michtom in the United States. \n"
                  "It became a popular children's toy, and it has been celebrated in story, song, and film. \n"
                  "Since the creation of the first teddy bears (which sought to imitate the form of real bear cubs), \n"
                  "teddies have greatly varied in form, style, color, and material. \n"
                  "They have become collector's items, with older and rarer teddies appearing at public auctions.\n"
                  "[2] Teddy bears are among the most popular gifts for children, and they are often given to\n"
                  "adults to signify affection, congratulations, or sympathy. ";

int main()
{
    mount_disk("disk.bin");

    fat32_t *fs = create_fat32();
    addr_t root = load_dir(fs, fs->root_clus);

    ls_dir(root);

    char target_name[] = "meow.txt";

    addr_t target_addr = find_file(root, target_name, sizeof(target_name));
    extern_memory_read(target_addr, file_cache);
    file_t *target = (file_t *)file_cache;

    printf("%d\n", target->file_size);

    if (target)
        print_file_name(j, *target);
    
    uint32_t target_size = ((sizeof(text) + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    addr_t target_buf = picos_memory_alloc(target_size / 64); // this is gerneral memory region

    read_file(fs, target, target_buf, target_size);

    for (uint32_t i = 0;i < target_size;i += 64) {
        extern_memory_read(target_buf + i, picos_cache);
        for (uint32_t j = 0;j < 64;j++) {
            if (i + j == target->file_size)
                goto end_cat;
            printf("%c", picos_cache[j]);
        }
            
    }
end_cat:
    printf("\n");



    write_file(fs, target, text, sizeof(text));
    extern_memory_write(target_addr, file_cache);

    dir_update(fs, root);   

    release_fat32(&fs);
    unmount_disk();

    free_dir_block(root);

}
