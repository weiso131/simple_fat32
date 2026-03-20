#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <disk_io.h>

typedef unsigned long addr_t;

#define EXTERN_NULL 0

static void extern_memory_read(addr_t block_addr, unsigned char *dest)
{
    memcpy(dest, (unsigned char *)block_addr, 64);
}

static void extern_memory_write(addr_t block_addr, unsigned char *src)
{
    memcpy((unsigned char *)block_addr, src, 64);
    return;
}

static void picos_read_sector(uint32_t sector, addr_t buf)
{
    read_sector(sector, (uint8_t *) buf);
}

static void picos_write_sector(uint32_t sec, addr_t sec_buf)
{
    write_sector(sec, (uint8_t *)sec_buf);
}

/* 64 bytes per block */
static addr_t picos_memory_alloc(uint32_t block_num)
{
    return (addr_t) malloc(block_num * 64);
}

static void picos_memory_release(addr_t addr)
{
    free((char *)addr);
}

extern unsigned char picos_fat_cache[64];
extern unsigned char picos_cache[64];
extern unsigned char dir_block_cache[64];
