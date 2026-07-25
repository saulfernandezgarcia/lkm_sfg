



int registry_add(struct lkm_plugin* plugin);

void registry_remove(struct lkm_plugin* plugin);

struct lkm_plugin* registry_find(const char* name);

/**
 * Allow for interaction with snapshot of contents in registry.
 */
void registry_snapshot(    
    void (*cb)(struct lkm_plugin *plugin, void *data),
    void *data);