// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */


#ifndef _CORE_INTERNAL_H
#define _CORE_INTERNAL_H

#include <linux/seq_file.h>

#include "lkm_plugin.h"



/**
 * To select the specified plugin by alias/name
 */
int core_select_plugin(const char *name);

/**
 * To select every available plugin
 */
int core_addall(void);

/**
 * Remove one specific item from "selected"
 */
int core_remove_plugin(const char *name);

/**
 * To empty the selected list
 */
void core_empty_selected(void);

/**
 * To execute the functionality of the selected plugins
 */
int core_execute_selected(struct seq_file* m);


/**
 * Debugfs
 */
int core_debugfs_init(void);
void core_debugfs_exit(void);

#endif