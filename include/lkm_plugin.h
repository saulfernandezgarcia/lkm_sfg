// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#ifndef _LKM_PLUGIN_H
#define _LKM_PLUGIN_H

/**
 * This header serves as ABI for the project.
 */

#include <linux/module.h>
#include <linux/seq_file.h>

#define LKM_ABI_VERSION 1

/**
 * 
 */
struct lkm_plugin {
    /* Plugin metadata */
    const char *name;
    const char *alias;
    const char *category;

    /* Plugin operations */
    int (*run)(struct seq_file *m);
    // "run" is a function pointer that returns an integer and that takes a seq_file struct pointer

    /* Managed by framework */
    u32 abi_version;
    struct module *owner;
};

#endif