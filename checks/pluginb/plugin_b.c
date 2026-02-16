#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "lkm_check.h"

static int plugin_b_enumeration(struct seq_file *m);
static int __init plugin_init(void);
static void __exit plugin_exit(void);

static struct lkm_check plugin_b_check = {
    .api_version = LKM_CHECK_API_VERSION,
    .owner = THIS_MODULE,
    .name = "pluginb",
    .alias = "pluginb",
    .category = "sample",
    .run = plugin_b_enumeration,
};

/**
 * 
 * RCU locks usage and processes:
 * https://www.kernel.org/doc/Documentation/RCU/listRCU.rst
 * 
 * https://docs.kernel.org/core-api/printk-formats.html
 */
static int plugin_b_enumeration(struct seq_file *m){
    pr_info("Plugin B is saying hi!\n");

    struct task_struct *task;
    int count = 0;

    rcu_read_lock();
    for_each_process(task)
        count++;
    rcu_read_unlock();

    seq_printf(m,
        "--- Check %s ---\n"
        "- Total processes:%d\n", plugin_b_check.alias, count);
    return 0;
}

static int __init plugin_init(void){
    return core_register_check(&plugin_b_check);
}
module_init(plugin_init);

static void __exit plugin_exit(void){
    core_unregister_check(&plugin_b_check);
}
module_exit(plugin_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("pluginb");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Sample plugin for process enumeration");