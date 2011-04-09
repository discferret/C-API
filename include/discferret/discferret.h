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
 * Microcode data -- in discferret_microcode.c
 */

/// DiscFerret microcode data
extern const unsigned char discferret_microcode[];
/// Length of DiscFerret microcode data
extern const unsigned long discferret_microcode_length;


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
	bool	has_index_freq_sense;		///< True if device supports index frequncy measurement
	bool	has_index_freq_avail_flag;	///< True if device has the "new index measurement available" flag bit
	bool	has_track0_flag;			///< True if device has the "track 0 reached during seek" flag bit
	double	index_freq_multiplier;		///< Index frequency multiplier
	long	current_track;				///< Current track number
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
	DISCFERRET_E_NOT_SUPPORTED,				///< Feature not supported by this firmware/microcode version
	DISCFERRET_E_RECAL_FAILED,				///< Recalibrate failed (track0 not reached after specified number of steps)
	DISCFERRET_E_TRACK0_REACHED,			///< Track 0 reached during seek (informative)
	DISCFERRET_E_CURRENT_TRACK_UNKNOWN		///< Current track not known before or after seek (need to Recalibrate the head)
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
 * @brief	Free a device list.
 * @param	devlist		Pointer to a DISCFERRET_DEVICE* block.
 *
 * Frees the device list created by discferret_find_devices.
 */
void discferret_devlist_free(DISCFERRET_DEVICE **devlist);

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
DISCFERRET_ERROR discferret_open(const char *serialnum, DISCFERRET_DEVICE_HANDLE **dh);

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
DISCFERRET_ERROR discferret_open_first(DISCFERRET_DEVICE_HANDLE **dh);

/**
 * @brief	Close a DiscFerret device.
 * @param	dh		DiscFerret device handle.
 * @returns DISCFERRET_E_OK on success, one of the DISCFERRET_E_xxx constants on error.
 *
 * Closes a DiscFerret device handle which was obtained by calling
 * discferret_open() or discferret_open_first(). This function MUST be called
 * after the application is finished with the DiscFerret.
 */
DISCFERRET_ERROR discferret_close(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Update a DiscFerret device handle's local copy of the device capability block.
 * @param	dh		DiscFerret device handle.
 *
 * Libdiscferret keeps a local copy of the capability data for a DiscFerret
 * unit. This capability data is updated automatically when the device is
 * opened, or when new microcode is uploaded.
 *
 * This function allows the capability data to be updated manually. Normally
 * this is not necessary, but may prove necessary if microcode is loaded using
 * the block-write functions (which is NOT recommended).
 */
DISCFERRET_ERROR discferret_update_capabilities(DISCFERRET_DEVICE_HANDLE *dh);

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
DISCFERRET_ERROR discferret_get_info(DISCFERRET_DEVICE_HANDLE *dh, DISCFERRET_DEVICE_INFO *info);

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
DISCFERRET_ERROR discferret_fpga_load_begin(DISCFERRET_DEVICE_HANDLE *dh);

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
DISCFERRET_ERROR discferret_fpga_load_block(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *block, const size_t len, const bool swap);

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
DISCFERRET_ERROR discferret_fpga_get_status(DISCFERRET_DEVICE_HANDLE *dh);

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
DISCFERRET_ERROR discferret_fpga_load_rbf(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *rbfdata, const size_t len);

/**
 * @brief	Read the contents of a DiscFerret FPGA register.
 * @param	dh		DiscFerret device handle.
 * @param	addr	Register address
 * @returns Value of the register, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Reads the current contents of the DiscFerret FPGA register at the address
 * specified by the addr parameter.
 */
int discferret_reg_peek(DISCFERRET_DEVICE_HANDLE *dh, const unsigned int addr);

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
DISCFERRET_ERROR discferret_reg_poke(DISCFERRET_DEVICE_HANDLE *dh, const unsigned int addr, const unsigned char data);

/**
 * @brief	Get current value of the Acquisition RAM address pointer.
 * @param	dh		DiscFerret device handle.
 * @returns	Current address, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Retrieves the current value of the DiscFerret's memory address pointer.
 */
long discferret_ram_addr_get(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Set Acquisition RAM address pointer.
 * @param	dh		DiscFerret device handle.
 * @param	addr	Address pointer value.
 * @returns DISCFERRET_E_OK, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Sets the DiscFerret's memory address pointer to the value passed in the addr parameter.
 */
DISCFERRET_ERROR discferret_ram_addr_set(DISCFERRET_DEVICE_HANDLE *dh, const unsigned long addr);

/**
 * @brief	Write a block of data to Acquisition RAM.
 * @param	dh		DiscFerret device handle.
 * @param	block	Data block to write.
 * @param	len		Size of data block.
 * @returns DISCFERRET_E_OK, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Writes a block of data to the DiscFerret's acquisition RAM, at the address
 * in the address pointer. The value of the address pointer can be read using
 * discferret_ram_addr_get(), or set using discferret_ram_addr_set().
 */
DISCFERRET_ERROR discferret_ram_write(DISCFERRET_DEVICE_HANDLE *dh, const unsigned char *block, const size_t len);

/**
 * @brief	Read a block of data from Acquisition RAM.
 * @param	dh		DiscFerret device handle.
 * @param	block	Pointer to data buffer.
 * @param	len		Number of bytes to read.
 * @returns DISCFERRET_E_OK on success, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Reads a block of data from the DiscFerret's acquisition RAM, at the address
 * set in the address pointer. The value of the address pointer can be read
 * using discferret_ram_addr_get(), or set using discferret_ram_addr_set().
 */
DISCFERRET_ERROR discferret_ram_read(DISCFERRET_DEVICE_HANDLE *dh, unsigned char *block, const size_t len);

/**
 * @brief	Get the current status of the DiscFerret.
 * @param	dh		DiscFerret device handle.
 * @returns	Status of the DiscFerret, or negative (one of the DISCFERRET_E_xxx constants) in case of error.
 *
 * Reads and returns the current value of the DiscFerret's status register.
 */
long discferret_get_status(DISCFERRET_DEVICE_HANDLE *dh);

/**
 * @brief	Measure the time taken for the last complete revolution of the disc
 * @param	dh		DiscFerret device handle.
 * @param	wait	If true, then this function will wait for a new measurement before returning it.
 * 					If false, the measurement currently in the holding register will be returned.
 * @param	timeval	Pointer to a double, which will receive the time value, in seconds.
 * @returns	DISCFERRET_E_OK on success, or one of the DISCFERRET_E_xxx constants in case of error.
 *
 * This function measures the amount of time which has elapsed between the
 * most recent index pulse, and the one immediately preceding it. This
 * value is proportional to the rotational speed of the disc.
 */
DISCFERRET_ERROR discferret_get_index_time(DISCFERRET_DEVICE_HANDLE *dh, const bool wait, double *timeval);

/**
 * @brief	Measure the rotational speed of the disc.
 * @param	dh		DiscFerret device handle.
 * @param	wait	If true, then this function will wait for a new measurement before returning it.
 * 					If false, the measurement currently in the holding register will be returned.
 * @param	freqval	Pointer to a double, which will receive the rotational speed value, in RPM.
 * @returns	DISCFERRET_E_OK on success, or one of the DISCFERRET_E_xxx constants in case of error.
 *
 * This function measures the amount of time which has elapsed between the
 * most recent index pulse, and the one immediately preceding it. This
 * value is then fed into a conversion function which calculates the
 * current rotational speed of the disc in the drive, in revolutions per
 * minute (RPM).
 *
 * A typical disc drive rotates at either 300 or 360 RPM. The measurement
 * range of this function is 90 RPM to 6,000,000 RPM on Microcode 0020, or
 * 4 RPM to 240,000 RPM on Microcode 001F. While Microcode 001F has a
 * narrower range, it also has a much lower accuracy and timing resolution.
 */
DISCFERRET_ERROR discferret_get_index_frequency(DISCFERRET_DEVICE_HANDLE *dh, const bool wait, double *freqval);

/**
 * @brief	Set the seek rate.
 * @param	dh			DiscFerret device handle.
 * @param	steprate_us	Desired seek rate in milliseconds per step.
 * @returns	DISCFERRET_E_OK on success, or one of the DISCFERRET_E_xxx constants in case of error.
 *
 * This function sets the DiscFerret's step rate timer to produce seek pulses
 * with a period of <i>steprate_ms</i> microseconds. This function must be
 * called before any of the other <i>discferret_seek_*</i> functions.
 *
 * DISCFERRET_E_BAD_PARAMETER will be returned if the seek rate exceeds
 * 63750 microseconds per step. The step rate has a resolution of 250
 * microseconds.
 */
DISCFERRET_ERROR discferret_seek_set_rate(DISCFERRET_DEVICE_HANDLE *dh, const unsigned long steprate_us);

/**
 * @brief	Reposition the drive heads at track 0.
 * @param	dh			DiscFerret device handle.
 * @param	maxsteps	Maximum number of steps to move the head.
 * @returns	DISCFERRET_E_OK on success, or one of the DISCFERRET_E_xxx constants in case of error.
 *
 * This function moves the disc head towards track zero, until either track
 * zero is reached, or the head has moved <i>maxsteps</i> steps.
 *
 * DISCFERRET_E_BAD_PARAMETER will be returned if <i>maxsteps</i> is less
 * than 1. DISCFERRET_E_RECAL_FAILED will be returned if track zero was not reached
 * within <i>maxsteps</i> steps.
 */
DISCFERRET_ERROR discferret_seek_recalibrate(DISCFERRET_DEVICE_HANDLE *dh, const unsigned long maxsteps);

/**
 * @brief	Seek the drive heads relative to their current position.
 * @param	dh			DiscFerret device handle.
 * @param	numsteps	Number of steps; positive to seek towards higher-numbered
 * 						tracks, negative to seek towards track zero.
 *
 * Seeks a specified number of steps in a given direction. If <i>numsteps</i>
 * is less than zero, then the drive will seek towards track zero. If
 * <i>numsteps</i> is greater than zero, then the drive will seek towards the
 * highest-numbered track available on the drive.
 *
 * DISCFERRET_E_BAD_PARAMETER will be returned if <i>numsteps</i> is zero.
 * DISCFERRET_E_TRACK0_REACHED will be returned if head reached track zero
 * during the seek operation. DISCFERRET_E_CURRENT_TRACK_UNKNOWN will be
 * returned if the current track number was unknown before the seek (and is
 * still unknown to libdiscferret).
 */
DISCFERRET_ERROR discferret_seek_relative(DISCFERRET_DEVICE_HANDLE *dh, const long numsteps);

/**
 * @brief	Seek the drive heads to an absolute track.
 * @param	dh			DiscFerret device handle.
 * @param	track		Target track number.
 *
 * Seeks the drive heads to the track number specified in the <i>track</i>
 * parameter.
 *
 * The drive head must be at a known location before calling this
 * function. DISCFERRET_E_CURRENT_TRACK_UNKNOWN will be returned if this is
 * not the case, and the seek operation will be aborted.
 */
DISCFERRET_ERROR discferret_seek_absolute(DISCFERRET_DEVICE_HANDLE *dh, const unsigned long track);

#ifdef __cplusplus
}
#endif

#endif
