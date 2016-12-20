#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>

// The function is called on activation of the device
static int mydevicefront_probe(struct xenbus_device *dev,
              const struct xenbus_device_id *id)
{
	printk(KERN_NOTICE "Probe called. We are good to go.\n");
	return 0;
}

// This is where we set up xenstore files and event channels
static int frontend_connect(struct xenbus_device *dev)
{
	pr_info("Connecting the frontend now\n");
	return 0;
}

// The function is called on a state change of the backend driver
static void mydevicefront_otherend_changed(struct xenbus_device *dev,
			    enum xenbus_state backend_state)
{
	switch (backend_state)
	{
		case XenbusStateInitialising:
			xenbus_switch_state(dev, XenbusStateInitialising);
			break;
		case XenbusStateInitialised:
		case XenbusStateReconfiguring:
		case XenbusStateReconfigured:
		case XenbusStateUnknown:
			break;

		case XenbusStateInitWait:
			if (dev->state != XenbusStateInitialising)
				break;
			if (frontend_connect(dev) != 0)
				break;

			xenbus_switch_state(dev, XenbusStateConnected);

			break;

		case XenbusStateConnected:
			pr_info("Other side says it is connected as well.\n");
			break;

		case XenbusStateClosed:
			if (dev->state == XenbusStateClosed)
				break;
			/* Missed the backend's CLOSING state -- fallthrough */
		case XenbusStateClosing:
			xenbus_frontend_closed(dev);
	}
}

// This defines the name of the devices the driver reacts to
static const struct xenbus_device_id mydevicefront_ids[] = {
	{ "mydevice"  },
	{ ""  }
};

// We set up the callback functions
static struct xenbus_driver mydevicefront_driver = {
	.ids  = mydevicefront_ids,
	.probe = mydevicefront_probe,
	.otherend_changed = mydevicefront_otherend_changed,
};

// On loading this kernel module, we register as a frontend driver
static int __init mydevice_init(void)
{
	printk(KERN_NOTICE "Hello World!\n");

	return xenbus_register_frontend(&mydevicefront_driver);
}
module_init(mydevice_init);

// ...and on unload we unregister
static void __exit mydevice_exit(void)
{
	xenbus_unregister_driver(&mydevicefront_driver);
	printk(KERN_ALERT "Goodbye world.\n");
}
module_exit(mydevice_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("xen:mydevice");
