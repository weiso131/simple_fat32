#include <stdio.h>
#include <disk_io.h>

FILE *disk;

void mount_disk(const char *path) {
    disk = fopen(path, "r+b");
}

void read_sector(uint32_t sector, void *buf) {
    fseek(disk, sector * 512, SEEK_SET);
    fread((uint8_t *)buf, 1, 512, disk);
}

void write_sector(uint32_t sector, const void *buf) {
    fseek(disk, sector * 512, SEEK_SET);
    fwrite((uint8_t *)buf, 1, 512, disk);
}

void unmount_disk() {
    fclose(disk);
}
