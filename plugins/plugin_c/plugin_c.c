// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "lkm_plugin_api.h"

static int plugin_c_enumeration(struct seq_file *m);

static struct lkm_plugin plugin_c = {
    .name = "plugin_c",
    .alias = "process_enum",
    .category = "enumeration",
    .run = plugin_c_enumeration,
};

LKM_REGISTER_PLUGIN(plugin_c);

/**
 * 
 * RCU locks usage and processes:
 * https://www.kernel.org/doc/Documentation/RCU/listRCU.rst
 * 
 * https://docs.kernel.org/core-api/printk-formats.html
 */
static int plugin_c_enumeration(struct seq_file *m){
    pr_info("Plugin C is saying hi!\n");

    struct task_struct *task;
    int count = 0;

    rcu_read_lock();
    for_each_process(task)
        count++;
    rcu_read_unlock();

    seq_printf(m,
        "--- Plugin %s ---\n"
        "- Total processes:%d\n", plugin_c.alias, count);
    return 0;
}


MODULE_LICENSE("GPL");
MODULE_ALIAS("plugin_c");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Sample plugin for process enumeration");