#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/device.h>
#include <linux/irqreturn.h>

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/events.h>

struct mydevicefront_info {
	unsigned int evtchn;
	unsigned int irq;
};

static irqreturn_t mydevicefront_interrupt(int irq, void *dev_id)
{
	struct mydevicefront_info *info = dev_id;

	/* FIXME: print something in an interrupt */
	pr_info("Got an interrupt from event channel %d\n", info->evtchn);
	return IRQ_HANDLED;
}

// The function is called on activation of the device
static int mydevicefront_probe(struct xenbus_device *dev,
              const struct xenbus_device_id *id)
{
	struct mydevicefront_info *info;

	printk(KERN_NOTICE "Probe called. We are good to go.\n");

	info = kzalloc(sizeof(struct mydevicefront_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	dev_set_drvdata(&dev->dev, info);
	return 0;
}

static int mydevicefront_remove(struct xenbus_device *dev)
{
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);

	if (info->irq > 0)
		unbind_from_irqhandler(info->irq, info); /* It also frees the event channel */

	kfree(info);

	pr_info("Removed mydevice\n");

	return 0;
}

// This is where we set up xenstore files and event channels
static int frontend_connect(struct xenbus_device *dev)
{
	int err;
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);

	pr_info("Connecting the frontend now\n");

	err = xenbus_alloc_evtchn(dev, &info->evtchn);
	if (err < 0) {
		pr_err("xenbus_alloc_evtchn failed : %d\n", err);
		return err;
	}

	pr_info("Allocated a event channel: %d\n", info->evtchn);

	err = bind_evtchn_to_irqhandler(info->evtchn,
					mydevicefront_interrupt,
					0, dev->devicetype, info);
	if (err < 0) {
		pr_err("Failed to bind_evtchn_to_irqhandler: %d\n", err);
		goto error_evtchan;
	}
	info->irq = err;

	err = xenbus_printf(XBT_NIL, dev->nodename,
			    "event-channel", "%u", info->evtchn);
	if (err) {
		pr_err("Failed to write event-channel: %d\n", err);
		goto error_evtchan;
	}

	return 0;

error_evtchan:
	xenbus_free_evtchn(dev, info->evtchn);
	return err;
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
	.remove = mydevicefront_remove,
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
