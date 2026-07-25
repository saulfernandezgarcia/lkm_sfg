// SPDX-License-Identifier: GPL-2.0
/**
 * Registry
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


static LIST_HEAD(list_available);
static DEFINE_MUTEX(lock_list_available);

struct entry_available{
    struct list_head list;
    struct lkm_plugin *plugin;
};

/**
 * 
 * Also increases module reference count for the plugin. After use of plugin is finished,
 * registry_release should be called unless ownership is being transferred.
 */
struct lkm_plugin* registry_acquire(const char* name){
    struct lkm_plugin* found = NULL;
    struct entry_available *pos = NULL;

    //Check to see if the plugin is in available
    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        if(strcmp(pos->plugin->alias, name) == 0 || strcmp(pos->plugin->name, name) == 0){
            if(try_module_get(pos->plugin->owner))
                found = pos->plugin;
            break;
        }
    }
    mutex_unlock(&lock_list_available);

    return found;
}


/**
 * 
 * Decreases module reference count for the plugin.
 * Only to be used after a registry_acquire has taken place.
 */
void registry_release(struct lkm_plugin* plugin){
    module_put(plugin->owner);
}


/**
 * "Registers" a plugin through its addition to the list "list_available"
 * Registration API Definition
 * @plugin: plugin plugin to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int registry_add(struct lkm_plugin* plugin){
    int ret = 0;
    struct entry_available *new_entry = NULL;
    struct entry_available *aux = NULL;


    //__Check if plugin is already in list of available
    mutex_lock(&lock_list_available);
    list_for_each_entry(aux, &list_available, list){
        if(aux->plugin == plugin){
            ret = -EEXIST;
            goto out_unlock_available;   
        }
    }


    pr_info("lkm: plugin %s requesting registration\n", plugin->name);
    mutex_lock(&lock_list_available);
    pr_info("lkm: plugin %s began registration\n", plugin->name);

    //Allocate new entry_available for list_selected 
    new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
    if(!new_entry){
        ret = -ENOMEM;
        goto out_unlock_available;
    }

    new_entry->plugin = plugin;
    list_add_tail(&new_entry->list, &list_available);
    pr_info("lkm: plugin %s finished registration\n", plugin->name);

out_unlock_available:
    mutex_unlock(&lock_list_available);

    return ret;
}



/**
 * Removes the plugin from the list of available plugins.
 * If the plugin is ALSO IN SELECTED, IT WILL NOT BE REMOVED FROM SELECTED.
 * CORRECT REMOVAL FROM TOOL REQUIRES REMOVAL FROM SELECTED AND REMOVAL FROM AVAILABLE
 */
void registry_remove(struct lkm_plugin* plugin){
    pr_info("lkm: registry: plugin %s requesting removal from list of available plugins\n", plugin->name);

    mutex_lock(&lock_list_available);


    //Removing plugin from "available" list
    struct entry_available *pos_a;
    struct entry_available *temp_a;

    pr_info("lkm: registry: plugin %s began unregistration\n", plugin->name);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        if(pos_a->plugin == plugin){
            list_del(&pos_a->list);
            kfree(pos_a);
            break;
        }
    }
    pr_info("lkm: registry: plugin %s was removed from list of available plugins\n", plugin->name);

    mutex_unlock(&lock_list_available);
}

/*
unsafe for now, think about pointer lifetime outside mutexes. No protection!!!

struct lkm_plugin* registry_find_plugin_by_name(const char* name){

    int found = 0;
    struct entry_available *pos = NULL;

    //Check to see if the plugin is in available
    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        if(strcmp(pos->plugin->alias, name) == 0 || strcmp(pos->plugin->name, name) == 0){
            found = 1;
            break;
        }
    }
    mutex_unlock(&lock_list_available);

    if(!found){
        return NULL;
    }

    return pos->plugin;
}

*/

/**
 * Allow for interaction with snapshot of contents in registry.
 */
void registry_snapshot(    
    void (*cb)(struct lkm_plugin *plugin, void *data),
    void *data){

    }