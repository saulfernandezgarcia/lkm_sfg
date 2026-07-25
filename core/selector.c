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
    
    int ret = 0;
    struct entry_selected *sel = NULL;
    

    //__Check if plugin is already in list of selected
    mutex_lock(&lock_list_selected);
    list_for_each_entry(sel, &list_selected, list){
        if(sel->plugin == plugin){
            ret = -EEXIST;
            goto out_unlock_selected;   
        }
    }

    //Allocate new entry_selected for list_selected
    sel = kzalloc(sizeof(*sel), GFP_KERNEL);
    if(!sel){
        ret = -ENOMEM;
        goto out_unlock_selected;
    }

    pr_info("lkm: selector: plugin %s was not in selected list. It will now be added.\n", plugin->alias);
    sel->plugin = plugin;
    list_add_tail(&sel->list, &list_selected);
    pr_info("lkm: selector: added to 'selected' the plugin with alias: %s\n", plugin->alias);

out_unlock_selected:
    mutex_unlock(&lock_list_selected);

    return ret;

    /*
    
    int ret = 0;
    struct lkm_plugin *found = NULL;
    struct entry_selected *sel = NULL;

    

    //__Check if plugin is already in list of selected
    mutex_lock(&lock_list_selected);
    list_for_each_entry(sel, &list_selected, list){
        if(sel->plugin == plugin){
            ret = -EEXIST;
            goto out_unlock_selected;
        }
    }

    //__Take module reference for refcount
    if(!try_module_get(plugin->owner)){
        ret = -EINVAL;
        goto out_unlock_selected;
    }
    
    //Allocate new entry_selected for list_selected
    sel = kzalloc(sizeof(*sel), GFP_KERNEL);
    if(!sel){
        ret = -ENOMEM;
        goto out_module_put;
    }
    
    pr_info("lkm: selector: plugin %s was not in selected list. It will now be added.\n", found->alias);
    sel->plugin = found;
    list_add_tail(&sel->list, &list_selected);
    ret = 0;
    pr_info("lkm: selector: added to 'selected' the plugin with alias: %s\n", found->alias);

    goto out_unlock_selected; //equivalent to performing unlock(selected) and unlock(available) and then return 0;

out_module_put:
    module_put(found->owner);

out_unlock_selected:
    mutex_unlock(&lock_list_selected);

    return ret;
    */
}



void selector_remove(struct lkm_plugin* plugin){
    pr_info("lkm: selector: plugin %s requesting removal from list of selected plugins\n", plugin->name);

    mutex_lock(&lock_list_selected);

    //Removing plugin from "list_selected":
    struct entry_selected *pos_s;
    struct entry_selected *temp_s;
    
    list_for_each_entry_safe(pos_s, temp_s, &list_selected, list){
        if(pos_s->plugin == plugin){
            list_del(&pos_s->list);
            module_put(pos_s->plugin->owner);
            kfree(pos_s);
            break;
        }
    }

    pr_info("lkm: selector: plugin %s was removed from list of selected plugins\n", plugin->name);

    mutex_unlock(&lock_list_selected);
}

void selector_remove_by_name(const char* name){

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