#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */

#include <xen/xen.h>       /* We are doing something with Xen */
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/page.h>

#include "../mydevice.h"

// This is where we set up path watchers and event channels
static void backend_connect(struct xenbus_device *dev)
{
	struct evtchn_bind_interdomain bind_interdomain;
	struct mydevice_rx_back_ring rx;
	struct mydevice_rx_response *rsp;
	struct mydevice_rx_sring *rxs;
	unsigned long rx_ring_ref;
	unsigned int evtchn;
	void *addr;
	int err;

	pr_info("Connecting the backend now\n");

	err = xenbus_gather(XBT_NIL, dev->otherend,
			    "rx-ring-ref", "%lu", &rx_ring_ref, NULL);
	if (err) {
		xenbus_dev_fatal(dev, err, "reading %s/ring-ref", dev->otherend);
		return;
	}

	err = xenbus_map_ring_valloc(dev, (grant_ref_t *) &rx_ring_ref, 1, &addr);
	if (err)
		return;		/* the error message will be saved in XenStore. */

	rxs = (struct mydevice_rx_sring *) addr;
	BACK_RING_INIT(&rx, rxs, XEN_PAGE_SIZE);

	/* Read the event channels allocated by domU */
	err = xenbus_gather(XBT_NIL, dev->otherend,
			    "event-channel", "%u", &evtchn, NULL);
	if (err < 0) {
		xenbus_dev_fatal(dev, err, "reading %s/event-channel", dev->otherend);
		return;
	}

	/* Allocates a new channel and binds it to the remote
	 * domain’s port. The new channel will be stored as
	 * bind_interdomain.local_port. */
	bind_interdomain.remote_dom  = dev->otherend_id;
	bind_interdomain.remote_port = evtchn;
	err = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
					  &bind_interdomain);
	if (err != 0) {
		pr_err("EVTCHNOP_bind_interdomain failed: %d\n", err);
		return;
	}

	rsp = RING_GET_RESPONSE(&rx, rx.rsp_prod_pvt++);
	snprintf(rsp->msg, sizeof(rsp->msg), "%s\n", "Hello world");
	RING_PUSH_RESPONSES(&rx);
	notify_remote_via_evtchn(bind_interdomain.local_port);
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
