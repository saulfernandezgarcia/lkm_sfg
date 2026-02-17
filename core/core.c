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


/**
 * 
 * kmalloc calloc array allocation: kcalloc
 * https://www.kernel.org/doc/html/v5.0/core-api/mm-api.html#c.kzalloc
 */
void core_for_each_selected_run(
    void (*cb)(struct lkm_check *check, void *data),
    void*data){
    
    struct entry_selected *pos = NULL;
    struct lkm_check **snapshot = NULL;
    int count = 0;
    int i = 0;

    //Count how many checks to run and allocate array
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

    //Add checks to snapshot + pin them to avoid unregistration
    list_for_each_entry(pos, &list_selected, list){
        if(try_module_get(pos->check->owner)){
            snapshot[i] = pos->check;
            i++;
        }
    }
    mutex_unlock(&lock_list_selected);

    //Run the checks with no locked lists along the process
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
 * errno-base.h
 */
int core_select_check(const char *name){
    
    int available = 0;
    struct lkm_check *found = NULL;
    struct entry_available *pos = NULL;

    //Check to see if the plugin is available
    mutex_lock(&lock_list_available);
    list_for_each_entry(pos, &list_available, list){
        if(strcmp(pos->check->alias, name) == 0 || strcmp(pos->check->name, name) == 0){
            found = pos->check;
            available = 1;
            break;
        }
    }

    //If found, actually store the data into our list of selected checks
    if(!available){
        mutex_unlock(&lock_list_available);
        return -ENOENT;
    } else if (available){

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
            //__Take module reference for refcount
            if(!try_module_get(found->owner)){
                mutex_unlock(&lock_list_selected);
                return -EINVAL;
            }
            
            pr_info("lkm: plugin %s was not in selected list. It will now be added.\n", found->alias);
            //Allocate new entry_selected for list_selected
            sel = kzalloc(sizeof(*sel), GFP_KERNEL);
            if(!sel){
                module_put(found->owner);
                mutex_unlock(&lock_list_selected);
                return -ENOMEM;
            }
            
            sel->check = found;
            list_add_tail(&sel->list, &list_selected);

            pr_info("lkm: added to 'selected' the check with alias: %s\n", found->alias);
        }

        mutex_unlock(&lock_list_selected);
        mutex_unlock(&lock_list_available);

    }

    return 0;
}

/**
 * 
 * Best-effort
 */
int core_addall(void){


    struct entry_available *pos = NULL;
    struct entry_selected *sel = NULL;
    struct entry_selected *new_sel = NULL;

    int last_error = 0;

    mutex_lock(&lock_list_available);
    mutex_lock(&lock_list_selected);

    list_for_each_entry(pos, &list_available, list){

        int already = 0;
        list_for_each_entry(sel, &list_selected, list){
            if(sel->check == pos->check){
                already = 1;
                break;
            }
        }

        if(!already){
            if(!try_module_get(pos->check->owner)){
                last_error = -EINVAL;
            }

            new_sel = kzalloc(sizeof(*new_sel), GFP_KERNEL);
            if(!new_sel){
                module_put(pos->check->owner);
                last_error = -ENOMEM;
            }

            new_sel->check = pos->check;
            list_add_tail(&new_sel->list, &list_selected);
        }
    }

    mutex_unlock(&lock_list_selected);
    mutex_unlock(&lock_list_available);

    return last_error;
}

int core_remove_check(const char*name){
    struct entry_selected *pos;
    struct entry_selected *temp;
    int found = 0;

    mutex_lock(&lock_list_selected);
    list_for_each_entry_safe(pos, temp, &list_selected, list){
        if(strcmp(pos->check->alias, name) == 0 || strcmp(pos->check->name, name) == 0){
            list_del(&pos->list);
            pr_info("lkm: removed from 'selected' the check with alias: %s\n", pos->check->alias);
            module_put(pos->check->owner);
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
        module_put(pos->check->owner);
        kfree(pos);
    }
    mutex_unlock(&lock_list_selected);
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
    new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
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

    mutex_lock(&lock_list_available);
    mutex_lock(&lock_list_selected);

    //Removing plugin from "list_selected":
    struct entry_selected *pos_s;
    struct entry_selected *temp_s;
    
    list_for_each_entry_safe(pos_s, temp_s, &list_selected, list){
        if(pos_s->check == check){
            list_del(&pos_s->list);
            module_put(pos_s->check->owner);
            kfree(pos_s);
            break;
        }
    }

    //Removing plugin from "available" list
    struct entry_available *pos_a;
    struct entry_available *temp_a;

    pr_info("lkm: check %s began unregistration\n", check->name);

    list_for_each_entry_safe(pos_a, temp_a, &list_available, list){
        if(pos_a->check == check){
            list_del(&pos_a->list);
            kfree(pos_a);
            break;
        }
    }
    pr_info("lkm: check %s finished unregistration\n", check->name);

    mutex_unlock(&lock_list_selected);
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
        module_put(pos_s->check->owner);
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