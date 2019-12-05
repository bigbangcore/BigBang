/*
 *  sll.h
 *  SlimGuard
 *  Copyright (c) 2019, Beichen Liu, Virginia Tech
 *  All rights reserved
 */

#ifndef SLL_H
#define SLL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

typedef struct sll_t
{
    struct sll_t* next;
} sll_t;

sll_t* next_entry(sll_t* cur);
sll_t* add_head(sll_t* node, sll_t* slist);
sll_t* remove_head(sll_t* slist);
void print_list(sll_t* slist);

#endif
