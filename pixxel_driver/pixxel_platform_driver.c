#include <linux/module.h>          /* Dynamic loading infrastructure (insmod/rmmod) */
#include <linux/init.h>            /* Layout macros like __init and __exit */
#include <linux/kernel.h>          /* Core types and log levels like pr_info/pr_err */
#include <linux/platform_device.h> /* Platform bus infrastructure matching DTB nodes */
#include <linux/io.h>              /* Memory-mapped I/O macros (ioread32/iowrite32) */
#include <linux/fs.h>              /* File operations structure and char device allocation */
#include <linux/cdev.h>            /* Character device management internal structures */
#include <linux/device.h>          /* Sysfs registration macros to auto-create /dev/ nodes */
#include <linux/uaccess.h>         /* Memory firewall utilities (copy_to_user/copy_from_user) */
#include <linux/of.h>
#include <linux/timer.h>   /* Kernel Timers for asynchronous delay loops */
#include <linux/jiffies.h> /* Kernel time unit conversions (msecs_to_jiffies) */

#define DRIVER_NAME "pixxel_virt_driver"
#define CLASS_NAME "pixxel_class"
#define DEVICE_NAME "pixxel"

/* Custom hardware register offsets as per provided in the assignment */
#define REG_ENABLE_OFFSET 0x00
#define REG_STATUS_OFFSET 0x04

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ashfaaq Khan");
MODULE_DESCRIPTION("Platform Character Driver for Pixxel Virtual Payload Device");

/* Simulated virtual registers — no real hardware at 0x60000000 in QEMU */
static u32 enable_reg = 0;
static u32 status_reg = 0;

/* Kernel space alloc struct*/
struct pixxel_virt_device
{
    dev_t dev_num;             /* Combines Major & Minor numbers into a single 32-bit ID */
    struct cdev cdev_struct;   /* Core Linux Virtual File System (VFS) representation of a char device */
    struct class *dev_class;   /* Sysfs class pointer used for dynamic node generation */
    struct device *dev_device; /* Concrete device pointer representing the node in /dev/ */
    void __iomem *base_addr;   /* Virtual kernel address mapped to the physical registers by the MMU */

    struct timer_list status_timer; /* Async kernel timer instance */
    u32 pending_status;             /* Holds the value to commit when timer fires */
};

static struct pixxel_virt_device my_pixxel_device;

static int pixxel_open(struct inode *inode, struct file *file);
static int pixxel_release(struct inode *inode, struct file *file);
static ssize_t pixxel_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset);
static ssize_t pixxel_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset);

/* The File Operations fops */
static const struct file_operations pixxel_fops = {
    .owner = THIS_MODULE,
    .open = pixxel_open,
    .release = pixxel_release,
    .read = pixxel_read,
    .write = pixxel_write,
};

/* Character Device System Call Logic handlers */

/* Triggered when user space calls: open in dev/pixxel */
static int pixxel_open(struct inode *inode, struct file *file)
{
    pr_info("%s: Device opened successfully by user application\n", DRIVER_NAME);
    return 0;
}

/* Triggered when user space calls: close(fd) */
static int pixxel_release(struct inode *inode, struct file *file)
{
    pr_info("%s: Device file descriptor released cleanly\n", DRIVER_NAME);
    return 0;
}

/* Triggered when user space calls: read(fd, buffer, length)*/
static ssize_t pixxel_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    int bytes_not_copied;

    if (*offset > 0)
        return 0;

    /* Securely read 32 bits from the physical status register (Offset 0x04) */
    pr_info("%s: Hardware register read request. Status matches: 0x%X\n", DRIVER_NAME, status_reg);

    bytes_not_copied = copy_to_user(user_buffer, &status_reg, sizeof(status_reg));
    if (bytes_not_copied != 0)
    {
        pr_err("%s: Failed to copy status data to user space context\n", DRIVER_NAME);
        return -EFAULT;
    }

    /* Advance file offset position marker by size of delivered packet */
    *offset += sizeof(status_reg);
    return sizeof(status_reg); /* Return exactly 4 bytes successfully processed */
}

/* Triggered when user space calls: write(fd, buffer, length)
 * Grabs the command integer from user space and commits it directly to hardware registers */
static ssize_t pixxel_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    u32 command_val = 0;
    int bytes_not_copied;

    /* Integrity Check reject if payload size is smaller than a single 32-bit uint */
    if (count < sizeof(u32))
    {
        pr_err("%s: Write payload rejected. Must pass a complete 4-byte 32-bit integer\n", DRIVER_NAME);
        return -EINVAL; /* Invalid Argument error */
    }

    /* Extract data payload safely out of unprivileged memory buffers */
    bytes_not_copied = copy_from_user(&command_val, user_buffer, sizeof(u32));
    if (bytes_not_copied != 0)
    {
        pr_err("%s: Failed to ingest command data from user space context\n", DRIVER_NAME);
        return -EFAULT;
    }

    pr_info("%s: Compiling write transaction. Value 0x%X committed to register 0x00\n", DRIVER_NAME, command_val);

    if (command_val == 1)
    {
        enable_reg = 1;
        my_pixxel_device.pending_status = 1;
    }
    else
    {
        enable_reg = 0;
        my_pixxel_device.pending_status = 0;
    }

    /* Schedule the timer to fire exactly 50 milliseconds from right now */
    mod_timer(&my_pixxel_device.status_timer, jiffies + msecs_to_jiffies(50));

    return count; /* Inform user space we successfully consumed all incoming bytes */
}

/* The Timer Callback Function: Executes 50ms after the write system call */
static void pixxel_timer_callback(struct timer_list *t)
{
    struct pixxel_virt_device *dev = timer_container_of(dev, t, status_timer);

    /* Commit the pending status directly to the hardware status register offset (0x04) */
    status_reg = dev->pending_status;
    pr_info("pixxel_virt_driver: Asynchronous 50ms delay complete. Status register updated to: 0x%X\n", dev->pending_status);
}

/* Probing, Resource Mapping allocation & Sysfs Creation
 * This runs immediately when the platform bus flags a match inside the Device Tree Blob (.dtb) */
static int pixxel_probe(struct platform_device *pdev)
{
    struct resource *res;
    int result;

    pr_info("%s: Match verified in Device Tree. Invoking probe lifecycle sequence...\n", DRIVER_NAME);

    /*Extract physical memory boundaries defined in the matching DTB node */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
    {
        pr_err("%s: Hardware register resource specification absent from DTB node\n", DRIVER_NAME);
        return -ENODEV;
    }

    /* Map physical space (0x60000000) into virtual kernel addressing limits using the MMU */
    my_pixxel_device.base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(my_pixxel_device.base_addr))
    {
        pr_err("%s: MMU Virtual address translation assignment failed\n", DRIVER_NAME);
        return PTR_ERR(my_pixxel_device.base_addr);
    }
    pr_info("%s: Physical register space 0x%pa mapped to virtual pointer %p\n", DRIVER_NAME, &res->start, my_pixxel_device.base_addr);

    /*Dynamically negotiate an unassigned Character Device Major and Minor identification group */
    result = alloc_chrdev_region(&my_pixxel_device.dev_num, 0, 1, DEVICE_NAME);
    if (result < 0)
    {
        pr_err("%s: Failed to negotiate dynamic major character file regions\n", DRIVER_NAME);
        return result;
    }
    pr_info("%s: Allocated Major ID: %d, Minor ID: %d\n", DRIVER_NAME, MAJOR(my_pixxel_device.dev_num), MINOR(my_pixxel_device.dev_num));

    /* Initialize character device structure variables and link them to the file operation handlers */
    cdev_init(&my_pixxel_device.cdev_struct, &pixxel_fops);
    my_pixxel_device.cdev_struct.owner = THIS_MODULE;
    result = cdev_add(&my_pixxel_device.cdev_struct, my_pixxel_device.dev_num, 1);
    if (result < 0)
    {
        pr_err("%s: Driver VFS interface link registration failed\n", DRIVER_NAME);
        unregister_chrdev_region(my_pixxel_device.dev_num, 1); /* Clean up allocated region on error */
        return result;
    }

    /* Create a Device Class entry inside Sysfs (/sys/class/pixxel_class) */
    my_pixxel_device.dev_class = class_create(CLASS_NAME);
    if (IS_ERR(my_pixxel_device.dev_class))
    {
        pr_err("%s: Class instantiation rejected by Sysfs framework\n", DRIVER_NAME);
        cdev_del(&my_pixxel_device.cdev_struct);
        unregister_chrdev_region(my_pixxel_device.dev_num, 1);
        return PTR_ERR(my_pixxel_device.dev_class);
    }

    /* Populate the visible communications gate file node at /dev/pixxel automatically */
    my_pixxel_device.dev_device = device_create(my_pixxel_device.dev_class, NULL, my_pixxel_device.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_pixxel_device.dev_device))
    {
        pr_err("%s: Target /dev/%s character file generation faulted\n", DRIVER_NAME, DEVICE_NAME);
        class_destroy(my_pixxel_device.dev_class);
        cdev_del(&my_pixxel_device.cdev_struct);
        unregister_chrdev_region(my_pixxel_device.dev_num, 1);
        return PTR_ERR(my_pixxel_device.dev_device);
    }

    timer_setup(&my_pixxel_device.status_timer, pixxel_timer_callback, 0);

    pr_info("%s: Device initialized cleanly! Interface available at /dev/%s\n", DRIVER_NAME, DEVICE_NAME);

    return 0;
}

/* Cleanup Routine, invoked when the driver module is unloaded or hardware unplugs */
static void pixxel_remove(struct platform_device *pdev)
{
    pr_info("%s: Terminating driver dependencies. Purging operational resources...\n", DRIVER_NAME);

    /* Tear down allocations in the exact reverse chronological order they were spun up */
    device_destroy(my_pixxel_device.dev_class, my_pixxel_device.dev_num);
    class_destroy(my_pixxel_device.dev_class);
    cdev_del(&my_pixxel_device.cdev_struct);
    unregister_chrdev_region(my_pixxel_device.dev_num, 1);
    timer_delete_sync(&my_pixxel_device.status_timer);

    pr_info("%s: All driver software assets completely scrubbed from the system pools.\n", DRIVER_NAME);
}

/* Identity table parsed by the kernel bus to pair our source file to a specific .dtb target node */
static const struct of_device_id pixxel_of_match[] = {
    {.compatible = "pixxel,virt-dev"},
    {} /* Null-terminated element placeholder to mark end of array list */
};
MODULE_DEVICE_TABLE(of, pixxel_of_match);

/* Structural registration configuration block defining the driver to the platform bus system */
static struct platform_driver pixxel_platform_driver = {
    .probe = pixxel_probe,
    .remove = pixxel_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = pixxel_of_match,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(pixxel_platform_driver);