/**
 * Test project configuration
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
#include "lkm_check.h"


static LIST_HEAD(list_available);
static DEFINE_MUTEX(lock_list_available);

static LIST_HEAD(list_selected);
static DEFINE_MUTEX(lock_list_selected);


struct entry_available{
    struct list_head list;
    struct lkm_check *check;
};


struct entry_selected{
    struct list_head list;
    struct lkm_check *check;
};

//--------------------------------------------------------------------------------
//List traversal

void core_for_each_available(
    void (*cb)(struct lkm_check *check, void *data),
    void*data){
    
    struct entry_available *pos;

    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        cb(pos->check, data);
    }
    mutex_unlock(&lock_list_available);
}


void core_for_each_selected(
    void (*cb)(struct lkm_check *check, void *data),
    void*data){
    
    struct entry_selected *pos;

    mutex_lock(&lock_list_selected);
    list_for_each_entry(pos, &list_selected, list){
        cb(pos->check, data);
    }
    mutex_unlock(&lock_list_selected);
}


//--------------------------------------------------------------------------------
//Entry selection

/**
 * 
 * errno-base.h
 */
int core_select_check(const char *name){
    
    struct lkm_check *found = NULL;
    struct entry_available *pos = NULL;

    //Check to see if the plugin is available
    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        if(strcmp(pos->check->alias, name) == 0 || strcmp(pos->check->name, name) == 0){
            found = pos->check;
            break;
        }
    }
    mutex_unlock(&lock_list_available);

    //If found, actually store the data into our list of selected checks
    if(!found){
        return -ENOENT;
    } else if (found){

        bool unselected = true;
        struct entry_selected *sel = NULL;

        mutex_lock(&lock_list_selected);

        //__Check if plugin is already in list of selected
        list_for_each_entry(sel, &list_selected, list){
            if(sel->check == found){
                unselected = false;
                mutex_unlock(&lock_list_selected);
                return -EEXIST;
            }
        }

        if(unselected){
            pr_info("lkm: plugin %s was not in selected list. It will now be added.", sel->check->alias);
            //Allocate new entry_selected for list_selected
            sel = kmalloc(sizeof(*sel), GFP_KERNEL);
            if(!sel){
                mutex_unlock(&lock_list_selected);
                return -ENOMEM;
            }
            
            sel->check = found;
            list_add_tail(&sel->list, &list_selected);

            pr_info("lkm: added to 'selected' the check with alias: %s\n", found->alias);
        }

        mutex_unlock(&lock_list_selected);

    }

    return 0;
}

//--------------------------------------------------------------------------------


/**
 * Registration API Definition
 * @check: plugin check to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int core_register_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting registration\n", check->name);

    mutex_lock(&lock_list_available);
    pr_info("lkm: check %s began registration\n", check->name);

    struct entry_available *new_entry = NULL;
    new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
    if(!new_entry){
        mutex_unlock(&lock_list_available);
        return -ENOMEM;
    }

    new_entry->check = check;
    list_add_tail(&new_entry->list, &list_available);

    pr_info("lkm: check %s finished registration\n", check->name);
    mutex_unlock(&lock_list_available);

    return 0;
}
EXPORT_SYMBOL(core_register_check);

/**
 * Unregistration API Definition
 * @check: plugin check to unregister.
 * 
 * Unregistration.
 * We first remove the plugin from the "selected" array to be able
 */
void core_unregister_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting unregistration\n", check->name);

    //Removing plugin from "list_selected":
    struct entry_selected *pos;
    struct entry_selected *temp;

    mutex_lock(&lock_list_selected);
    
    list_for_each_entry_safe(pos, temp, &list_selected, list){
        if(pos->check == check){
            list_del(&pos->list);
            kfree(pos);
        }
    }
    mutex_unlock(&lock_list_selected);

    //Removing plugin from "available" list

    struct entry_available *pos_a;
    struct entry_available *temp_a;

    mutex_lock(&lock_list_available);
    pr_info("lkm: check %s began unregistration\n", check->name);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        if(pos_a->check == check){
            list_del(&pos_a->list);
            kfree(pos_a);
            break;
        }
    }
    pr_info("lkm: check %s finished unregistration\n", check->name);
    mutex_unlock(&lock_list_available);


    
}
EXPORT_SYMBOL(core_unregister_check);


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
        pr_info("-Deleting plugin from list of selected: %s\n", pos_s->check->alias);
        list_del(&pos_s->list);
        kfree(pos_s);
    }
    mutex_unlock(&lock_list_selected);

    //Free list_available
    struct entry_available *pos_a;
    struct entry_available *temp_a;

    mutex_lock(&lock_list_available);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        pr_info("-Deleting plugin from available ones: %s\n", pos_a->check->alias);
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