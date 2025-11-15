#include <stdio.h>
#include <stdlib.h>

#include <disk_io.h>
#include <type.h>

int main()
{
    mount_disk("disk.bin");

    struct sector0_struct *sector0 = malloc(BLOCK_SIZE);

    read_sector(0, (uint8_t *)sector0);
    printf("uint16_t byte_per_sec: %u\n", sector0->bpb.byte_per_sec);
    printf("uint8_t sec_per_clus: %u\n", sector0->bpb.sec_per_clus);
    printf("uint16_t rsvd_sec_cnt: %u\n", sector0->bpb.rsvd_sec_cnt);
    printf("uint8_t num_fats: %u\n", sector0->bpb.num_fats);
    printf("uint16_t root_ent_cnt: %u\n", sector0->bpb.root_ent_cnt);
    printf("uint16_t tot_sec_16: %u\n", sector0->bpb.tot_sec_16);
    printf("uint8_t media: %u\n", sector0->bpb.media);
    printf("uint16_t fat_sz_16: %u\n", sector0->bpb.fat_sz_16);
    printf("uint16_t sec_per_trk: %u\n", sector0->bpb.sec_per_trk);
    printf("uint16_t num_heads: %u\n", sector0->bpb.num_heads);
    printf("uint32_t hidd_sec: %u\n", sector0->bpb.hidd_sec);
    printf("uint32_t tot_sec32: %u\n", sector0->bpb.tot_sec32);
    printf("uint32_t fat_sz32: %u\n", sector0->bpb.fat_sz32);
    printf("uint16_t ext_flags: %u\n", sector0->bpb.ext_flags);
    printf("uint16_t fs_ver: %u\n", sector0->bpb.fs_ver);
    printf("uint32_t root_clus: %u\n", sector0->bpb.root_clus);
    printf("uint16_t fs_info: %u\n", sector0->bpb.fs_info);
    printf("uint16_t bk_boot_sec: %u\n", sector0->bpb.bk_boot_sec);

    unmount_disk();
}
