#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/device.h>
#include <linux/irqreturn.h>

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/grant_table.h>

#include "../mydevice.h"


#define GRANT_INVALID_REF	0


struct mydevicefront_info {
	struct mydevice_rx_front_ring rx;
	int rx_ring_ref;

	unsigned int evtchn;
	unsigned int irq;
};

static irqreturn_t mydevicefront_interrupt(int irq, void *dev_id)
{
	struct mydevicefront_info *info = dev_id;
	struct mydevice_rx_response *rsp;

	while (RING_HAS_UNCONSUMED_RESPONSES(&info->rx)) {
		rsp = RING_GET_RESPONSE(&info->rx, info->rx.rsp_cons++);
		/* FIXME: schedule a softirq to print something */
		pr_info("Got a message: %s", rsp->msg);
	}

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

        /* End access and free the pages */
	if (info->rx_ring_ref != GRANT_INVALID_REF)
		gnttab_end_foreign_access(info->rx_ring_ref, 0, (unsigned long) info->rx.sring);

	kfree(info);

	pr_info("Removed mydevice\n");

	return 0;
}

static int setup_ring(struct xenbus_device *dev)
{
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);
	struct mydevice_rx_sring *rxs;
	grant_ref_t gref;
	int rc;

	info->rx_ring_ref = GRANT_INVALID_REF;
	info->rx.sring = NULL;

	rxs = (struct mydevice_rx_sring *) get_zeroed_page(GFP_NOIO | __GFP_HIGH);
	if (!rxs) {
		rc = -ENOMEM;
		xenbus_dev_fatal(dev, rc, "allocating ring page");
		return rc;
	}
	SHARED_RING_INIT(rxs);
	FRONT_RING_INIT(&info->rx, rxs, XEN_PAGE_SIZE);

	rc = xenbus_grant_ring(dev, rxs, 1, &gref);
	if (rc < 0) {
		free_page((unsigned long) rxs);
		return rc;
	}
	info->rx_ring_ref = gref;

	return 0;
}

static int setup_evtchn(struct xenbus_device *dev)
{
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);
	int err;

	err = xenbus_alloc_evtchn(dev, &info->evtchn);
	if (err < 0) {
		pr_err("xenbus_alloc_evtchn failed : %d\n", err);
		return err;
	}

	err = bind_evtchn_to_irqhandler(info->evtchn,
					mydevicefront_interrupt,
					0, dev->devicetype, info);
	if (err < 0) {
		pr_err("Failed to bind_evtchn_to_irqhandler: %d\n", err);
		goto error_evtchan;
	}
	info->irq = err;

	return 0;

error_evtchan:
	xenbus_free_evtchn(dev, info->evtchn);
	return err;

}

static int write_store(struct xenbus_device *dev)
{
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);
	struct xenbus_transaction xbt;
	int rc;

	rc = xenbus_transaction_start(&xbt);
	if (rc) {
		xenbus_dev_fatal(dev, rc, "starting transaction");
		return rc;
	}

	rc = xenbus_printf(xbt, dev->nodename, "rx-ring-ref", "%u", info->rx_ring_ref);
	if (rc) {
		xenbus_dev_fatal(dev, rc, "%s", "writing rx-ring-ring");
		return rc;
	}

	rc = xenbus_printf(xbt, dev->nodename, "event-channel", "%u", info->evtchn);
	if (rc) {
		xenbus_dev_fatal(dev, rc, "%s", "writing event-channel");
		return rc;
	}

	rc = xenbus_transaction_end(xbt, 0);
	if (rc) {
		xenbus_dev_fatal(dev, rc, "completing transaction");
		return rc;
	}

	return 0;
}

// This is where we set up xenstore files and event channels
static int frontend_connect(struct xenbus_device *dev)
{
	struct mydevicefront_info *info = dev_get_drvdata(&dev->dev);
	int rc;

	pr_info("Connecting the frontend now\n");

	rc = setup_ring(dev);
	if (rc < 0)
		return rc;

	rc = setup_evtchn(dev);
	if (rc < 0) {
		gnttab_end_foreign_access_ref(info->rx_ring_ref, 0);
		free_page((unsigned long) info->rx.sring);
		info->rx.sring = NULL;
	}

	rc = write_store(dev);

	/* FIXME: cleanup the above resources on error */

	return rc;
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
