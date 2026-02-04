#ifndef _LKM_CHECK_H
#define _LKM_CHECK_H

/**
 * This header serves as ABI for the project.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/seq_file.h>

#define LKM_CHECK_API_VERSION 1
#define PLUGIN_MAX_NAME 64
#define PLUGIN_MAX_CATEGORY 64

/**
 * 
 */
struct lkm_check {
    int api_version;
    
    struct module *owner;
    struct list_head list;

    const char name[PLUGIN_MAX_NAME];
    const char category[PLUGIN_MAX_CATEGORY];

    int (*run)(struct seq_file *m);
    // "run" is a function pointer that returns an integer and that takes a seq_file struct pointer
};


/**
 * 
 */
int core_register_check(struct lkm_check *check);
void core_unregister_check(struct lkm_check *check);


#endif