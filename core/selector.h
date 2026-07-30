// SPDX-License-Identifier: GPL-2.0
/**
 * Selector
 * 
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#ifndef _SELECTOR_H
#define _SELECTOR_H

#include "lkm_plugin.h"


int selector_add(struct lkm_plugin* plugin);

int selector_remove(const char* name);
int selector_remove_plugin(struct lkm_plugin* plugin);

void selector_clear(void);

void selector_destroy(void);

int selector_for_each(
    int (*cb)(struct lkm_plugin *plugin, void *data),
    void *data
);

#endif
