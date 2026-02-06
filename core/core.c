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
static DEFINE_MUTEX(check_lock);

static struct dentry *lkm_dir;

// "available" file section

/**
 * fops_available contains "available_open" which calls "single_open" using "available_show".
 */
static int available_show(struct seq_file *m, void *v){
    struct lkm_check *c;

    mutex_lock(&check_lock);
    list_for_each_entry(c, &check_list, list){
        seq_printf(m, "%s\n", c->name);
    }
    mutex_unlock(&check_lock);

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



//-----------------------------------------------------

/**
 * Registration API Definition
 * @check: plugin check to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int core_register_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting registration\n", check->name);

    mutex_lock(&check_lock);
    pr_info("lkm: check %s began registration\n", check->name);
    list_add_tail(&(check->list), &check_list);
    pr_info("lkm: check %s finished registration\n", check->name);
    mutex_unlock(&check_lock);

    return 0;
}
EXPORT_SYMBOL(core_register_check);

/**
 * Unregistration API Definition
 * @check: plugin check to unregister.
 * 
 * Unregistration.
 */
void core_unregister_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting unregistration\n", check->name);

    mutex_lock(&check_lock);
    pr_info("lkm: check %s began unregistration\n", check->name);
    list_del(&check)
    pr_info("lkm: check %s finished unregistration\n", check->name);
    mutex_unlock(&check_lock);
}
EXPORT_SYMBOL(core_unregister_check);


/**
 * __init
 * 
 * https://docs.kernel.org/filesystems/debugfs.html
 */
static int __init core_init(void){
    pr_info("lkm CORE: initiating in kernel\n");
    
    lkm_dir = debugfs_create_dir("lkmsfg", NULL);

    pr_info("lkm CORE: creating interactive files\n");

    debugfs_create_file("available", 0444, lkm_dir, &ops_available);
    debugfs_create_file("order", 0644, lkm_dir, &ops_order);
    debugfs_create_file("results", 0444, lkm_dir, &ops_results);

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

    debugfs_remove(lkm_dir);

    pr_info("lkm CORE: removed from kernel\n");
}
module_exit(core_exit);

//-----------------------------------------------------
MODULE_LICENSE("GPL");
MODULE_ALIAS("sfgcore");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Development version of core for lkm management.");