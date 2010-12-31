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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	A structure containing information about a specific DiscFerret device.
 */
typedef struct {
	char *devicename;					///< Device name string.
	char *manufacturer;					///< Device manufacturer.
	char *serialnumber;					///< Device serial number.
	libusb_device_handle *dh;			///< Device handle.
} DISCFERRET_DEVICE;

/**
 * @brief	DiscFerret library error codes.
 */
enum {
	DISCFERRET_E_OK				=	0,	///< Operation succeeded.
	DISCFERRET_E_ALREADY_INIT,			///< Library already initialised
	DISCFERRET_E_NOT_INIT,				///< Library not initialised yet
	DISCFERRET_E_USB_ERROR,				///< USB error
};

/**
 * @brief	Initialise the DiscFerret library.
 * @note	Must be called before calling any other discferret_* functions.
 */
int discferret_init(void);

/**
 * @brief Enumerate all available DiscFerret devices.
 */

#ifdef __cplusplus
}
#endif

#endif
