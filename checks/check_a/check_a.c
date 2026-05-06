// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched/signal.h>

#include "lkm_check.h"

static int check_a_process(struct seq_file *m);
static int __init check_init(void);
static void __exit check_exit(void);

static struct lkm_check check_a_check = {
    .abi_version = LKM_CHECK_ABI_VERSION,
    .owner = THIS_MODULE,
    .name = "check_a",
    .alias = "check_a",
    .category = "sample",
    .run = check_a_process,
};


static int check_a_process(struct seq_file *m){
    pr_info("Check A is saying hi!\n");
    seq_printf(m, "--- Check A is running its specific code!\n");
    return 0;
}

static int __init check_init(void){
    return core_register_check(&check_a_check);
}
module_init(check_init);

static void __exit check_exit(void){
    core_unregister_check(&check_a_check);
}
module_exit(check_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("check_a");
MODULE_AUTHOR("SAUL FERNANDEZ GARCIA");
MODULE_DESCRIPTION("Sample check plugin for console printing.");