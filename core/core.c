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


static LIST_HEAD(list_available);
static DEFINE_MUTEX(lock_list_available);

static LIST_HEAD(list_selected);
static DEFINE_MUTEX(lock_list_selected);


struct entry_available{
    struct list_head list;
    struct lkm_plugin *plugin;
};


struct entry_selected{
    struct list_head list;
    struct lkm_plugin *plugin;
};

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
int core_select_plugin(const char *name){
    
    int ret = 0;
    struct lkm_plugin *found = NULL;
    struct entry_available *pos = NULL;
    struct entry_selected *sel = NULL;

    //Check to see if the plugin is available
    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        if(strcmp(pos->plugin->alias, name) == 0 || strcmp(pos->plugin->name, name) == 0){
            found = pos->plugin;
            break;
        }
    }

    //If found, actually store the data into our list of selected plugins
    if(!found){
        ret = -ENOENT;
        goto out_unlock_available;

    } 

    //__Check if plugin is already in list of selected
    mutex_lock(&lock_list_selected);
    list_for_each_entry(sel, &list_selected, list){
        if(sel->plugin == found){
            ret = -EEXIST;
            goto out_unlock_selected;
        }
    }

    //__Take module reference for refcount
    if(!try_module_get(found->owner)){
        ret = -EINVAL;
        goto out_unlock_selected;
    }
    
    //Allocate new entry_selected for list_selected
    sel = kzalloc(sizeof(*sel), GFP_KERNEL);
    if(!sel){
        ret = -ENOMEM;
        goto out_module_put;
    }
    
    pr_info("lkm: plugin %s was not in selected list. It will now be added.\n", found->alias);
    sel->plugin = found;
    list_add_tail(&sel->list, &list_selected);
    ret = 0;
    pr_info("lkm: added to 'selected' the plugin with alias: %s\n", found->alias);

    goto out_unlock_selected; //equivalent to performing unlock(selected) and unlock(available) and then return 0;

out_module_put:
    module_put(found->owner);

out_unlock_selected:
    mutex_unlock(&lock_list_selected);

out_unlock_available:
    mutex_unlock(&lock_list_available);


    return ret;
}

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

/**
 * 
 * list_for_each_entry_safe()
 */
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


/**
 * Registration API Definition
 * @plugin: plugin plugin to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int core_register_plugin(struct lkm_plugin *plugin){
    
    int ret = 0;
    struct entry_available *new_entry = NULL;

    pr_info("lkm: plugin %s requesting registration\n", plugin->name);
    mutex_lock(&lock_list_available);
    pr_info("lkm: plugin %s began registration\n", plugin->name);

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

int lkm_register_plugin(struct lkm_plugin *plugin){
    // add safety checks

    return core_register_plugin(plugin);
}
EXPORT_SYMBOL(lkm_register_plugin);




/**
 * Unregistration API Definition
 * @plugin: plugin plugin to unregister.
 * 
 * Unregistration.
 * We first remove the plugin from the "selected" array to be able
 */
void core_unregister_plugin(struct lkm_plugin *plugin){
    pr_info("lkm: plugin %s requesting unregistration\n", plugin->name);

    mutex_lock(&lock_list_available);
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

    //Removing plugin from "available" list
    struct entry_available *pos_a;
    struct entry_available *temp_a;

    pr_info("lkm: plugin %s began unregistration\n", plugin->name);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        if(pos_a->plugin == plugin){
            list_del(&pos_a->list);
            kfree(pos_a);
            break;
        }
    }
    pr_info("lkm: plugin %s finished unregistration\n", plugin->name);

    mutex_unlock(&lock_list_selected);
    mutex_unlock(&lock_list_available);
}

void lkm_unregister_plugin(struct lkm_plugin *plugin){
    // add safety checks

    core_unregister_plugin(plugin);
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