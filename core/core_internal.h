#ifndef _CORE_INTERNAL_H
#define _CORE_INTERNAL_H

#include <linux/seq_file.h>

#include "lkm_check.h"


/**
 * To iterate through the lists
 */
void core_for_each_available(
    void (*cb)(struct lkm_check *check, void *data),
    void *data);

void core_for_each_selected(
    void (*cb)(struct lkm_check *check, void *data),
    void *data);


/**
 * To select the specified check by alias/name
 */
int core_select_check(const char *name);

/**
 * Remove one specific item from "selected"
 */
int core_remove_check(const char *name);

/**
 * To empty the selected list
 */
void core_empty_selected(void);

int core_addall(void);

/**
 * Debugfs
 */
int core_debugfs_init(void);
void core_debugfs_exit(void);

#endif