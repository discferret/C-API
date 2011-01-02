/**
/ * @file discferret.c
 */

#include <string.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>
#include "discferret.h"
#include "discferret_version.h"

/// USB timeout value, in milliseconds
#define USB_TIMEOUT 100

/// DiscFerret control registers (write only)
enum {
	DRIVE_CONTROL			= 0x04,
	ACQCON					= 0x05,
	ACQ_START_EVT			= 0x06,
	ACQ_STOP_EVT			= 0x07,
	ACQ_START_NUM			= 0x08,
	ACQ_STOP_NUM			= 0x09,

	ACQ_HSTMD_THR_START		= 0x10,
	ACQ_HSTMD_THR_STOP		= 0x11,

	MFM_SYNCWORD_START_L	= 0x20,
	MFM_SYNCWORD_START_H	= 0x21,
	MFM_SYNCWORD_STOP_L		= 0x22,
	MFM_SYNCWORD_STOP_H		= 0x23,
	MFM_MASK_START_L		= 0x24,
	MFM_MASK_START_H		= 0x25,
	MFM_MASK_STOP_L			= 0x26,
	MFM_MASK_STOP_H			= 0x27,
	MFM_CLKSEL				= 0x2F,	// MFM clock select

	SCRATCHPAD				= 0x30,
	INVERSE_SCRATCHPAD		= 0x31,
	FIXED55					= 0x32,
	FIXEDAA					= 0x33,
	CLOCK_TICKER			= 0x34,
	CLOCK_TICKER_PLL		= 0x35,

	HSIO_DIR				= 0xE0,	// HSIO bit-bang pin direction
	HSIO_PIN				= 0xE1,	// HSIO pins

	STEP_RATE				= 0xF0,	// step rate, 250us per count
	STEP_CMD				= 0xFF	// step command, bit7=direction, rest=num steps
};

/// Status registers (read only)
enum {
	STATUS1					= 0x0E,
	STATUS2					= 0x0F
};

/// Step Command bits
enum {
	STEP_CMD_TOWARDS_ZERO	= 0x80,
	STEP_CMD_AWAYFROM_ZERO	= 0x00,
	STEP_COUNT_MASK			= 0x7F
};

/// DRIVE_CONTROL bits
enum {
	DRIVE_CONTROL_DENSITY	= 0x01,
	DRIVE_CONTROL_INUSE		= 0x02,
	DRIVE_CONTROL_DS0		= 0x04,
	DRIVE_CONTROL_DS1		= 0x08,
	DRIVE_CONTROL_DS2		= 0x10,
	DRIVE_CONTROL_DS3		= 0x20,
	DRIVE_CONTROL_MOTEN		= 0x40,
	DRIVE_CONTROL_SIDESEL	= 0x80
};

/// ACQCON bits
enum {
	ACQCON_WRITE			= 0x04,
	ACQCON_ABORT			= 0x02,
	ACQCON_START			= 0x01
};

/// masks and events for ACQ_*_EVT registers
enum {
	ACQ_EVENT_IMMEDIATE		= 0x00,
	ACQ_EVENT_INDEX			= 0x01,
	ACQ_EVENT_MFM			= 0x02,
	// "wait for HSTMD before acq" combination bit
	ACQ_EVENT_WAIT_HSTMD	= 0x80
};

/// legal MFM_CLKSEL values
enum {
	MFM_CLKSEL_1MBPS		= 0x00,
	MFM_CLKSEL_500KBPS		= 0x01,
	MFM_CLKSEL_250KBPS		= 0x02,
	MFM_CLKSEL_125KBPS		= 0x03
};

/// STATUS1 register bits
enum {
	STATUS1_ACQSTATUS_MASK	= 0x07,
	STATUS1_ACQ_WRITING		= 0x04,
	STATUS1_ACQ_WAITING		= 0x02,
	STATUS1_ACQ_ACQUIRING	= 0x01,
	STATUS1_ACQ_IDLE		= 0x00
};

/// STATUS2 register bits
enum {
	STATUS2_INDEX			= 0x80,
	STATUS2_TRACK0			= 0x40,
	STATUS2_WRITE_PROTECT	= 0x20,
	STATUS2_DISC_CHANGE		= 0x10,
	STATUS2_DENSITY			= 0x08,
	STATUS2_STEPPING		= 0x04,
	STATUS2_RAM_EMPTY		= 0x02,
	STATUS2_RAM_FULL		= 0x01
};

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

static char* discferret_copyright_notice(void)
{
#ifndef NDEBUG
#define DBGSTR " (debug build)"
#else
#define DBGSTR ""
#endif
	return "libdiscferret rev " HG_REV ", tag '" HG_TAG "'" DBGSTR " (C) 2010 P. A. Pemberton. <http://www.discferret.com/>";
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
	unsigned char val;
	for (int i=0; i<8; i++)
		val = (val << 1) | ((num & 1<<i) ? 1 : 0);
	return val;
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
	discferret_copyright_notice();

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

					// TODO: get firmware version and set capability flags
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

int discferret_open_first(DISCFERRET_DEVICE_HANDLE **dh)
{
	return discferret_open(NULL, dh);
}

int discferret_close(DISCFERRET_DEVICE_HANDLE *dh)
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

int discferret_get_info(DISCFERRET_DEVICE_HANDLE *dh, DISCFERRET_DEVICE_INFO *info)
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

int discferret_fpga_load_begin(DISCFERRET_DEVICE_HANDLE *dh)
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

int discferret_fpga_get_status(DISCFERRET_DEVICE_HANDLE *dh)
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
