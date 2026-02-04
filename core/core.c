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


/**
 * Registration API Definition
 * @check: plugin check to register.
 * 
 * Registration is in queue fashion (list_add_tail).
 */
int core_register_check(struct lkm_check *check){
    pr_info("lkm: check %s requesting registration.\n", check->name);

    mutex_lock(&check_lock);
    pr_info("lkm: check %s began registration.\n", check->name);
    list_add_tail(&(check->list), &check_list);
    pr_info("lkm: check %s finished registration.\n", check->name);
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
    pr_info("lkm: check %s requesting unregistration.\n", check->name);

    mutex_lock(&check_lock);
    pr_info("lkm: check %s began unregistration.\n", check->name);
    list_del(&check)
    pr_info("lkm: check %s finished unregistration.\n", check->name);
    mutex_unlock(&check_lock);
}
EXPORT_SYMBOL(core_unregister_check);


/**
 * __init and __exit
 */
static int __init core_init(void){

}
module_init(core_init);

static void __exit core_exit(void){

}
module_exit(core_exit);

//-----------------------------------------------------
MODULE_LICENSE("GPL");
MODULE_ALIAS("sfgcore");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Development version of core for lkm management.");