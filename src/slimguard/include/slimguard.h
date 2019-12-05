/*
 *  slimguard.h
 *  SlimGuard
 *  Copyright (c) 2019, Beichen Liu, Virginia Tech
 *  All rights reserved
 */

#ifndef SLIMGUARD_H
#define SLIMGUARD_H

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define ETP (8)                     // entropy to 8 bits
#define GP (10)                     // 1 guard page per GP pages
#define BKT (1 << ETP)              // bucket size
#define ALIGN (7)                   // alignment
#define MAX_EXP (17)                // max size allocate
#define MIN_EXP (6)                 // smallest blk
#define FLI (MAX_EXP - MIN_EXP + 1) // num of first level index
#define SLI_LOG2 (4)                // number of second level index
#define SLI (1 << SLI_LOG2)
#define SMALL_BLK (1 << (MIN_EXP + 1))
#define ELE (1 << 24) // 3 ** 24 elements
#define SEED_MAX (1 << 10)
#define PAGE_SIZE (1 << 12)
#define INDEX (FLI * SLI)
#define BUCKET_SIZE (8UL << 30) // 4GB

#ifdef __cplusplus
extern "C"
{
#endif

    int log2_64(uint64_t value);
    uint8_t sz2cls(uint32_t sz);
    uint32_t cls2sz(uint16_t cls);
    uint64_t round_size(uint64_t sz);

    void* get_mem(uint64_t size);
    void init_bibop();
    void init_bucket(uint8_t index);
    void* get_next(uint8_t index);
    void* get_random_obj(uint8_t index);

    char HashPointer(void* ptr);
    void set_canary(void* ptr, uint8_t index);
    void get_canary(void* ptr, uint8_t index);

    void mark_used(void* ptr, uint8_t index);
    void mark_free(void* ptr, uint8_t index);

#ifdef RELEASE_MEM
    void increment_pc(void* ptr, uint8_t index);
    void decrement_pc(void* ptr, uint8_t index);
#endif

    void* xxmalloc(size_t sz);
    void xxfree(void* ptr);
    void* xxrealloc(void* ptr, size_t size);
    void* xxmemalign(size_t alignment, size_t size);

    void* slimguard_malloc(size_t sz);
    void slimguard_free(void* ptr);
    void* slimguard_realloc(void* ptr, size_t sz);
    void* slimguard_memalign(size_t alignment, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
