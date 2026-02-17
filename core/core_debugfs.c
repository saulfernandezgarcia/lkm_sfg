#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "core_internal.h"


static struct dentry *lkm_dir;

//--------------------------------------------------------------------------------
// Available

/**
 * This function will be called back (cb) by the core.
 */
static void available_cb(struct lkm_check *check, void*data){
    struct seq_file *m = data;
    seq_printf(m, "%s\n", check->alias);
}


/**
 * fops_available contains "available_open" which calls "single_open" using "available_show".
 */
static int available_show(struct seq_file *m, void *v){
    core_for_each_available(available_cb, m);
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

//--------------------------------------------------------------------------------
// Selected

static void selected_cb(struct lkm_check *check, void *data){
    struct seq_file *m = data;
    seq_printf(m, "%s\n", check->name);
}

static int selected_show(struct seq_file *m, void *v){
    core_for_each_selected(selected_cb, m);
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
    .open = selected_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

//--------------------------------------------------------------------------------
// Results

static void results_cb(struct lkm_check *check, void*data){
    struct seq_file *m = data;

    seq_printf(m, "==== %s ====\n", check->alias);
    check->run(m);
    seq_printf(m, "\n");
}

static int results_show(struct seq_file* m, void *v){
    core_for_each_selected(results_cb, m);
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

//--------------------------------------------------------------------------------
// Add

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
 * 
 * https://docs.kernel.org/core-api/kernel-api.html
 * https://c-for-dummies.com/blog/?p=1769
 * 
 * TODO: reconsider using simple_write_to_buffer
 */
static ssize_t add_write(struct file* file, const char __user *user_buffer, size_t size, loff_t *offset){
    //For now, only copying one item from the user to the file
    
    char my_kbuffer[256];
    int ret;

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
    



    // Tokenize, locate, and select according to passed name/alias
    // - best effort approach
    char *cur = my_kbuffer;
    char *token;
    int last_error = 0;
    const char* delimiters = " \t,";

    while((token = strsep(&cur, delimiters)) != NULL){
        if(*token != '\0'){
            ret = core_select_check(token);

            if(ret < 0)
                last_error = ret;
        }
    }


    //As per convention, return the number of written bytes
    if(last_error < 0){
        return last_error;
    } else {
        // Update pointer to offset from start of file
        *offset += size;

        return size;
    }
}


static const struct file_operations fops_add = {
    .owner = THIS_MODULE,
    .write = add_write,
};

//--------------------------------------------------------------------------------
// Empty

static ssize_t empty_write(struct file* file, const char __user *user_buffer, size_t size, loff_t *offset){
    core_empty_selected();
    return size;
}

static const struct file_operations fops_empty = {
    .owner = THIS_MODULE,
    .write = empty_write,
};

//--------------------------------------------------------------------------------
// Add all

static ssize_t addall_write(struct file* file, const char __user *user_buffer, size_t size, loff_t *offset){
    int ret = 0;
    
    ret = core_addall();
    
    if(ret < 0){
        return ret;
    }
    *offset += size;
    return size;
}

static const struct file_operations fops_addall = {
    .owner = THIS_MODULE,
    .write = addall_write,
};


//--------------------------------------------------------------------------------
// Remove specific

static ssize_t remove_write(struct file* file, const char __user *user_buffer, size_t size, loff_t *offset){
    //For now, only removing one item at a time

    char my_kbuffer[256];
    int ret;

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
    


    // Tokenize, locate, and remove according to passed name/alias
    // - best effort approach
    char *cur = my_kbuffer;
    char *token;
    int last_error = 0;
    const char* delimiters = " \t,";

    while((token = strsep(&cur, delimiters)) != NULL){
        if(*token != '\0'){
            ret = core_remove_check(token);

            if(ret < 0)
                last_error = ret;
        }
    }

    //As per convention, return the number of written bytes
    if(ret < 0){
        return ret;
    } else {
        // Update pointer to offset from start of file
        *offset += size;

        return size;
    }
}

static const struct file_operations fops_remove = {
    .owner = THIS_MODULE,
    .write = remove_write,
};

//--------------------------------------------------------------------------------


int core_debugfs_init(void){
    pr_info("lkm CORE: creating debugfs directory\n");

    lkm_dir = debugfs_create_dir("lkmsfg", NULL);
    if(!lkm_dir)
        return -ENOMEM;
    pr_info("lkm CORE: directory was created\n");

    pr_info("lkm CORE: creating interactive command files\n");

    debugfs_create_file("available", 0444, lkm_dir, NULL, &fops_available);
    debugfs_create_file("selected", 0444, lkm_dir, NULL, &fops_selected);
    debugfs_create_file("results", 0444, lkm_dir, NULL, &fops_results);
    debugfs_create_file("add", 0200, lkm_dir, NULL, &fops_add);
    debugfs_create_file("remove", 0200, lkm_dir, NULL, &fops_remove);
    debugfs_create_file("empty", 0200, lkm_dir, NULL, &fops_empty);
    debugfs_create_file("addall", 0200, lkm_dir, NULL, &fops_addall);

    return 0;
}

void core_debugfs_exit(void){
    debugfs_remove(lkm_dir);
}