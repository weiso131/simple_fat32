#pragma once
#include <stdint.h>

#define BLOCK_SIZE 512

void mount_disk(const char *path);

void read_sector(uint32_t sector, uint8_t *buf);

void write_sector(uint32_t sector, const uint8_t *buf);

void unmount_disk();
