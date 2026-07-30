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


#include "registry.h"
#include "selector.h"
#include "executor.h"
#include "core_internal.h"
#include "lkm_plugin.h"



//--------------------------------------------------------------------------------
//Entry selection

//DONE
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

//DONE
/**
 * 
 * Best-effort approach, returns last error if any.
 */
int core_addall(void){
    return registry_for_each_acquired(core_addall_cb, NULL);
}

static int core_addall_cb(struct lkm_plugin *plugin, void *data){
    int ret;
    
    ret = selector_add(plugin);
    if(ret)
        registry_release(plugin);
    
    return ret;
}


//DONE
/**
 * Deselects plugin
 */
int core_remove_plugin(const char* name){
    return selector_remove(name);
}


//DONE
int core_empty_selected(void){
    return selector_clear();
}

//--------------------------------------------------------------------------------
// Plugin execution

int core_execute_selected(struct seq_file* m){
    return executor_run_selected(m);
}


//--------------------------------------------------------------------------------
// Plugin registration and unregistration from the core

//DONE
int lkm_register_plugin(struct lkm_plugin *plugin){
    // add safety checks
    return registry_add(plugin);

}
EXPORT_SYMBOL(lkm_register_plugin);


//DONE
void lkm_unregister_plugin(struct lkm_plugin *plugin){
    // add safety checks (does plugin exist, is it valid plugin, etc.)

    selector_remove(plugin);
    registry_remove(plugin);
}
EXPORT_SYMBOL(lkm_unregister_plugin);



//--------------------------------------------------------------------------------
// Initialitation and Exit of module

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
    selector_free_list();

    //Free list_available
    registry_free_list();

    //Remove debugfs
    core_debugfs_exit();

    pr_info("lkm CORE: removed from kernel\n");
}
module_exit(core_exit);



//--------------------------------------------------------------------------------

MODULE_LICENSE("GPL");
MODULE_ALIAS("sfgcore");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Development version of core for lkm management.");