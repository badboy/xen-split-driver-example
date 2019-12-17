#ifndef __MYDEVICE_H__
#define __MYDEVICE_H__

#include <xen/interface/io/ring.h>

struct mydevice_rx_request {
	char not_used_now;
};

struct mydevice_rx_response {
	char msg[32];
};

DEFINE_RING_TYPES(mydevice_rx, struct mydevice_rx_request,
		  struct mydevice_rx_response);
#endif
