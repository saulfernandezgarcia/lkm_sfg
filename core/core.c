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

#include "lkm_check.h"


static LIST_HEAD(check_list);
static DEFINE_MUTEX(lock_check_list);

static struct dentry *lkm_dir;


#define MAX_SELECTED_COUNT 32
static DEFINE_MUTEX(lock_selected);
static struct lkm_check* selected_checks[MAX_SELECTED_COUNT];
static int selected_checks_count;

//_________________________ "available" file section

static int available_show(struct seq_file *m, void *v);
static int available_open(struct inode *inode, struct file *file);


/**
 * fops_available contains "available_open" which calls "single_open" using "available_show".
 */
static int available_show(struct seq_file *m, void *v){
    struct lkm_check *c;

    mutex_lock(&lock_check_list);
    list_for_each_entry(c, &check_list, list){
        seq_printf(m, "%s\n", c->name);
    }
    mutex_unlock(&lock_check_list);

    return 0;
}

/**
 * @inode: must be passed as part of file_operations.open function definition.
 * @file:  
 * Using "single_open" instead of "seq_open" for simplicity (no large data handling)
 * Using "single_open" requires having "single_release" as .release
 */
static int available_open(struct inode *inode, struct file *file){
    return single_open(file, available_show, NULL);
}


/**
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=Making%20it%20all%20work%C2%B6
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=The%20other%20operations%20of%20interest%20%2D%20read%28%29%2C%20llseek%28%29%2C%20and%20release%28%29%20%2D%20are%20all%20implemented%20by%20the%20seq%5Ffile%20code%20itself%2E%20So%20a%20virtual%20file%E2%80%99s%20file%5Foperations%20structure%20will%20look%20like
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=The%20extra%2Dsimple%20version%C2%B6
 * 
 */
static const struct file_operations fops_available = {
    .owner = THIS_MODULE,
    .open = available_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};


//_________________________ "selected" file section

/**
 * https://tldp.org/LDP/lkmpg/2.4/html/c577.htm#:~:text=loff%5Ft%20%2A%29%3B-,static
 * https://stackoverflow.com/a/27722831
 * 
 * copy_from_user: https://elixir.bootlin.com/linux/v6.18.6/source/include/linux/uaccess.h#L205
 * 
 * In kernel space, copy a string: https://docs.kernel.org/core-api/kernel-api.html
 * - CAREFUL, REQUIRES KFREE AT SOME POINT!!!
 * 
 * We receive a "buffer" of length "len" from the user. 
 * We pass to "copy_from_user" a destination ("selected" file), the origin, and the 
 * length of data from the origin.
 * 
 * TODO: reconsider using simple_write_to_buffer
 */
static ssize_t selected_write(struct file* file, const char __user *user_buffer, size_t size, loff_t *offset){
    //For now, only copying one item from the user to the file
    
    char my_kbuffer[256];

    // Copy what the user wrote to our own buffer
    if (size >= sizeof(my_kbuffer) || size == 0)
        return -EINVAL;
    if (copy_from_user(my_kbuffer, user_buffer, size))
        return -EFAULT;
    
    //Because "echo" by the user adds a trailing newline at the end, 
    //we must change it to null terminated:
    if(size > 0 && my_kbuffer[size-1] == '\n')
        my_kbuffer[size-1] = '\0';
    else
        my_kbuffer[size] = '\0';
    
    // Update pointer to offset from start of file
    *offset += size;


    
    //Check to see if the plugin is available
    mutex_lock(&lock_check_list);

    struct lkm_check *c;
    struct lkm_check *found = NULL;
    list_for_each_entry(c, &check_list, list){
        if(strcmp(c->alias, my_kbuffer) == 0 || strcmp(c->name, my_kbuffer) == 0){
            found = c;
            break;
        }
    }
    mutex_unlock(&lock_check_list);

    //Actually store the data into our array of checks
    mutex_lock(&lock_selected);
    if(found){

        //__Check if plugin is not already included in "to run"
        int i;
        for(i = 0; i < selected_checks_count; i++){
            if(selected_checks[i] == found)
                break;
        }

        if(i == selected_checks_count && selected_checks_count < MAX_SELECTED_COUNT){
            pr_info("lkm: added to 'selected' the check with alias: %s\n", found->alias);
            selected_checks[selected_checks_count] = found;
            selected_checks_count++;
        }

    }
    mutex_unlock(&lock_selected);

    //As per convention, return the number of written bytes
    return size;
}

static int selected_show(struct seq_file *m, void *v){

    mutex_lock(&lock_selected);
    pr_info("lkm: printing selected checks\n");
    for(int i = 0; i < selected_checks_count; i++){
        seq_printf(m, "%s\n", selected_checks[i]->name);
    }
    mutex_unlock(&lock_selected);

    return 0;
}

static int selected_open(struct inode *inode, struct file* file){
    return single_open(file, selected_show, NULL);
}

/**
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=Making%20it%20all%20work%C2%B6
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=The%20other%20operations%20of%20interest%20%2D%20read%28%29%2C%20llseek%28%29%2C%20and%20release%28%29%20%2D%20are%20all%20implemented%20by%20the%20seq%5Ffile%20code%20itself%2E%20So%20a%20virtual%20file%E2%80%99s%20file%5Foperations%20structure%20will%20look%20like
 * https://docs.kernel.org/filesystems/seq_file.html#:~:text=The%20extra%2Dsimple%20version%C2%B6
 * 
 */
static const struct file_operations fops_selected = {
    .owner = THIS_MODULE,
    .write = selected_write,
    .open = selected_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};


//_________________________ "results" file section

static int results_show(struct seq_file* m, void *v){
    struct lkm_check *c = NULL;
    
    mutex_lock(&lock_check_list);
    for(int i = 0; i < selected_checks_count; i++){
        list_for_each_entry(c, &check_list, list){
            if(strcmp(c->alias, selected_checks[i]->alias) == 0 || strcmp(c->name, selected_checks[i]->name) == 0){
                seq_printf(m, "==== %s ====\n", c->alias);
                c->run(m);
                seq_printf(m, "\n");
            }
        }
    }
    mutex_unlock(&lock_check_list);

    return 0;
}

static int results_open(struct inode * inode, struct file* file){
    return single_open(file, results_show, NULL);
}

static const struct file_operations fops_results = {
    .owner = THIS_MODULE,
    .open = results_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};




//-----------------------------------------------------

/**
 * Registration API Definition
 * @check: plugin check to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int core_register_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting registration\n", check->name);

    mutex_lock(&lock_check_list);
    pr_info("lkm: check %s began registration\n", check->name);
    list_add_tail(&(check->list), &check_list);
    pr_info("lkm: check %s finished registration\n", check->name);
    mutex_unlock(&lock_check_list);

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

    //Removing plugin from "selected" array
    mutex_lock(&lock_selected);

    //__Check if plugin is not already included in "to run"
    int i;
    for(i = 0; i < selected_checks_count; i++){
        if(selected_checks[i] == check)
            break;
    }

    //__If found, remove it AND preserve order: shift left https://stackoverflow.com/a/12633220
    if(i < selected_checks_count){
        for(; i < selected_checks_count - 1; i++){
            selected_checks[i] = selected_checks[i+1];
        }
        selected_checks_count--;
    }
    mutex_unlock(&lock_selected);


    //Removing plugin from "available" list
    mutex_lock(&lock_check_list);
    pr_info("lkm: check %s began unregistration\n", check->name);
    list_del(&(check->list));
    pr_info("lkm: check %s finished unregistration\n", check->name);
    mutex_unlock(&lock_check_list);


    
}
EXPORT_SYMBOL(core_unregister_check);


/**
 * __init
 * 
 * https://docs.kernel.org/filesystems/debugfs.html
 * 
 * Debugfs files will be at /sys/kernel/debug/lkmsfg/
 */
static int __init core_init(void){
    pr_info("lkm CORE: initiating in kernel\n");
    
    lkm_dir = debugfs_create_dir("lkmsfg", NULL);

    pr_info("lkm CORE: creating interactive files\n");

    debugfs_create_file("available", 0444, lkm_dir, NULL, &fops_available);
    debugfs_create_file("selected", 0644, lkm_dir, NULL, &fops_selected);
    debugfs_create_file("results", 0444, lkm_dir, NULL, &fops_results);

    pr_info("lkm CORE: loaded into kernel\n");

    return 0;

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

    //Free allocated strings in selected_checks:
    mutex_lock(&lock_selected);
    pr_info("Emptying selected_checks.");
    selected_checks_count = 0;
    //kfree(selected_checks[i]); <-- should not do since plugin handles its ¡struct as a static struct,
    //so unloading the lkm implies freeing its static data
    mutex_unlock(&lock_selected);

    //Destroy list of available checks:
    struct lkm_check *c;
    struct lkm_check *temp_storage;
    mutex_lock(&lock_check_list);
    list_for_each_entry_safe(c, temp_storage, &check_list, list){
        pr_info("Deleting plugin from available ones: %s\n", c->name);
        list_del(&c->list);
    }
    mutex_unlock(&lock_check_list);

    //Remove debugfs:
    debugfs_remove(lkm_dir);

    pr_info("lkm CORE: removed from kernel\n");
}
module_exit(core_exit);

//-----------------------------------------------------
MODULE_LICENSE("GPL");
MODULE_ALIAS("sfgcore");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Development version of core for lkm management.");