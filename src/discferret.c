/**
 */

#include <string.h>
#include <malloc.h>
#include <libusb-1.0/libusb.h>
#include "discferret.h"
#include "discferret_version.h"

/// Global libusb context
libusb_context *usbctx = NULL;

char* discferret_copyright_notice(void)
{
	return "libdiscferret rev " HG_REV " (C) 2010 P. A. Pemberton. <http://www.discferret.com/>";
}

int discferret_init(void)
{
	int r;

	// Check if library has already been initialised
	if (usbctx != NULL)
		return DISCFERRET_E_ALREADY_INIT;

	// Initialise libusb
	if ((r = libusb_init(&usbctx)) < 0)
		return DISCFERRET_E_USB_ERROR;

	return DISCFERRET_E_OK;
}

int discferret_done(void)
{
	// Check if library has been initialised
	if (usbctx == NULL)
		return DISCFERRET_E_NOT_INIT;

	// Close down libusb
	libusb_exit(usbctx);
}

