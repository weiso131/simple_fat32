#include <type.h>
#include <stdlib.h>

#define is_valid_ascii(c) (c >= 0x20 && c <= 0x7E)

#define get_clus_first_sec(fs, n) ((n - 2) * (fs)->sec_per_clus + (fs)->first_data_sec)

#define get_clus_fat_sec(fs, n) ((fs)->first_fat_sec + (n * 4) / (fs)->byte_per_sec)

#define get_clus_fat_offset(fs, n) ((n * 4) % (fs)->byte_per_sec)

void update_next_clus(fat32_t *fs, uint32_t n, uint32_t ent_val);

#define get_next_clus(fs, clus, fat_buf) (*((uint32_t *)(fat_buf + get_clus_fat_offset(fs, clus))) & 0x0FFFFFFF)

#define set_next_clus(fs, clus, fat_buf, value) \
    do {\
        *((uint32_t *)(fat_buf + get_clus_fat_offset(fs, clus))) &= 0xF0000000;\
        *((uint32_t *)(fat_buf + get_clus_fat_offset(fs, clus))) |= (value & 0x0FFFFFFF);\
    } while (0)

static inline uint16_t *ascii_to_unicode(const void *file_name, const size_t name_size)
{
    uint16_t *unicode = malloc(sizeof(uint16_t) * name_size);

    for (size_t i = 0;i < name_size;i++)
        unicode[i] = ((uint8_t *)file_name)[i];

    return unicode;
}
    
