// SPDX-License-Identifier: GPL-2.0
/**
 * LKM: core manager
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

#include "registry.h"




//--------------------------------------------------------------------------------
//List traversal

void core_for_each_available(
    void (*cb)(struct lkm_plugin *plugin, void *data),
    void*data){
    
    struct entry_available *pos;
    struct lkm_plugin **snapshot;

    int count = 0;
    int i = 0;

    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list)
        count++;

    if(!count){
        mutex_unlock(&lock_list_available);
        return;
    }

    snapshot = kcalloc(count, sizeof(*snapshot), GFP_KERNEL);
    if(!snapshot){
        mutex_unlock(&lock_list_available);
        return;
    }

    list_for_each_entry(pos, &list_available, list){
        if(try_module_get(pos->plugin->owner)){
            snapshot[i] = pos->plugin;
            i++;
        }
    }
    mutex_unlock(&lock_list_available);

    for(int j = 0; j < i; j++){
        cb(snapshot[j], data);
        module_put(snapshot[j]->owner);
    }

    kfree(snapshot);    
}


/**
 * 
 * kmalloc calloc array allocation: kcalloc
 * https://www.kernel.org/doc/html/v5.0/core-api/mm-api.html#c.kzalloc
 */
void core_for_each_selected(
    void (*cb)(struct lkm_plugin *plugin, void *data),
    void*data){
    
    struct entry_selected *pos = NULL;
    struct lkm_plugin **snapshot = NULL;
    int count = 0;
    int i = 0;

    //Count how many plugins to run and allocate array
    mutex_lock(&lock_list_selected);
    list_for_each_entry(pos, &list_selected, list){
        count++;
    }
    

    if(!count){
        mutex_unlock(&lock_list_selected);
        return;
    }

    snapshot = kcalloc(count, sizeof(*snapshot), GFP_KERNEL);
    if(!snapshot){
        mutex_unlock(&lock_list_selected);
        return;
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
        cb(snapshot[j], data);
        module_put(snapshot[j]->owner);
    }

    kfree(snapshot);

}

//--------------------------------------------------------------------------------
//Entry selection

/**
 * 
 */
int core_select_plugin(const char* name){
    struct lkm_plugin* plugin = registry_acquire(name);
    
    if(!plugin)
        return -ENOENT;
    
    int ret = selector_add(plugin);
    if(ret)
        registry_release(plugin);
    
    return ret;
}




//TODO
/**
 * 
 * Best-effort approach, returns last error if any.
 */
int core_addall(void){

    struct entry_available *pos = NULL;
    struct entry_selected *sel = NULL;
    struct entry_selected *new_sel = NULL;
    int last_ret = 0;

    mutex_lock(&lock_list_available);
    mutex_lock(&lock_list_selected);

    list_for_each_entry(pos, &list_available, list){

        int already = 0;
        list_for_each_entry(sel, &list_selected, list){
            if(sel->plugin == pos->plugin){
                already = 1;
                break;
            }
        }

        if(already)
            continue;

        if(!try_module_get(pos->plugin->owner)){
            last_ret = -EINVAL;
            continue;
        }

        new_sel = kzalloc(sizeof(*new_sel), GFP_KERNEL);
        if(!new_sel){
            module_put(pos->plugin->owner);
            last_ret = -ENOMEM;
            continue;
        }

        new_sel->plugin = pos->plugin;
        list_add_tail(&new_sel->list, &list_selected);
        pr_info("lkm: added to 'selected' the plugin with alias: %s\n", new_sel->plugin->alias);
    }

    mutex_unlock(&lock_list_selected);
    mutex_unlock(&lock_list_available);

    return last_ret;
}

//DONE
/**
 * Deselects plugin
 */
int core_remove_plugin(const char*name){
    return selector_remove(name);
}

    /*
int core_remove_plugin(const char*name){
    struct entry_selected *pos;
    struct entry_selected *temp;
    int found = 0;

    mutex_lock(&lock_list_selected);
    list_for_each_entry_safe(pos, temp, &list_selected, list){
        if(strcmp(pos->plugin->alias, name) == 0 || strcmp(pos->plugin->name, name) == 0){
            list_del(&pos->list);
            pr_info("lkm: removed from 'selected' the plugin with alias: %s\n", pos->plugin->alias);
            module_put(pos->plugin->owner);
            kfree(pos);
            found = 1;
            break;
        }
    }
    mutex_unlock(&lock_list_selected);

    if(!found)
        return -ENOENT;

    return 0;
}
*/

void core_empty_selected(void){
    struct entry_selected *pos;
    struct entry_selected *temp;

    mutex_lock(&lock_list_selected);
    list_for_each_entry_safe(pos, temp, &list_selected, list){
        list_del(&pos->list);
        module_put(pos->plugin->owner);
        kfree(pos);
    }
    mutex_unlock(&lock_list_selected);
}

//--------------------------------------------------------------------------------
// Plugin registration and unregistration from the core

int lkm_register_plugin(struct lkm_plugin *plugin){
    // add safety checks
    return registry_add(plugin);

}
EXPORT_SYMBOL(lkm_register_plugin);



void lkm_unregister_plugin(struct lkm_plugin *plugin){
    // add safety checks (does plugin exist, is it valid plugin, etc.)

    selector_remove(plugin);
    registry_remove(plugin);
}
EXPORT_SYMBOL(lkm_unregister_plugin);



//--------------------------------------------------------------------------------


/**
 * __init
 * 
 * https://docs.kernel.org/filesystems/debugfs.html
 * 
 * Debugfs files will be at /sys/kernel/debug/lkmsfg/
 */
static int __init core_init(void){
    pr_info("lkm CORE: loading into kernel\n");
    return core_debugfs_init();
}
module_init(core_init);

/**
 * __exit
 * 
 * Will recursively remove the directory tree we created with
 * debugfs and the files within it.
 */
static void __exit core_exit(void){
    pr_info("lkm CORE: removing from kernel\n");

    //Free list_selected
    struct entry_selected *pos_s;
    struct entry_selected *temp_s;
    
    mutex_lock(&lock_list_selected);
    list_for_each_entry_safe(pos_s, temp_s, &list_selected, list){
        pr_info("-Deleting plugin from list of selected: %s\n", pos_s->plugin->alias);
        list_del(&pos_s->list);
        module_put(pos_s->plugin->owner);
        kfree(pos_s);
    }
    mutex_unlock(&lock_list_selected);

    //Free list_available
    struct entry_available *pos_a;
    struct entry_available *temp_a;

    mutex_lock(&lock_list_available);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        pr_info("-Deleting plugin from available ones: %s\n", pos_a->plugin->alias);
        list_del(&pos_a->list);
        kfree(pos_a);
    }

    mutex_unlock(&lock_list_available);

    //Remove debugfs:
    core_debugfs_exit();

    pr_info("lkm CORE: removed from kernel\n");
}
module_exit(core_exit);

//--------------------------------------------------------------------------------

MODULE_LICENSE("GPL");
MODULE_ALIAS("sfgcore");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Development version of core for lkm management.");