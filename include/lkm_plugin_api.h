// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2026 Saúl Fernández García <https://github.com/saulfernandezgarcia>
 */

#ifndef _LKM_PLUGIN_API_H
#define _LKM_PLUGIN_API_H

#include "lkm_plugin.h"

/**
 * This header serves as API for the plugins
 */

int lkm_register_plugin(struct lkm_plugin *plugin);
void lkm_unregister_plugin(struct lkm_plugin *plugin);


#define LKM_REGISTER_PLUGIN(_plugin) \
static int __init __lkm_plugin_init(void){          \
    (_plugin).abi_version = LKM_ABI_VERSION;        \
    (_plugin).owner = THIS_MODULE;                  \
    return lkm_register_plugin(&(_plugin));         \
}                                                   \
                                                    \
static void __exit __lkm_plugin_exit(void){         \
    lkm_unregister_plugin(&(_plugin));              \
}                                                   \
                                                    \
module_init(__lkm_plugin_init);                     \
module_exit(__lkm_plugin_exit);


#endif