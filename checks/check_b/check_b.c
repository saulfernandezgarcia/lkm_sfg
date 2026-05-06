// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "lkm_check.h"

static int check_b_enumeration(struct seq_file *m);
static int __init check_init(void);
static void __exit check_exit(void);

static struct lkm_check check_b_check = {
    .abi_version = LKM_CHECK_ABI_VERSION,
    .owner = THIS_MODULE,
    .name = "check_b",
    .alias = "check_b",
    .category = "sample",
    .run = check_b_enumeration,
};

/**
 * 
 * RCU locks usage and processes:
 * https://www.kernel.org/doc/Documentation/RCU/listRCU.rst
 * 
 * https://docs.kernel.org/core-api/printk-formats.html
 */
static int check_b_enumeration(struct seq_file *m){
    pr_info("Check B is saying hi!\n");

    struct task_struct *task;
    int count = 0;

    rcu_read_lock();
    for_each_process(task)
        count++;
    rcu_read_unlock();

    seq_printf(m,
        "--- Check %s ---\n"
        "- Total processes:%d\n", check_b_check.alias, count);
    return 0;
}

static int __init check_init(void){
    return core_register_check(&check_b_check);
}
module_init(check_init);

static void __exit check_exit(void){
    core_unregister_check(&check_b_check);
}
module_exit(check_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("check_b");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Sample check plugin for process enumeration");