/****************************************************************************
 *
 *                - DiscFerret Interface Library for C / C++ -
 *
 * Copyright (C) 2010 - 2011 P. A. Pemberton t/a. Red Fox Engineering.
 * All Rights Reserved.
 *
 * Copyright 2010-2011 Philip Pemberton
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>
#include "discferret.h"
#include "discferret_version.h"

/// USB timeout value, in milliseconds
#define USB_TIMEOUT 1000

/// DiscFerret hardware commands
enum {
	CMD_NOP					= 0,
	CMD_FPGA_INIT			= 1,
	CMD_FPGA_LOAD			= 2,
	CMD_FPGA_POLL			= 3,
	CMD_FPGA_POKE			= 4,
	CMD_FPGA_PEEK			= 5,
	CMD_RAM_ADDR_SET		= 6,
	CMD_RAM_ADDR_GET		= 7,
	CMD_RAM_WRITE			= 8,
	CMD_RAM_READ			= 9,
	CMD_RAM_WRITE_FAST		= 10,
	CMD_RAM_READ_FAST		= 11,
	CMD_RESET				= 0xFB,
	CMD_SECRET_SQUIRREL		= 0xFC,
	CMD_PROGRAM_SERIAL		= 0xFD,
	CMD_BOOTLOADER			= 0xFE,
	CMD_GET_VERSION			= 0xFF
};

/// DiscFerret hardware error codes
enum {
	FW_ERR_OK					= 0,
	FW_ERR_HARDWARE_ERROR		= 1,
	FW_ERR_INVALID_LEN			= 2,
	FW_ERR_FPGA_NOT_CONF		= 3,
	FW_ERR_FPGA_REFUSED_CONF	= 4,
	FW_ERR_INVALID_PARAM		= 5
};

/// DiscFerret library's libusb context
static libusb_context *usbctx = NULL;

/***
 * Microcode data -- in discferret_microcode.inc.c
 *
 * Yes, I know that #including C files directly is bad form, but in this case
 * we want to be able to update the microcode in the Makefile and not have to
 * export the MCODE to applications which use libdiscferret.
 */
#include "discferret_microcode.inc.c"

char* discferret_copyright_notice(void)
{
#ifndef NDEBUG
#define DBGSTR " (debug build)"
#else
#define DBGSTR " (release build)"
#endif

#ifdef CHECKOUT
	return "libdiscferret rev " HG_REV ", tag '" HG_TAG "'" DBGSTR " (C) 2010 P. A. Pemberton. <http://www.discferret.com/>";
#else
	return "libdiscferret release " VERSION DBGSTR " (C) 2010 P. A. Pemberton. <http://www.discferret.com/>";
#endif
#undef DBGSTR
}

/**
 * @brief	Swap the bits in a byte
 * 
 * Used by the RBF Uploader -- the PIC's MSSP sends bits to the FPGA config
 * port in reverse order. To save CPU time on the PIC, we swap the bits here,
 * then send the 'bitswapped' block instead.
 */
static unsigned char bitswap(unsigned char num)
{
	unsigned char val = 0;
	for (int i=0; i<8; i++)
		val = (val << 1) | ((num & 1<<i) ? 1 : 0);
	return val;
}

DISCFERRET_ERROR discferret_init(void)
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
	discferret_copyright_notice();

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_done(void)
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
		if (libusb_get_device_descriptor(usb_devices[i], &desc) != 0) continue;

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

void discferret_devlist_free(DISCFERRET_DEVICE **devlist)
{
	// Make sure devlist is non-null
	if (devlist == NULL) return;
	if ((*devlist) == NULL) return;

	// Free the device list
	free(*devlist);
}

DISCFERRET_ERROR discferret_open(const char *serialnum, DISCFERRET_DEVICE_HANDLE **dh)
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
				// Try and claim the primary interface
				int r = libusb_claim_interface(ldh, 0);
				if (r < 0) {
					// Failed to claim interface, so close the device and go around again.
					libusb_close(ldh);
					match = false;
					continue;
				} else {
					// Interface claimed! Pass the device handle back to the caller.
					*dh = malloc(sizeof(DISCFERRET_DEVICE_HANDLE));
					(*dh)->dh = ldh;

					// Pull the firmware version and set the capability flags
					if (discferret_update_capabilities(*dh) != DISCFERRET_E_OK) {
						libusb_close(ldh);
						match = false;
						continue;
					}

					// Set the initial track number to "unknown"
					(*dh)->current_track = -1;
					break;
				}
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

DISCFERRET_ERROR discferret_open_first(DISCFERRET_DEVICE_HANDLE **dh)
{
	return discferret_open(NULL, dh);
}

DISCFERRET_ERROR discferret_close(DISCFERRET_DEVICE_HANDLE *dh)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Close the device handle
	libusb_close(dh->dh);

	// Free allocated memory
	free(dh);

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_update_capabilities(DISCFERRET_DEVICE_HANDLE *dh)
{
	int err;
	DISCFERRET_DEVICE_INFO devinfo;

	if ((err = discferret_get_info(dh, &devinfo)) != DISCFERRET_E_OK) {
		return err;
	}

	// Set default values
	dh->has_fast_ram_access = false;
	dh->has_index_freq_sense = false;
	dh->has_index_freq_avail_flag = false;
	dh->index_freq_multiplier = 0;
	dh->has_track0_flag = false;

	// Firmware 001B added Fast RAM Access
	if (devinfo.firmware_ver >= 0x001B) {
		dh->has_fast_ram_access = true;
	}

	// Do we recognise this type of microcode?
	if (devinfo.microcode_type == 0xDD55) {
		// Yes -- it's the Baseline microcode.
		// Microcode 001F adds index frequency measurement at a low resolution
		if (devinfo.microcode_ver >= 0x001F) {
			dh->has_index_freq_sense = true;
			// 250us per step
			dh->index_freq_multiplier = 250.0e-6;
		}

		// Microcode 0020 improves index freq measurement resolution and adds
		// a 'new index measurement' flag.
		if (devinfo.microcode_ver >= 0x0020) {
			// 10us per step
			dh->index_freq_multiplier = 10.0e-6;
			dh->has_index_freq_avail_flag = true;
		}

		// Microcode 0021 adds a 'track0 reached during seek' flag
		if (devinfo.microcode_ver >= 0x0021) {
			dh->has_track0_flag = true;
		}
	}

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_get_info(DISCFERRET_DEVICE_HANDLE *dh, DISCFERRET_DEVICE_INFO *info)
{
	unsigned char buf[64];
	int i, r, a;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Send a GET VERSION command to the device
	i=0;
	buf[i++] = CMD_GET_VERSION;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 64, &a, USB_TIMEOUT);
	if ((r != 0) || (a < 11)) return DISCFERRET_E_USB_ERROR;

	// Decode the response packet
	for (i=1; i<5; i++)
		info->hardware_rev[i-1] = buf[i];
	info->hardware_rev[4] = '\0';
	info->firmware_ver		= (buf[5] << 8) + buf[6];
	info->microcode_type	= (buf[7] << 8) + buf[8];
	info->microcode_ver		= (buf[9] << 8) + buf[10];

	// Get string descriptors
	struct libusb_device_descriptor desc;
	libusb_get_device_descriptor(libusb_get_device(dh->dh), &desc);

	info->productname[0] = '\0';
	if (desc.iProduct != 0) {
		int len = libusb_get_string_descriptor_ascii(dh->dh, desc.iProduct, info->productname, sizeof(info->productname));
		if (len <= 0) info->productname[0] = '\0';
	}

	info->manufacturer[0] = '\0';
	if (desc.iManufacturer != 0) {
		int len = libusb_get_string_descriptor_ascii(dh->dh, desc.iManufacturer, info->manufacturer, sizeof(info->manufacturer));
		if (len <= 0) info->manufacturer[0] = '\0';
	}

	info->serialnumber[0] = '\0';
	if (desc.iSerialNumber != 0) {
		int len = libusb_get_string_descriptor_ascii(dh->dh, desc.iSerialNumber, info->serialnumber, sizeof(info->serialnumber));
		if (len <= 0) info->serialnumber[0] = '\0';
	}

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_fpga_load_begin(DISCFERRET_DEVICE_HANDLE *dh)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Send an FPGA_INIT command
	unsigned char buf = CMD_FPGA_INIT;
	int a, r;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, &buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, &buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf) {
		case FW_ERR_HARDWARE_ERROR:
			return DISCFERRET_E_HARDWARE_ERROR;
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

DISCFERRET_ERROR discferret_fpga_load_block(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *block, const size_t len, const bool swap)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Check that the block length does not exceed 62 bytes
	if (len > 62) return DISCFERRET_E_BAD_PARAMETER;

	unsigned char buf[64];
	int i = 0, a, r;
	// Command code and length
	buf[i++] = CMD_FPGA_LOAD;
	buf[i++] = len;
	if (!swap) {
		// Send without bitswap
		memcpy(&buf[i], block, len);
		i += len;
	} else {
		// Send with bitswap
		for (a=0; a<len; a++)
			buf[i++] = bitswap(block[a]);
	}
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf[0]) {
		case FW_ERR_INVALID_LEN:
			return DISCFERRET_E_BAD_PARAMETER;
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}


DISCFERRET_ERROR discferret_fpga_get_status(DISCFERRET_DEVICE_HANDLE *dh)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Send an FPGA_POLL command
	unsigned char buf = CMD_FPGA_POLL;
	int a, r;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, &buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, &buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf) {
		case FW_ERR_FPGA_NOT_CONF:
			return DISCFERRET_E_FPGA_NOT_CONFIGURED;
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

DISCFERRET_ERROR discferret_fpga_load_rbf(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *rbfdata, size_t len)
{
	int resp;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle and data block pointer are not NULL
	if ((dh == NULL) || (rbfdata == NULL)) return DISCFERRET_E_BAD_PARAMETER;

	// Start the load sequence
	resp = discferret_fpga_load_begin(dh);
	if (resp != DISCFERRET_E_OK) return resp;

	// Make sure the FPGA is in load mode
	resp = discferret_fpga_get_status(dh);
	if (resp != DISCFERRET_E_FPGA_NOT_CONFIGURED) return DISCFERRET_E_HARDWARE_ERROR;

	// Start loading blocks of RBF data
	size_t pos, i;
	pos = 0;
	while (pos < len) {
		// calculate largest block we can send without overflowing the device buffer
		i = ((len - pos) > 62) ? 62 : (len - pos);
		// send the block
		resp = discferret_fpga_load_block(dh, &rbfdata[pos], i, true);
		if (resp != DISCFERRET_E_OK) return resp;
		// update read pointer
		pos += i;
	}

	// Check that the FPGA load completed successfully
	resp = discferret_fpga_get_status(dh);
	if (resp != DISCFERRET_E_OK) return DISCFERRET_E_FPGA_NOT_CONFIGURED;

	// Load complete. Update the capability flags.
	return discferret_update_capabilities(dh);
}

int discferret_reg_peek(DISCFERRET_DEVICE_HANDLE *dh, unsigned int addr)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	unsigned char buf[64];
	int i = 0, a, r;
	// Command code and length
	buf[i++] = CMD_FPGA_PEEK;
	buf[i++] = addr >> 8;
	buf[i++] = addr & 0xff;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code and data byte
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 2, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 2)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf[0]) {
		case FW_ERR_OK:
			return buf[1];
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

DISCFERRET_ERROR discferret_fpga_load_default(DISCFERRET_DEVICE_HANDLE *dh)
{
	return discferret_fpga_load_rbf(dh, discferret_microcode, discferret_microcode_length);
}

DISCFERRET_ERROR discferret_reg_poke(DISCFERRET_DEVICE_HANDLE *dh, unsigned int addr, unsigned char data)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	unsigned char buf[64];
	int i = 0, a, r;
	// Command code and length
	buf[i++] = CMD_FPGA_POKE;
	buf[i++] = addr >> 8;
	buf[i++] = addr & 0xff;
	buf[i++] = data;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf[0]) {
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

long discferret_ram_addr_get(DISCFERRET_DEVICE_HANDLE *dh)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	unsigned char buf[64];
	int i = 0, a, r;
	// Command code and length
	buf[i++] = CMD_RAM_ADDR_GET;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code and data byte
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 4, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 4)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf[0]) {
		case FW_ERR_OK:
			return (buf[1]) + (buf[2] << 8) + (buf[3] << 16);
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

DISCFERRET_ERROR discferret_ram_addr_set(DISCFERRET_DEVICE_HANDLE *dh, unsigned long addr)
{
	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	unsigned char buf[64];
	int i = 0, a, r;
	// Command code and length
	buf[i++] = CMD_RAM_ADDR_SET;
	buf[i++] = addr & 0xff;
	buf[i++] = addr >> 8;
	buf[i++] = addr >> 16;
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, buf, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, buf, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (buf[0]) {
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

static int ramWrite_private(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *block, size_t len)
{
	unsigned char packet[65536+3];
	size_t i = 0;
	int r, a;

	if (dh->has_fast_ram_access) {
		// Fast Write can write up to 64K in a chunk
		if (len > 65536) return DISCFERRET_E_BAD_PARAMETER;
		packet[i++] = CMD_RAM_WRITE_FAST;
		packet[i++] = (len-1) & 0xff;
		packet[i++] = (len-1) >> 8;
	} else {
		// No FAST WRITE support, max 64 bytes less header
		if (len > (64-3)) return DISCFERRET_E_BAD_PARAMETER;
		packet[i++] = CMD_RAM_WRITE;
		packet[i++] = len;
		packet[i++] = len >> 8;
	}

	// Append data block to packet buffer
	memcpy(&packet[i], block, len);
	i += len;

	// Send the packet
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, packet, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	// Read the response code
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, packet, 1, &a, USB_TIMEOUT);
	if ((r != 0) || (a != 1)) return DISCFERRET_E_USB_ERROR;

	// Check the response code
	switch (packet[0]) {
		case FW_ERR_OK:
			return DISCFERRET_E_OK;
		default:
			return DISCFERRET_E_USB_ERROR;
	}
}

DISCFERRET_ERROR discferret_ram_write(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *block, size_t len)
{
	size_t blksz, pos, i;
	int resp;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Make sure block pointer is non-NULL, and length is > 0
	if (block == NULL) return DISCFERRET_E_BAD_PARAMETER;
	if (len == 0) return DISCFERRET_E_BAD_PARAMETER;

	if (dh->has_fast_ram_access)
		// Note that Fast Write can send 65536 bytes, but this involves sending
		// a final packet with only 3 bytes in it, which is a bit wasteful.
		// So instead we send less data, but fill all the packets we send.
		blksz = 65536-3;
	else
		// no Fast Write support, max 64 bytes in a packet, less 3 byte header
		blksz = 64-3;

	pos = 0;
	while (pos < len) {
		// Calculate largest possible block size
		i = ((len - pos) > blksz) ? blksz : (len - pos);
		// Send the data block
		resp = ramWrite_private(dh, &block[pos], i);
		if (resp != DISCFERRET_E_OK) return resp;
		// update read pointer
		pos += i;
	}

	return DISCFERRET_E_OK;
}

static int ramRead_private(DISCFERRET_DEVICE_HANDLE *dh, unsigned char *block, size_t len)
{
	unsigned char packet[65536+3];
	size_t i = 0;
	int r, a;

	if (dh->has_fast_ram_access) {
		// Fast Read can read up to 64K in a chunk
		if (len > 65536) return DISCFERRET_E_BAD_PARAMETER;
		packet[i++] = CMD_RAM_READ_FAST;
		packet[i++] = (len-1) & 0xff;
		packet[i++] = (len-1) >> 8;
	} else {
		// No FAST READ support, max 64 bytes less header
		if (len > (64-1)) return DISCFERRET_E_BAD_PARAMETER;
		packet[i++] = CMD_RAM_READ;
		packet[i++] = len;
		packet[i++] = len >> 8;
	}

	// Send the command packet
	r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_OUT, packet, i, &a, USB_TIMEOUT);
	if ((r != 0) || (a != i)) return DISCFERRET_E_USB_ERROR;

	if (dh->has_fast_ram_access) {
		// Fast Read: read the data block
		r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, packet, len, &a, USB_TIMEOUT);
		if ((r != 0) || (a != len)) return DISCFERRET_E_USB_ERROR;

		// Copy data block into user buffer
		memcpy(block, packet, len);

		return DISCFERRET_E_OK;
	} else {
		// Slow Read: read the response code and data block
		r = libusb_bulk_transfer(dh->dh, 1 | LIBUSB_ENDPOINT_IN, packet, len+1, &a, USB_TIMEOUT);
		if ((r != 0) || (a != (len+1))) return DISCFERRET_E_USB_ERROR;

		// Copy data block into user buffer
		memcpy(block, &packet[1], len);

		// Check the response code
		switch (packet[0]) {
			case FW_ERR_OK:
				return DISCFERRET_E_OK;
			default:
				return DISCFERRET_E_USB_ERROR;
		}
	}
}

DISCFERRET_ERROR discferret_ram_read(DISCFERRET_DEVICE_HANDLE *dh, unsigned char *block, size_t len)
{
	size_t blksz, pos, i;
	int resp;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Make sure block pointer is non-NULL, and length is > 0
	if (block == NULL) return DISCFERRET_E_BAD_PARAMETER;
	if (len == 0) return DISCFERRET_E_BAD_PARAMETER;

	if (dh->has_fast_ram_access)
		// Device has Fast Read support, 64K max packet size
		blksz = 65536;
	else
		// no Fast Read support, max 64 bytes in a packet, less 1-byte header
		blksz = 64-1;

	pos = 0;
	while (pos < len) {
		// Calculate largest possible block size
		i = ((len - pos) > blksz) ? blksz : (len - pos);
		// Read the data block
		resp = ramRead_private(dh, &block[pos], i);
		if (resp != DISCFERRET_E_OK) return resp;
		// update read pointer
		pos += i;
	}

	return DISCFERRET_E_OK;
}

long discferret_get_status(DISCFERRET_DEVICE_HANDLE *dh)
{
	int rva, rvb;

	// Check that the library has been initialised
	if (usbctx == NULL) return DISCFERRET_E_NOT_INIT;

	// Make sure device handle is not NULL
	if (dh == NULL) return DISCFERRET_E_BAD_PARAMETER;

	// Read the status registers
	if ((rva = discferret_reg_peek(dh, DISCFERRET_R_STATUS1)) < 0) return rva;
	if ((rvb = discferret_reg_peek(dh, DISCFERRET_R_STATUS2)) < 0) return rvb;

	return (rvb << 8) + rva;
}

DISCFERRET_ERROR discferret_get_index_time(DISCFERRET_DEVICE_HANDLE *dh, bool wait, double *timeval)
{
	int err;
	uint16_t i;

	// Make sure we have index frequency measurement support
	if (!dh->has_index_freq_sense) {
		return DISCFERRET_E_NOT_SUPPORTED;
	}

	// Wait for a new measurement if we've been asked to do so
	if (wait && dh->has_index_freq_avail_flag) {
		int x = 0;
		do {
			x = discferret_get_status(dh);
		} while ((x >= 0) && ((x & DISCFERRET_STATUS_NEW_INDEX_MEAS) == 0));
		if (x < 0) return x;
	}

	// Get the time measurement
	err = discferret_reg_peek(dh, DISCFERRET_R_INDEX_FREQ_HIGH);
	if (err < 0) return err;
	i = ((uint16_t)err) << 8;
	err = discferret_reg_peek(dh, DISCFERRET_R_INDEX_FREQ_LOW);
	if (err < 0) return err;
	i = i + (err & 0xff);

	// Convert number of counts into a real time value
	*timeval = ((double)i) * dh->index_freq_multiplier;

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_get_index_frequency(DISCFERRET_DEVICE_HANDLE *dh, bool wait, double *freqval)
{
	double tm;
	int err;

	// Get time taken for one revolution
	err = discferret_get_index_time(dh, wait, &tm);
	if (err < 0) return err;

	// Calculate rotation speed in RPM
	*freqval = 60.0 / tm;

	return DISCFERRET_E_OK;
}

DISCFERRET_ERROR discferret_seek_set_rate(DISCFERRET_DEVICE_HANDLE *dh, unsigned long steprate_us)
{
	unsigned long srval = (steprate_us / 250);

	// seek rate counter must be <= 255
	if (srval > 255)
		return DISCFERRET_E_BAD_PARAMETER;

	return discferret_reg_poke(dh, DISCFERRET_R_STEP_RATE, srval);
}

DISCFERRET_ERROR discferret_seek_recalibrate(DISCFERRET_DEVICE_HANDLE *dh, unsigned long maxsteps)
{
	unsigned long stepcnt = maxsteps;

	// max number of steps must be at least 1
	if (maxsteps < 1) {
		return DISCFERRET_E_BAD_PARAMETER;
	}

	// seek back, abort if we hit track 0
	bool track0_hit = false;
	while ((stepcnt > 0) && (!track0_hit)) {
		// figure out how many steps we can move
		unsigned long thisstep = (stepcnt > (DISCFERRET_STEP_COUNT_MASK+1)) ? (DISCFERRET_STEP_COUNT_MASK+1) : stepcnt;
		stepcnt -= thisstep;

		// now move the head by that number of steps
		discferret_reg_poke(dh, DISCFERRET_R_STEP_CMD, DISCFERRET_STEP_CMD_TOWARDS_ZERO | (thisstep-1));

		// wait for the seek to complete
		long status;
		do {
			// get discferret status
			status = discferret_get_status(dh);

			// if get_status returns an error code, pass it back to the caller
			if (status < 0)
				return status;
		} while ((status & DISCFERRET_STATUS_STEPPING) != 0);

		// did we reach track 0?
		if (dh->has_track0_flag) {
			if ((status & (DISCFERRET_STATUS_TRACK0_HIT | DISCFERRET_STATUS_TRACK0)) != 0)
				track0_hit = true;
		} else {
			if ((status & DISCFERRET_STATUS_TRACK0) != 0)
				track0_hit = true;
		}
	}

	// we're now either at track 0, or somewhere between last_known_track and track 0...
	// if we didn't hit T0, then the recalibrate failed.
	if (track0_hit) {
		dh->current_track = 0;
		return DISCFERRET_E_OK;
	} else {
		dh->current_track = -1;
		return DISCFERRET_E_RECAL_FAILED;
	}
}

DISCFERRET_ERROR discferret_seek_relative(DISCFERRET_DEVICE_HANDLE *dh, long numsteps)
{
	unsigned long stepcnt = (numsteps < 0) ? (-numsteps) : numsteps;

	// max number of steps must be at least 1
	if (stepcnt < 1) {
		return DISCFERRET_E_BAD_PARAMETER;
	}

	// seek, abort if we hit track 0
	bool track0_hit = false;
	while ((stepcnt > 0) && (!track0_hit)) {
		// figure out how many steps we can move
		unsigned long thisstep = (stepcnt > (DISCFERRET_STEP_COUNT_MASK+1)) ? (DISCFERRET_STEP_COUNT_MASK+1) : stepcnt;
		stepcnt -= thisstep;

		// now move the head by that number of steps
		if (numsteps < 0) {
			// seek towards zero
			discferret_reg_poke(dh, DISCFERRET_R_STEP_CMD, DISCFERRET_STEP_CMD_TOWARDS_ZERO | (thisstep-1));
		} else {
			// seek away from zero
			discferret_reg_poke(dh, DISCFERRET_R_STEP_CMD, DISCFERRET_STEP_CMD_AWAYFROM_ZERO | (thisstep-1));
		}

		// wait for the seek to complete
		long status;
		do {
			// get discferret status
			status = discferret_get_status(dh);

			// if get_status returns an error code, pass it back to the caller
			if (status < 0)
				return status;
		} while ((status & DISCFERRET_STATUS_STEPPING) != 0);

		// did we reach track 0?
		if (dh->has_track0_flag) {
			if ((status & (DISCFERRET_STATUS_TRACK0_HIT | DISCFERRET_STATUS_TRACK0)) != 0)
				track0_hit = true;
		} else {
			if ((status & DISCFERRET_STATUS_TRACK0) != 0)
				track0_hit = true;
		}
	}

	// we're now either at track 0, or at the track we requested
	if ((track0_hit) && (numsteps < 0)) {
		// hit track 0
		dh->current_track = 0;
		return DISCFERRET_E_TRACK0_REACHED;
	} else {
		// didn't hit track 0; update track count accordingly
		if (dh->current_track == -1) {
			// current track number unknown, leave it like that.
			return DISCFERRET_E_CURRENT_TRACK_UNKNOWN;
		} else {
			dh->current_track += numsteps;
			return DISCFERRET_E_OK;
		}
	}
}

DISCFERRET_ERROR discferret_seek_absolute(DISCFERRET_DEVICE_HANDLE *dh, unsigned long track)
{
	if (dh->current_track == -1)
		return DISCFERRET_E_CURRENT_TRACK_UNKNOWN;

	// track number is known. move the disc head.
	return discferret_seek_relative(dh, track - dh->current_track);
}

// vim: ts=4
