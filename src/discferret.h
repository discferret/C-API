/****************************************************************************
 *
 *                - DiscFerret Interface Library for C / C++ -
 *
 * Copyright (C) 2010 - 2011 P. A. Pemberton t/a. Red Fox Engineering.
 * All Rights Reserved.
 *
 * This library (libdiscferret.so, discferret.c, discferret.h) is distributed
 * under the Apache License, version 2.0. Distribution is permitted under the
 * terms of the aforementioned License.
 ****************************************************************************/

/**
 * @file discferret.h
 * @brief DiscFerret interface library (libdiscferret)
 * @author Philip Pemberton
 * @date 2010-2011
 */

#ifndef _DISCFERRET_H
#define _DISCFERRET_H

#include <stdbool.h>
#include <libusb-1.0/libusb.h>
#include "discferret_registers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	A structure to encapsulate information about a specific DiscFerret device.
 */
typedef struct {
	unsigned char	productname[256];	///< Device product name.
	unsigned char	manufacturer[256];	///< Device manufacturer.
	unsigned char	serialnumber[256];	///< Device serial number.
	uint16_t		vid;				///< USB Vendor ID.
	uint16_t		pid;				///< USB Product ID.
} DISCFERRET_DEVICE;

/**
 * @brief	Handle to an open DiscFerret device.
 */
typedef struct {
	struct libusb_device_handle *dh;	///< Libusb device handle.
	bool	has_fast_ram_access;		///< True if device supports Fast RAM R/W operations
} DISCFERRET_DEVICE_HANDLE;

/**
 * @brief	Structure to contain information about a DiscFerret device.
 */
typedef struct {
	unsigned int	firmware_ver;		///< Firmware version
	unsigned int	microcode_type;		///< Microcode type
	unsigned int	microcode_ver;		///< Microcode version
	char			hardware_rev[5];	///< Hardware revision
	unsigned char	productname[256];	///< Device product name.
	unsigned char	manufacturer[256];	///< Device manufacturer.
	unsigned char	serialnumber[256];	///< Device serial number.
} DISCFERRET_DEVICE_INFO;

/**
 * @brief	DiscFerret library error codes.
 */
typedef enum {
	DISCFERRET_E_OK				=	0,		///< Operation succeeded.
	DISCFERRET_E_ALREADY_INIT	=	-1024,	///< Library already initialised
	DISCFERRET_E_NOT_INIT,					///< Library not initialised yet
	DISCFERRET_E_BAD_PARAMETER,				///< Bad parameter passed to a library function
	DISCFERRET_E_USB_ERROR,					///< USB error
	DISCFERRET_E_OUT_OF_MEMORY,				///< Out of memory
	DISCFERRET_E_NO_MATCH,					///< Unable to find a device matching the specified search criteria
	DISCFERRET_E_HARDWARE_ERROR,			///< Hardware error (malfunction)
	DISCFERRET_E_FPGA_NOT_CONFIGURED,		///< FPGA not configured (microcode not loaded)
} DISCFERRET_ERROR;

/**
 * @brief	Initialise libDiscFerret.
 * @note	Must be called before calling any other discferret_* functions.
 * @returns	Error code, or DISCFERRET_E_OK on success.
 */
DISCFERRET_ERROR discferret_init(void);

/**
 * @brief	Shut down libDiscFerret.
 * @note	Must be called after all DiscFerret calls have completed, but before the user application exits.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 */
DISCFERRET_ERROR discferret_done(void);

/**
 * @brief	Enumerate all available DiscFerret devices.
 * @param	devlist		Pointer to a DISCFERRET_DEVICE* block, or NULL.
 * @returns	Number of devices found, or one of the DISCFERRET_E_* error constants.
 *
 * This function scans the system for available DiscFerret devices which have
 * not been claimed by another process. It returns either a DISCFERRET_E_xxx
 * constant (if there was an error), or a count of the number of available
 * devices.
 *
 * If devlist is set to NULL, then a count of devices will be performed, but
 * no device list will be returned (for obvious reasons).
 *
 * A return code of 0 means the function succeeded, but no devices were found.
 */
int discferret_find_devices(DISCFERRET_DEVICE **devlist);

/**
 * @brief	Open a DiscFerret with a given serial number
 * @param	serialnum	Serial number of the DiscFerret unit to open.
 * @param	dh			Pointer to a pointer to a DiscFerret Device Handle,
 * 						which will store the device handle.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Opens a DiscFerret device, and returns the device handle. If device opening
 * fails, then one of the DISCFERRET_E_xxx constants will be returned, and
 * *dh will be set to NULL.
 *
 * dh (the pointer-to-a-pointer) MUST NOT be set to NULL.
 */
int discferret_open(char *serialnum, DISCFERRET_DEVICE_HANDLE **dh);

/**
 * @brief	Open the first available DiscFerret device.
 * @param	dh			Pointer to a pointer to a DiscFerret Device Handle,
 * 						which will store the device handle.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * This is an alternative to the process of calling discferret_find_devices()
 * and discferret_open(), and is intended for situations where only one
 * DiscFerret is known to be attached to the system.
 *
 * A search is run for DiscFerret units, and the first available (unclaimed)
 * DiscFerret unit is claimed, opened and a handle returned.
 *
 * dh (the pointer-to-a-pointer) MUST NOT be set to NULL.
 */
int discferret_open_first(DISCFERRET_DEVICE_HANDLE **dh);

/**
 * @brief	Close a DiscFerret device.
 * @param	dh		DiscFerret device handle.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Closes a DiscFerret device handle which was obtained by calling
 * discferret_open() or discferret_open_first(). This function MUST be called
 * after the application is finished with the DiscFerret.
 */
int discferret_close(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Retrieve a DiscFerret's unique ID and firmware version information.
 * @param	dh		DiscFerret device handle.
 * @param	info	Pointer to a DISCFERRET_DEVICE_INFO block where the version info will be stored.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Obtains the DiscFerret device's version information, including hardware,
 * firmware and microcode version information. Microcode version information
 * is (obviously) only valid if a valid microcode image has been loaded.
 *
 * If the hardware version and/or serial number have not been programmed,
 * these will generally read as '????' or an empty string, though this is
 * not guaranteed.
 */
int discferret_get_info(DISCFERRET_DEVICE_HANDLE *dh, DISCFERRET_DEVICE_INFO *info);

/**
 * @brief	Begin loading FPGA microcode.
 * @param	dh		DiscFerret device handle.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Call this function once at the beginning of an FPGA microcode load cycle.
 * A typical load looks like this:
 *   1. A call to discferret_fpga_load_begin()
 *   2. One or more calls to discferret_fpga_load_block() or
 *      discferret_fpga_load_rbf()
 *   3. A call to discferret_fpga_get_status() to see if the FPGA accepted
 *      the microcode load.
 *
 * User applications should consider using discferret_fpga_load_rbf() instead,
 * as this is encapsulates the entire sequence as shown above, and markedly
 * simplifies user code.
 */
int discferret_fpga_load_begin(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Load a block of microcode into the FPGA (max 62 bytes)
 * @param	dh		DiscFerret device handle.
 * @param	block	Data block to load.
 * @param	len		Length of data block.
 * @param	swap	TRUE to bitswap the data before sending it.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Loads a block of microcode data (maximum length 62 bytes) into the FPGA,
 * optionally bitswapping the data before sending it.
 *
 * User applications should consider using discferret_fpga_load_rbf() instead,
 * as this is encapsulates the entire loading sequence, and markedly simplifies
 * user code.
 */
int discferret_fpga_load_block(DISCFERRET_DEVICE_HANDLE *dh, unsigned char *block, size_t len, bool swap);

/**
 * @brief	Get the current status of the DiscFerret's FPGA
 * @param	dh		DiscFerret device handle.
 * @returns DISCFERRET_E_OK if microcode is loaded,
 * 			DISCFERRET_E_FPGA_NOT_CONFIGURED if FPGA is not configured, or
 * 			one of the DISCFERRET_E_xxx constants on error.
 *
 * Requests the current status of the FPGA from the DiscFerret. If the return
 * code is DISCFERRET_E_OK, then microcode has been loaded, and the Microcode
 * Version and Microcode Type returned by discferret_get_info() can be
 * reasonably assumed to be accurate and valid. If the return code is
 * DISCFERRET_E_FPGA_NOT_CONFIGURED, then the microcode has not yet been loaded
 * into the FPGA, and the DiscFerret will not function.
 */
int discferret_fpga_get_status(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Load an RBF-format microcode image into the DiscFerret's FPGA
 * @param	dh		DiscFerret device handle.
 * @param	rbfdata	Pointer to RBF data, as read from the .RBF file.
 * @param	len		Length of the RBF data buffer.
 * @returns DISCFERRET_E_OK if microcode was loaded successfully,
 * 			DISCFERRET_E_BAD_PARAMETER if one or more parameters were invalid,
 * 			DISCFERRET_E_HARDWARE_ERROR if FPGA failed to enter load mode,
 * 			DISCFERRET_E_FPGA_NOT_CONFIGURED if FPGA rejected the config load.
 */
int discferret_fpga_load_rbf(DISCFERRET_DEVICE_HANDLE *dh, unsigned char *rbfdata, size_t len);

/**
 * @brief	Read the contents of a DiscFerret FPGA register.
 * @param	dh		DiscFerret device handle.
 * @param	addr	Register address
 * @returns Value of the register, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Reads the current contents of the DiscFerret FPGA register at the address
 * specified by the addr parameter.
 */
int discferret_reg_peek(DISCFERRET_DEVICE_HANDLE *dh, unsigned int addr);

/**
 * @brief	Write a value to a DiscFerret FPGA register.
 * @param	dh		DiscFerret device handle.
 * @param	addr	Register address
 * @param	data	Register value
 * @returns DISCFERRET_E_OK, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Writes the value in the data parameter to the DiscFerret FPGA register at
 * the address specified in the addr parameter.
 */
int discferret_reg_poke(DISCFERRET_DEVICE_HANDLE *dh, unsigned int addr, unsigned char data);

long discferret_ram_addr_get(DISCFERRET_DEVICE_HANDLE *dh);
int discferret_ram_addr_set(DISCFERRET_DEVICE_HANDLE *dh, unsigned long addr);


#ifdef __cplusplus
}
#endif

#endif
