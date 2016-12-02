#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>

// The function is called on activation of the device
static int mydeviceback_probe(struct xenbus_device *dev,
			const struct xenbus_device_id *id)
{
	printk(KERN_NOTICE "Probe called. We are good to go.\n");
	return 0;
}

// This defines the name of the devices the driver reacts to
static const struct xenbus_device_id mydeviceback_ids[] = {
	{ "mydevice" },
	{ "" }
};

// We set up the callback functions
static struct xenbus_driver mydeviceback_driver = {
	.ids  = mydeviceback_ids,
	.probe = mydeviceback_probe,
};

// On loading this kernel module, we register as a frontend driver
static int __init mydeviceback_init(void)
{
	printk(KERN_NOTICE "Hello World!\n");

	return xenbus_register_backend(&mydeviceback_driver);
}
module_init(mydeviceback_init);

// ...and on unload we unregister
static void __exit mydeviceback_exit(void)
{
	xenbus_unregister_driver(&mydeviceback_driver);
	printk(KERN_ALERT "Goodbye world.\n");
}
module_exit(mydeviceback_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("xen-backend:mydevice");
