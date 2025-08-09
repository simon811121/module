#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#define DEVICE_NAME "mychardev"
#define BUF_LEN 128

static int major;
static char msg[BUF_LEN] = {0};
static int msg_len = 0;

static DEFINE_MUTEX(my_mutex);       // 互斥鎖
static atomic_t device_opened = ATOMIC_INIT(1);  // 開啟次數

static int dev_open(struct inode *inode, struct file *file) {
    if (!atomic_dec_and_test(&device_opened)) {
        atomic_inc(&device_opened);
        printk(KERN_WARNING "mychardev: device is already opened!\n");
        return -EBUSY;
    }
    printk(KERN_INFO "mychardev: device opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    atomic_inc(&device_opened);
    printk(KERN_INFO "mychardev: device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    int bytes_read = 0;

    if (*offset >= msg_len) return 0;

    if (len > msg_len - *offset)
        len = msg_len - *offset;

    if (mutex_lock_interruptible(&my_mutex))
        return -ERESTARTSYS;

    if (copy_to_user(buffer, msg + *offset, len) != 0) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    *offset += len;
    bytes_read = len;

    mutex_unlock(&my_mutex);
    printk(KERN_INFO "mychardev: read %d bytes\n", bytes_read);
    return bytes_read;
}

static ssize_t dev_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    if (len > BUF_LEN)
        len = BUF_LEN;

    if (mutex_lock_interruptible(&my_mutex))
        return -ERESTARTSYS;

    if (copy_from_user(msg, buffer, len) != 0) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    msg[len] = '\0';
    msg_len = len;

    mutex_unlock(&my_mutex);
    printk(KERN_INFO "mychardev: wrote %zu bytes\n", len);
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init chardev_init(void) {
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "mychardev: register failed\n");
        return major;
    }

    mutex_init(&my_mutex);
    printk(KERN_INFO "mychardev: registered with major number %d\n", major);
    printk(KERN_INFO "Run: mknod /dev/%s c %d 0\n", DEVICE_NAME, major);
    return 0;
}

static void __exit chardev_exit(void) {
    unregister_chrdev(major, DEVICE_NAME);
    mutex_destroy(&my_mutex);
    printk(KERN_INFO "mychardev: unregistered device\n");
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Po-Hao Huang (Simon)");
MODULE_DESCRIPTION("Char device with mutex and open lock");