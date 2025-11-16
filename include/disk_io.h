#pragma once
#include <stdint.h>

#define BLOCK_SIZE 512

void mount_disk(const char *path);

void read_sector(uint32_t sector, void *buf);

void write_sector(uint32_t sector, const void *buf);

void unmount_disk();
