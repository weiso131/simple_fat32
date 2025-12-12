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
    

void ls_dir(dir_block_t *dir)
{
    for (dir_block_t *now = dir;now;now = now->next)
        print_file_name(j, now->file);
}

file_t *find_file(dir_block_t *dir, const char *file_name, uint8_t name_size)
{

    file_t *target = NULL;

    for (dir_block_t *now = dir;now;now = now->next) {
        int result = memcmp(now->file.name, file_name, name_size);
        if (!result) {
            target = &now->file;
            goto find_file;
        }
    }
find_file:
    return target;
}

#define min(x, y) ((x < y) ? x : y)

#define update_fat_read(fs, clus, fat_sec, fat_buf) \
    do {\
        if (get_clus_fat_sec(fs, clus) != fat_sec) {\
            fat_sec = get_clus_fat_sec(fs, clus);\
            read_sector(fat_sec, fat_buf);\
        }\
    } while (0)

void read_file(fat32_t *fs, file_t *file, void *buf, size_t cnt)
{
    if (file->file_size == 0)
        return;

    uint8_t *read_block = malloc(BLOCK_SIZE);
    uint8_t *fat_buf = malloc(BLOCK_SIZE);
    uint32_t clus = file->fst_clus;
    uint32_t fat_sec = 0;

    while (clus < END_OF_CLUS) {
        uint32_t first_sec = get_clus_first_sec(fs, clus);

        for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
            read_sector(first_sec + i, read_block);
            int copy_size = min(cnt, BLOCK_SIZE);
            memcpy(buf, read_block, copy_size);
            buf += copy_size;
            cnt -= copy_size;

            if (cnt == 0) 
                goto end_read;
                
        }

        update_fat_read(fs, clus, fat_sec, fat_buf);

        clus = get_next_clus(fs, clus, fat_buf); 

    }
end_read:
    free(read_block);
    free(fat_buf);
    return;
}

void update_new_fsi_nxt_free(fat32_t *fs, uint8_t *fat_buf, uint32_t fat_sec)
{
    char flag = 0;
    // if has fat_buf, use it

    if (!fat_buf || (get_clus_fat_sec(fs, fs->fsi_nxt_free) != fat_sec)) {
        fat_sec = get_clus_fat_sec(fs, fs->fsi_nxt_free);
        fat_buf = malloc(BLOCK_SIZE);
        read_sector(fat_sec, fat_buf);
        flag = 1;
    }
    if (get_next_clus(fs, fs->fsi_nxt_free, fat_buf) == 0) 
        goto fsi_nxt_free_still_free;

    fs->fsi_nxt_free++;

    while (get_next_clus(fs, fs->fsi_nxt_free, fat_buf) != 0) {
        fs->fsi_nxt_free++;
        update_fat_read(fs, fs->fsi_nxt_free, fat_sec, fat_buf);
    }
    fs->fsi_free_cnt--;

fsi_nxt_free_still_free:
    if (flag)
        free(fat_buf);

}

void write_file(fat32_t *fs, file_t *file, const void *buf, size_t size)
{
    uint32_t clus = file->fst_clus, last_clus = file->fst_clus;
    uint8_t *fat_buf = malloc(BLOCK_SIZE);
    uint32_t fat_sec = get_clus_fat_sec(fs, clus);
    read_sector(fat_sec, fat_buf);

    uint32_t write_cnt = 0;

    file->attr |= DIR_ATTR_DIRTY;

    file->file_size = size;

    while (clus < END_OF_CLUS) {
        uint32_t next_clus = get_next_clus(fs, clus, fat_buf);
        if (write_cnt < size) {
            uint32_t first_sec = get_clus_first_sec(fs, clus);
            for (uint8_t i = 0;i < fs->sec_per_clus;i++) {
                if (write_cnt + BLOCK_SIZE > size) {
                    // To avoid wrong memory copy
                    uint8_t *tmp = malloc(BLOCK_SIZE);
                    memcpy(tmp, buf + write_cnt, size - write_cnt);
                    write_sector(first_sec + i, tmp);
                    free(tmp);
                } else
                    write_sector(first_sec + i, buf + write_cnt);
                
                write_cnt += BLOCK_SIZE;
                if (write_cnt >= size) {
                    // update fat buffer to END_OF_CLUS
                    set_next_clus(fs, clus, fat_buf, END_OF_CLUS);
                    break;
                }
            }
        } else {
            set_next_clus(fs, clus, fat_buf, 0);
            fs->fsi_free_cnt++;
            fs->fsi_nxt_free = min(fs->fsi_nxt_free, clus);
        }

        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            // write back the FAT
            if (write_cnt >= size)
                write_sector(fat_sec, fat_buf);
            fat_sec = get_clus_fat_sec(fs, clus);
            read_sector(fat_sec, fat_buf);
        }
        last_clus = clus;
        clus = next_clus;

    }

    clus = last_clus;
    while (write_cnt < size) {
        set_next_clus(fs, clus, fat_buf, fs->fsi_nxt_free);
        set_next_clus(fs, fs->fsi_nxt_free, fat_buf, END_OF_CLUS);
        clus = fs->fsi_nxt_free;
        if (get_clus_fat_sec(fs, clus) != fat_sec) {
            // write back the FAT
            write_sector(fat_sec, fat_buf);
            fat_sec = get_clus_fat_sec(fs, clus);
            read_sector(fat_sec, fat_buf);
        }
        update_new_fsi_nxt_free(fs, fat_buf, fat_sec);
        uint32_t first_sec = get_clus_first_sec(fs, clus);
        for (uint8_t i = 0;i < fs->sec_per_clus && write_cnt < size;i++, write_cnt += BLOCK_SIZE) {
            if (write_cnt + BLOCK_SIZE > size) {
                // To avoid wrong memory copy
                uint8_t *tmp = malloc(BLOCK_SIZE);
                memcpy(tmp, buf + write_cnt, size - write_cnt);
                write_sector(first_sec + i, tmp);
                free(tmp);
            } else
                write_sector(first_sec + i, buf + write_cnt);
                
        }
    }

    write_sector(fat_sec, fat_buf);

    free(fat_buf);
    return;
}

void dir_update(fat32_t *fs, dir_block_t *dir)
{
    uint8_t *fat_buf = malloc(BLOCK_SIZE);
    fat32_dir_t *dir_buf = malloc(BLOCK_SIZE);
    uint32_t fat_sec = -1;
    uint32_t dir_sec = 0;
    for (dir_block_t *now = dir;now;now = now->next) {
        if (!(now->file.attr &= DIR_ATTR_DIRTY))
            continue;
        if (!(now->file.attr &= DIR_ATTR_NAME_CHANGE)) {
            uint32_t x = now->file.block_entry_end;
            uint32_t clus_num = x / (BLOCK_SIZE / 32 * fs->sec_per_clus);
            uint8_t sec = (x / (BLOCK_SIZE / 32)) % fs->sec_per_clus;
            uint32_t num = (x / fs->sec_per_clus) % (BLOCK_SIZE / 32);
            uint32_t clus = now->clus;

            for (uint32_t j = 0;j < clus_num;j++) {
                update_fat_read(fs, clus, fat_sec, fat_buf);
                clus = get_next_clus(fs, clus, fat_buf);
            }
            if (dir_sec != get_clus_first_sec(fs, clus) + sec) {
                if (dir_sec)
                    write_sector(dir_sec, dir_buf);
                dir_sec = get_clus_first_sec(fs, clus) + sec;
                read_sector(dir_sec, dir_buf);
                dir_buf[num].file_size = now->file.file_size;
            }
        }
    }
    if (dir_sec)
        write_sector(dir_sec, dir_buf);
    free(dir_buf);
    free(fat_buf);
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

const char text[] = "test\n";

int main()
{
    mount_disk("disk.bin");

    fat32_t *fs = create_fat32();
    dir_block_t *root = load_dir(fs, fs->root_clus);

    ls_dir(root);

    char target_name[] = "meow.txt";

    file_t *target = find_file(root, target_name, sizeof(target_name));

    printf("%d\n", target->file_size);

    if (target)
        print_file_name(j, *target);
        
    uint8_t *target_buf = malloc(sizeof(text));

    read_file(fs, target, target_buf, sizeof(text));

    printf("%s\n", target_buf);

    write_file(fs, target, text, sizeof(text));

    dir_update(fs, root);   

    release_fat32(&fs);
    unmount_disk();
    free(root);

}
