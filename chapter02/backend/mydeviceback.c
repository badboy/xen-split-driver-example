#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>

// This is where we set up path watchers and event channels
static void backend_connect(struct xenbus_device *dev)
{
	pr_info("Connecting the backend now\n");
}

// This will destroy event channel handlers
static void backend_disconnect(struct xenbus_device *dev)
{
	pr_info("Connecting the backend now\n");
}

// We try to switch to the next state from a previous one
static void set_backend_state(struct xenbus_device *dev,
			      enum xenbus_state state)
{
	while (dev->state != state) {
		switch (dev->state) {
		case XenbusStateInitialising:
			switch (state) {
			case XenbusStateInitWait:
			case XenbusStateConnected:
			case XenbusStateClosing:
				xenbus_switch_state(dev, XenbusStateInitWait);
				break;
			case XenbusStateClosed:
				xenbus_switch_state(dev, XenbusStateClosed);
				break;
			default:
				BUG();
			}
			break;
		case XenbusStateClosed:
			switch (state) {
			case XenbusStateInitWait:
			case XenbusStateConnected:
				xenbus_switch_state(dev, XenbusStateInitWait);
				break;
			case XenbusStateClosing:
				xenbus_switch_state(dev, XenbusStateClosing);
				break;
			default:
				BUG();
			}
			break;
		case XenbusStateInitWait:
			switch (state) {
			case XenbusStateConnected:
				backend_connect(dev);
				xenbus_switch_state(dev, XenbusStateConnected);
				break;
			case XenbusStateClosing:
			case XenbusStateClosed:
				xenbus_switch_state(dev, XenbusStateClosing);
				break;
			default:
				BUG();
			}
			break;
		case XenbusStateConnected:
			switch (state) {
			case XenbusStateInitWait:
			case XenbusStateClosing:
			case XenbusStateClosed:
				backend_disconnect(dev);
				xenbus_switch_state(dev, XenbusStateClosing);
				break;
			default:
				BUG();
			}
			break;
		case XenbusStateClosing:
			switch (state) {
			case XenbusStateInitWait:
			case XenbusStateConnected:
			case XenbusStateClosed:
				xenbus_switch_state(dev, XenbusStateClosed);
				break;
			default:
				BUG();
			}
			break;
		default:
			BUG();
		}
	}
}

// The function is called on activation of the device
static int mydeviceback_probe(struct xenbus_device *dev,
			const struct xenbus_device_id *id)
{
	printk(KERN_NOTICE "Probe called. We are good to go.\n");

	xenbus_switch_state(dev, XenbusStateInitialising);
	return 0;
}

// The function is called on a state change of the frontend driver
static void mydeviceback_otherend_changed(struct xenbus_device *dev, enum xenbus_state frontend_state)
{
	switch (frontend_state) {
		case XenbusStateInitialising:
			set_backend_state(dev, XenbusStateInitWait);
			break;

		case XenbusStateInitialised:
			break;

		case XenbusStateConnected:
			set_backend_state(dev, XenbusStateConnected);
			break;

		case XenbusStateClosing:
			set_backend_state(dev, XenbusStateClosing);
			break;

		case XenbusStateClosed:
			set_backend_state(dev, XenbusStateClosed);
			if (xenbus_dev_is_online(dev))
				break;
			/* fall through if not online */
		case XenbusStateUnknown:
			set_backend_state(dev, XenbusStateClosed);
			device_unregister(&dev->dev);
			break;

		default:
			xenbus_dev_fatal(dev, -EINVAL, "saw state %s (%d) at frontend",
					xenbus_strstate(frontend_state), frontend_state);
			break;
	}
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
	.otherend_changed = mydeviceback_otherend_changed,
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
