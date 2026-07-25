// SPDX-License-Identifier: GPL-2.0
/**
 * Selector
 * 
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */


#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "core_internal.h"
#include "lkm_plugin.h"

static LIST_HEAD(list_selected);
static DEFINE_MUTEX(lock_list_selected);


struct entry_selected{
    struct list_head list;
    struct lkm_plugin *plugin;
};


/**
 * 
 * Adds a referenced plugin to the selected list.
 * On success, ownership of the reference is transferred to selector.
 * On failure, ownership responsibility remains within the caller.
 */
int selector_add(struct lkm_plugin* plugin){
    
    struct entry_selected* pos;

    mutex_lock(&lock_list_selected);

    pos = selector_find_plugin_locked(plugin);
    if(pos){
        //Entry already exists
        mutex_unlock(&lock_list_selected);
        return -EEXIST;
    }

    //Allocate new entry_selected for list_selected
    pos = kzalloc(sizeof(*pos), GFP_KERNEL);
    if(!pos){
        mutex_unlock(&lock_list_selected);
        return -ENOMEM;
    }

    pr_info("lkm: selector: plugin %s was not in selected list. It will now be added.\n", plugin->alias);
    pos->plugin = plugin;
    list_add_tail(&pos->list, &list_selected);
    pr_info("lkm: selector: added to 'selected' the plugin with alias: %s\n", plugin->alias);

    mutex_unlock(&lock_list_selected);

    return 0;
}



int selector_remove(const char* name){
    struct entry_selected *pos;

    mutex_lock(&lock_list_selected);

    pos = selector_find_name_locked(name);    
    if(!pos){
        mutex_unlock(&lock_list_selected);
        return -ENOENT;
    }

    list_del(&pos->list);
    kfree(pos);
    mutex_unlock(&lock_list_selected);

    registry_release(pos->plugin);

    return 0;
}

/**
 * Assumes the mutex lock_list_selected for list_selected is already held.
 * Traverses list_selected entries and tries to find a matching plugin.
 * Returns entry_selected if successful.
 */
static struct entry_selected* selector_find_name_locked(const char* name){
    struct entry_selected *pos;
    list_for_each_entry(pos, &list_selected, list){
        if(strcmp(pos->plugin->alias, name) == 0 || strcmp(pos->plugin->name, name) == 0){
            return pos;
        }
    }
    return NULL;
}

/**
 * Assumes the mutex lock_list_selected is held.
 */
static struct entry_selected* selector_find_plugin_locked(struct lkm_plugin* plugin){
    struct entry_selected *pos;
    list_for_each_entry(pos, &list_selected, list){
        if(pos->plugin == plugin){
            return pos;
        }
    }
    return NULL;
}

/*
unsafe! pointer post-function lifetime protection

struct lkm_plugin* selector_find_plugin_by_name(const char* name){
    
    int found = 0;
    struct entry_selected *sel = NULL;

    //Check to see if the plugin is in selected
    mutex_lock(&lock_list_selected);
    list_for_each_entry(sel, &list_selected, list){
        if(strcmp(sel->plugin->alias, name) == 0 || strcmp(sel->plugin->name, name) == 0){
            found = 1;
            break;
        }
    }
    mutex_unlock(&lock_list_selected);

    if(!found){
        return NULL;
    }

    return sel->plugin;
}
*/

/**
 * Allow for interaction with snapshot of contents in registry.
 */
void selector_snapshot(    
    void (*cb)(struct lkm_plugin *plugin, void *data),
    void *data){
        
}