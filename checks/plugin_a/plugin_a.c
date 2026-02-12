#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "lkm_check.h"

static int plugin_a_process(struct seq_file *m);
static int __init plugin_init(void);
static void __exit plugin_exit(void);

static struct lkm_check plugin_a_check = {
    .api_version = LKM_CHECK_API_VERSION,
    .owner = THIS_MODULE,
    .name = "plugina",
    .alias = "plugina",
    .category = "sample",
    .run = plugin_a_process,
};


static int plugin_a_process(struct seq_file *m){
    pr_info("Plugin A is saying hi!\n");
    seq_printf(m, "--- Plugin A is running its specific code!\n");
    return 0;
}

static int __init plugin_init(void){
    return core_register_check(&plugin_a_check);
}
module_init(plugin_init);

static void __exit plugin_exit(void){
    core_unregister_check(&plugin_a_check);
}
module_exit(plugin_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("plugina");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Sample Plugin");