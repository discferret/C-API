/**
 * @file discferret.c
 */

// TODO: remove stdio dep!
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>
#include "discferret.h"
#include "discferret_version.h"

/// Global libusb context
libusb_context *usbctx = NULL;

static char* ___discferret_copyright_notice(void)
{
#ifndef NDEBUG
#define DBGSTR " (debug build)"
#else
#define DBGSTR ""
#endif
	return "libdiscferret rev " HG_REV ", tag '" HG_TAG "'" DBGSTR " (C) 2010 P. A. Pemberton. <http://www.discferret.com/>";
#undef DBGSTR
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

#ifndef NDEBUG
	// Set libusb verbosity level
	libusb_set_debug(usbctx, 3);
#endif

	// Keep the copyright notice in the binary
	___discferret_copyright_notice();

	return DISCFERRET_E_OK;
}

int discferret_done(void)
{
	// Check if library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Close down libusb
	libusb_exit(usbctx);

	return DISCFERRET_E_OK;
}

int discferret_find_devices(DISCFERRET_DEVICE **devlist)
{
	int devcount = 0;
	libusb_device **usb_devices;
	int cnt;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Initialise the device list pointer
	if (devlist != NULL) *devlist = NULL;

	// Scan for libusb devices
	cnt = libusb_get_device_list(usbctx, &usb_devices);

	// If no devices (or an error), then we can't do anything...
	if (cnt <= 0) {
		*devlist = NULL;
		return (cnt < 0) ? DISCFERRET_E_USB_ERROR : 0;
	}

	// Device count more than 0. Loop through the device list looking for DiscFerrets.
	for (int i=0; i<cnt; i++) {
		// Read the device descriptor
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(usb_devices[i], &desc);

		// If this is a DiscFerret, then add it to the device list
		if ((desc.idVendor == 0x04d8) && (desc.idProduct == 0xfbbb)) {
			if (devlist != NULL) {
				// Try and open the device (to get the string descriptors)
				struct libusb_device_handle *dh;
				int r = libusb_open(usb_devices[i], &dh);
				if (r != 0) continue;

				// Allocate memory for the device descriptor
				DISCFERRET_DEVICE *tmp = realloc(*devlist, (devcount+1)*sizeof(DISCFERRET_DEVICE));

				// Did the memory allocation operation succeed?
				if (tmp == NULL) {
					free(*devlist);
					*devlist = NULL;
					return DISCFERRET_E_OUT_OF_MEMORY;
				} else {
					*devlist = tmp;
				}

				// Allocation succeeded. Fill in the info for the new device
				devlist[devcount]->vid = desc.idVendor;
				devlist[devcount]->pid = desc.idProduct;

				// Get string descriptors
				devlist[devcount]->productname[0] = '\0';
				if (desc.iProduct != 0) {
					int len = libusb_get_string_descriptor_ascii(dh, desc.iProduct, devlist[devcount]->productname, sizeof(devlist[devcount]->productname));
					if (len <= 0) devlist[devcount]->productname[0] = '\0';
				}

				devlist[devcount]->manufacturer[0] = '\0';
				if (desc.iManufacturer != 0) {
					int len = libusb_get_string_descriptor_ascii(dh, desc.iManufacturer, devlist[devcount]->manufacturer, sizeof(devlist[devcount]->manufacturer));
					if (len <= 0) devlist[devcount]->manufacturer[0] = '\0';
				}

				devlist[devcount]->serialnumber[0] = '\0';
				if (desc.iSerialNumber != 0) {
					int len = libusb_get_string_descriptor_ascii(dh, desc.iSerialNumber, devlist[devcount]->serialnumber, sizeof(devlist[devcount]->serialnumber));
					if (len <= 0) devlist[devcount]->serialnumber[0] = '\0';
				}
				// Close the device
				libusb_close(dh);
			}

			// Increment device counter
			devcount++;
		}
	}

	// We're done with the device list... free it.
	libusb_free_device_list(usb_devices, true);

	// Return number of devices we found
	return devcount;
}

int discferret_open(char *serialnum, DISCFERRET_DEVICE_HANDLE **dh)
{
	libusb_device **usb_devices;
	bool match = false;
	int cnt;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure the device handle is not null
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Scan for libusb devices
	cnt = libusb_get_device_list(usbctx, &usb_devices);

	// If no devices (or an error), then we can't do anything...
	if (cnt <= 0) {
		*dh = NULL;
		return (cnt < 0) ? DISCFERRET_E_USB_ERROR : 0;
	}

	// Device count more than 0. Loop through the device list looking for a
	// matching DiscFerret
	for (int i=0; i<cnt; i++) {

		// Read the device descriptor
		struct libusb_device_descriptor desc;
		libusb_get_device_descriptor(usb_devices[i], &desc);

		// If the VID, PID and serial number (if specified) match, open it.
		if ((desc.idVendor == 0x04d8) && (desc.idProduct == 0xfbbb)) {
			struct libusb_device_handle *ldh;

			// Are we matching on serial number?
			if ((serialnum == NULL) || (strlen(serialnum) == 0)) {
				// No, just opening a device based on VPID. Therefore, this is a match.
				int r = libusb_open(usb_devices[i], &ldh);
				if (r != 0) continue;
				match = true;
			} else {
				// VID/PID match, check the serial number
				int r = libusb_open(usb_devices[i], &ldh);
				if (r != 0) continue;

				// Get the serial number
				unsigned char sernum[256];
				if (desc.iSerialNumber != 0) {
					int len = libusb_get_string_descriptor_ascii(ldh, desc.iSerialNumber, sernum, sizeof(sernum));
					if (len <= 0) sernum[0] = '\0';

					// if lengths do not match, this is not a match!
					if (strlen(serialnum) != len) {
						libusb_close(ldh);
						continue;
					}

					if (strncmp((char *)sernum, (char *)serialnum, len) == 0) {
						// Serial number matches. This is a match!
						match = true;
					}
				}
			}

			// Do we have a match?
			if (match) {
				// May as well save the device handle (saves re-opening it later)
				*dh = malloc(sizeof(DISCFERRET_DEVICE_HANDLE));
				(*dh)->dh = ldh;
				break;
			}
		}
	}

	// We're done with the device list... free it.
	libusb_free_device_list(usb_devices, true);

	// Any matches?
	if (!match) {
		*dh = NULL;
		return DISCFERRET_E_NO_MATCH;
	} else {
		return DISCFERRET_E_OK;
	}
}

int discferret_open_first(DISCFERRET_DEVICE_HANDLE **dh)
{
	// Find the first available DiscFerret device and open it
	return discferret_open(NULL, dh);
}
