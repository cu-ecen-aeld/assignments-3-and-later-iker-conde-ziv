/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *p_aesd_dev = filp->private_data;
    struct aesd_buffer_entry *p_aesd_buffer_entry = NULL;
    size_t entry_offset_byte_rtn = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    if (p_aesd_dev == NULL) {
        mutex_unlock(&p_aesd_dev->lock);
        return -1;
    }
    mutex_lock(&p_aesd_dev->lock);
    p_aesd_buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&p_aesd_dev->circular_buffer, *f_pos, &entry_offset_byte_rtn);
    if (p_aesd_buffer_entry == NULL) {
        mutex_unlock(&p_aesd_dev->lock);
        return 0;
    }
    void *kernel_data = (void*)(p_aesd_buffer_entry->buffptr + entry_offset_byte_rtn);
    size_t kernel_data_size = p_aesd_buffer_entry->size - entry_offset_byte_rtn;
    if (kernel_data_size > count)
        kernel_data_size = count;
    if (copy_to_user(buf, kernel_data, kernel_data_size) != 0) {
        PDEBUG("Error copying data to user space\n");
        mutex_unlock(&p_aesd_dev->lock);
        return -1;
    }
    *f_pos = *f_pos + kernel_data_size;
    //*f_pos = *f_pos + kernel_data_size + 1;
    retval = kernel_data_size;
    mutex_unlock(&p_aesd_dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *p_aesd_dev = filp->private_data;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    mutex_lock(&p_aesd_dev->lock);
    void *new_buffer = kmalloc(count, GFP_KERNEL);
    if (new_buffer == NULL) {
        mutex_unlock(&p_aesd_dev->lock);
        return retval;
    }
    if (copy_from_user(new_buffer, buf, count) != 0) {
        PDEBUG("Error copying data to kernel space\n");
        mutex_unlock(&p_aesd_dev->lock);
        return -1;
    }

    if (p_aesd_dev->temp_buffer_size + count < sizeof(p_aesd_dev->temp_buffer)) {
        memcpy(p_aesd_dev->temp_buffer + p_aesd_dev->temp_buffer_size, new_buffer, count);
        p_aesd_dev->temp_buffer_size += count;

        if (p_aesd_dev->temp_buffer[p_aesd_dev->temp_buffer_size - 1] == '\n') {
            PDEBUG("Complete write: %s\n", p_aesd_dev->temp_buffer);

            memcpy(new_buffer, p_aesd_dev->temp_buffer, p_aesd_dev->temp_buffer_size);
            struct aesd_buffer_entry add_entry = {0};
            add_entry.buffptr = (char*)new_buffer;
            add_entry.size = p_aesd_dev->temp_buffer_size;
            const char* pointer_to_free = aesd_circular_buffer_add_entry(&p_aesd_dev->circular_buffer, &add_entry);
            if (!pointer_to_free)
                kfree(pointer_to_free);

            memset(p_aesd_dev->temp_buffer, 0, sizeof(p_aesd_dev->temp_buffer));
            p_aesd_dev->temp_buffer_size = 0;
        } else {
            PDEBUG("Partial write without newline, waiting for more data\n");
        }
        retval = count;
    } else {
        PDEBUG("Buffer overflow\n");
    }
    mutex_unlock(&p_aesd_dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *p_aesd_dev = filp->private_data;

    /* Get buffer size */
    loff_t buffer_size = 0;
    int i = 0;
    for (i = 0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        buffer_size =  buffer_size + p_aesd_dev->circular_buffer.entry[i].size;
    }
    return fixed_size_llseek(filp, offset, whence, buffer_size);
}

static long aesd_adjust_file_offset (struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    struct aesd_dev *p_aesd_dev = filp->private_data;

    /* Check write_cmd validity */
    int no_of_cmds = 0;
    for (no_of_cmds = 0; no_of_cmds<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; no_of_cmds++)
    {
        if (p_aesd_dev->circular_buffer.entry[no_of_cmds].size == 0)
            break;
    }
    if (no_of_cmds < write_cmd)
    {
        PDEBUG("Number of command does not exist\n");
        return -EINVAL;
    }

    /* Check write_cmd_offset validity */
    if (p_aesd_dev->circular_buffer.entry[write_cmd].size < write_cmd_offset)
    {
        PDEBUG("Offset of command is to big\n");
        return -EINVAL;
    }
    
    int i = 0;
    int cmd_offset = 0;
    for (i = 0; i<(write_cmd); i++)
    {
        cmd_offset = cmd_offset + p_aesd_dev->circular_buffer.entry[i].size;
    }

    long return_value = cmd_offset + write_cmd_offset;
    filp->f_pos = return_value;
    return return_value;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = -EINVAL;
    struct aesd_seekto seekto;
    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0)
            {
                return -EFAULT;
            }
            else
            {
                retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            }
            break;

        default:
            return -ENOTTY;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    memset(aesd_device.temp_buffer, 0, sizeof(aesd_device.temp_buffer));
    aesd_device.temp_buffer_size = 0;
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    mutex_unlock(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
