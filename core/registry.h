// SPDX-License-Identifier: GPL-2.0
/**
 * Registry
 * 
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#ifndef _REGISTRY_H
#define _REGISTRY_H

#include "lkm_plugin.h"

struct lkm_plugin* registry_acquire(const char* name);
void registry_release(struct lkm_plugin *plugin);

int registry_add(struct lkm_plugin* plugin);
void registry_remove(struct lkm_plugin* plugin);

void registry_for_each_acquired(
    int (*cb)(struct lkm_plugin* plugin, void* data),
    void* data
);

void registry_destroy(void);

#endif
