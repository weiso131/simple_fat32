#include <type.h>

#define is_valid_ascii(c) (c >= 0x20 && c <= 0x7E)

#define get_clus_first_sec(fs, n) ((n - 2) * (fs)->sec_per_clus + (fs)->first_data_sec)

#define get_clus_fat_sec(fs, n) ((fs)->first_fat_sec + (n * 4) / (fs)->byte_per_sec)

#define get_clus_fat_offset(fs, n) ((n * 4) % (fs)->byte_per_sec)

void update_next_clus(fat32_t *fs, uint32_t n, uint32_t ent_val);

uint32_t get_next_clus(fat32_t *fs, uint32_t n);

#define ascii_to_unicode_little(x) ((uint16_t) x)
