// SPDX-License-Identifier: GPL-2.0
/**
 * Selector
 * 
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */


#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "selector.h"
#include "registry.h"

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


static struct lkm_plugin * selector_remove_entry(struct entry_selected* entry){
    struct lkm_plugin* plugin = entry->plugin;

    list_del(&entry->list);
    kfree(entry);

    return plugin;
}


int selector_remove(const char* name){
    struct entry_selected *entry;
    struct lkm_plugin* plugin;

    mutex_lock(&lock_list_selected);

    entry = selector_find_name_locked(name);    
    if(!entry){
        mutex_unlock(&lock_list_selected);
        return -ENOENT;
    }
    plugin = selector_remove_entry(entry);

    mutex_unlock(&lock_list_selected);

    registry_release(plugin);

    return 0;
}

int selector_remove_plugin(struct lkm_plugin* plugin){
    struct entry_selected *entry;
        
    mutex_lock(&lock_list_selected);

    entry = selector_find_plugin_locked(plugin);
    if(!entry){
        mutex_unlock(&lock_list_selected);
        return -ENOENT;
    }
    plugin = selector_remove_entry(entry);
    
    mutex_unlock(&lock_list_selected);

    registry_release(plugin);

    return 0;
}


void selector_clear(void){
    struct entry_selected *pos;
    struct entry_selected *temp;
    struct lkm_plugin* plugin;
    
    mutex_lock(&lock_list_selected);
    list_for_each_entry_safe(pos, temp, &list_selected, list){
        pr_info("-Deleting plugin from list of selected: %s\n", pos->plugin->alias);
        plugin = selector_remove_entry(pos);
        registry_release(plugin);
    }
    mutex_unlock(&lock_list_selected);

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

/**
 * Calls callback "cb" for each selected plugin.
 * 
 * Each plugin is pinned with a temporary module reference increase, and then it is decreased after cb returns.
 * Ownership of the reference is NOT transfered to the callback cb function.
 */
int selector_for_each(
    int (*cb)(struct lkm_plugin *plugin, void *data),
    void *data
){  
    struct entry_selected *pos;
    struct lkm_plugin **snapshot;

    int count = 0;
    int i = 0;
    int ret = 0;

    //Count how many entries in list_selected
    mutex_lock(&lock_list_selected);
    list_for_each_entry(pos, &list_selected, list){
        count++;
    }
    
    if(!count){
        mutex_unlock(&lock_list_selected);
        return 0;
    }

    snapshot = kcalloc(count, sizeof(*snapshot), GFP_KERNEL);
    if(!snapshot){
        mutex_unlock(&lock_list_selected);
        return -ENOMEM;
    }

    //Add plugins to snapshot + pin them to avoid unregistration
    list_for_each_entry(pos, &list_selected, list){
        if(try_module_get(pos->plugin->owner)){
            snapshot[i] = pos->plugin;
            i++;
        }
    }
    mutex_unlock(&lock_list_selected);

    //Run the plugins with no locked lists along the process
    for(int j = 0; j < i; j++){
        int cb_ret;

        cb_ret = cb(snapshot[j], data);
        module_put(snapshot[j]->owner);

        if(cb_ret && !ret)
            ret = cb_ret;
    }

    kfree(snapshot);

    return ret;
}


//DONE
/**
 * Destroy selector subsystem.
 * Currently calls selector_clear() because goal is emptying list_selected.
 * However, this call is left for the evolution of the selector and potential
 * increase in teardown operations.
 */
void selector_destroy(void){
    selector_clear();
}