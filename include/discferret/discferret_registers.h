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
 * @file	discferret_registers.h
 * @brief	DiscFerret registers and bit allocations.
 */

#ifndef DISCFERRET_REGISTERS_H
#define DISCFERRET_REGISTERS_H

/// DiscFerret control registers (write only)
enum {
	/// Disc drive control register
	DISCFERRET_R_DRIVE_CONTROL			= 0x04,
	/// Acquisition control register
	DISCFERRET_R_ACQCON					= 0x05,
	/// Acquisition start event
	DISCFERRET_R_ACQ_START_EVT			= 0x06,
	/// Acquisition stop event
	DISCFERRET_R_ACQ_STOP_EVT			= 0x07,
	/// Number of start event triggers required before acquisition is started
	DISCFERRET_R_ACQ_START_NUM			= 0x08,
	/// Number of stop event triggers required before acquisition is stopped
	DISCFERRET_R_ACQ_STOP_NUM			= 0x09,
	/// Acquisition clock divider ratio
	DISCFERRET_R_ACQ_CLKSEL				= 0x0A,

	/// Status register 1
	DISCFERRET_R_STATUS1				= 0x0E,
	/// Status register 2
	DISCFERRET_R_STATUS2				= 0x0F,

	/// Hard sector track mark detector threshold, acquisition start event
	DISCFERRET_R_ACQ_HSTMD_THR_START	= 0x10,
	/// Hard sector track mark detector threshold, acquisition stop event
	DISCFERRET_R_ACQ_HSTMD_THR_STOP		= 0x11,

	/// MFM synchronisation word, acquisition start event, low byte
	DISCFERRET_R_MFM_SYNCWORD_START_L	= 0x20,
	/// MFM synchronisation word, acquisition start event, high byte
	DISCFERRET_R_MFM_SYNCWORD_START_H	= 0x21,
	/// MFM synchronisation word, acquisition stop event, low byte
	DISCFERRET_R_MFM_SYNCWORD_STOP_L	= 0x22,
	/// MFM synchronisation word, acquisition stop event, high byte
	DISCFERRET_R_MFM_SYNCWORD_STOP_H	= 0x23,
	/// MFM synchronisation word comparison mask, acquisition start event, low byte
	DISCFERRET_R_MFM_MASK_START_L		= 0x24,
	/// MFM synchronisation word comparison mask, acquisition start event, high byte
	DISCFERRET_R_MFM_MASK_START_H		= 0x25,
	/// MFM synchronisation word comparison mask, acquisition stop event, low byte
	DISCFERRET_R_MFM_MASK_STOP_L		= 0x26,
	/// MFM synchronisation word comparison mask, acquisition stop event, high byte
	DISCFERRET_R_MFM_MASK_STOP_H		= 0x27,
	/// MFM sync word detector clock select
	DISCFERRET_R_MFM_CLKSEL				= 0x2F,

	/// TEST REGISTER: Scratchpad register (read/write)
	DISCFERRET_R_SCRATCHPAD				= 0x30,
	/// TEST REGISTER: Inverse Scratchpad (read only, returns NOT of Scratchpad)
	DISCFERRET_R_INVERSE_SCRATCHPAD		= 0x31,
	/// TEST REGISTER: Fixed 0x55 (read only, always returns 0x55)
	DISCFERRET_R_FIXED55				= 0x32,
	/// TEST REGISTER: Fixed 0xAA (read only, always returns 0xAA)
	DISCFERRET_R_FIXEDAA				= 0x33,
	/// TEST REGISTER: Clock ticker (read only, value increments at 20MHz)
	DISCFERRET_R_CLOCK_TICKER			= 0x34,
	/// TEST REGISTER: Clock ticker (read only, value increments at PLL clock rate)
	DISCFERRET_R_CLOCK_TICKER_PLL		= 0x35,

	/**
	 * Index frequency measurement in 250us counts, high byte.
	 * @note Reading this register latches the low byte of the counter into DISCFERRET_R_INDEX_FREQ_LOW.
	 *       Reading DISCFERRET_R_INDEX_FREQ_LOW before DISCFERRET_R_INDEX_FREQ_HIGH will produce an erroneous result.
	 */
	DISCFERRET_R_INDEX_FREQ_HIGH		= 0x40,
	/// Index frequency, low byte -- see DISCFERRET_R_INDEX_FREQ_HIGH.
	DISCFERRET_R_INDEX_FREQ_LOW			= 0x41,

	/// HSIO: pin direction register
	DISCFERRET_R_HSIO_DIR				= 0xE0,
	/// HSIO: pin settings register
	DISCFERRET_R_HSIO_PIN				= 0xE1,

	/// Stepping controller: step rate, 250us per count
	DISCFERRET_R_STEP_RATE				= 0xF0,
	/// Stepping controller: step command. Bit 7 = direction, bits 6..0 = number of steps.
	DISCFERRET_R_STEP_CMD				= 0xFF
};

/// Step Command bits
enum {
	/// Step towards track zero (decrement track number)
	DISCFERRET_STEP_CMD_TOWARDS_ZERO	= 0x80,
	/// Step away from track zero (increment track number)
	DISCFERRET_STEP_CMD_AWAYFROM_ZERO	= 0x00,
	/// Bit mask for the step count
	DISCFERRET_STEP_COUNT_MASK			= 0x7F
};

/// DiscFerret DRIVE_CONTROL register bits
enum {
	/// Density output (pin 2)
	DISCFERRET_DRIVE_CONTROL_DENSITY	= 0x01,
	/// In Use output (pin 4)
	DISCFERRET_DRIVE_CONTROL_INUSE		= 0x02,
	/// Drive select 0 (pin 10)
	DISCFERRET_DRIVE_CONTROL_DS0		= 0x04,
	/// Drive select 1 (pin 12)
	DISCFERRET_DRIVE_CONTROL_DS1		= 0x08,
	/// Drive select 2 (pin 14)
	DISCFERRET_DRIVE_CONTROL_DS2		= 0x10,
	/// Drive select 3 (pin 6)
	DISCFERRET_DRIVE_CONTROL_DS3		= 0x20,
	/// Motor enable (pin 10)
	DISCFERRET_DRIVE_CONTROL_MOTEN		= 0x40,
	/// Side select (pin 32)
	DISCFERRET_DRIVE_CONTROL_SIDESEL	= 0x80
};

/// DiscFerret Acquisition Control register bits
enum {
	/// 1=start a write operation
	DISCFERRET_ACQCON_WRITE			= 0x04,
	/// 1=abort current read/write operation
	DISCFERRET_ACQCON_ABORT			= 0x02,
	/// 1=start a read operation
	DISCFERRET_ACQCON_START			= 0x01
};

/// Masks and events for ACQ_{START,STOP}_EVT registers
enum {
	/// Start/stop acquisition immediately
	DISCFERRET_ACQ_EVENT_IMMEDIATE	= 0x00,
	/// Start/stop acquisition at next index pulse
	DISCFERRET_ACQ_EVENT_INDEX		= 0x01,
	/// Start/stop acquisition at next sync word match
	DISCFERRET_ACQ_EVENT_SYNC_WORD	= 0x02,
	/// "wait for HSTMD before checking for acquisition trigger" combination bit
	DISCFERRET_ACQ_EVENT_WAIT_HSTMD	= 0x80
};

/// Legal MFM_CLKSEL values
enum {
	/// 1 megabit per second (IBM 2.88MB MFM)
	DISCFERRET_MFM_CLKSEL_1MBPS		= 0x00,
	/// 500kbps (IBM 1.44MB MFM)
	DISCFERRET_MFM_CLKSEL_500KBPS	= 0x01,
	/// 250kbps (IBM 720K MFM, ??? FM)
	DISCFERRET_MFM_CLKSEL_250KBPS	= 0x02,
	/// 125kbps (??? FM)
	DISCFERRET_MFM_CLKSEL_125KBPS	= 0x03
};

/// Legal ACQ_CLKSEL values
enum {
	/// Full rate (100MHz)
	DISCFERRET_ACQ_RATE_100MHZ		= 0x00,
	/// Half rate (50MHz)
	DISCFERRET_ACQ_RATE_50MHZ		= 0x01,
	/// Quarter rate (25MHz)
	DISCFERRET_ACQ_RATE_25MHZ		= 0x02,
	/// Eighth rate (12.5MHz)
	DISCFERRET_ACQ_RATE_12_5MHZ		= 0x03
};

/// DiscFerret STATUS bits
enum {
	/// Last seek operation terminated because track 0 was reached
	DISCFERRET_STATUS_TRACK0_HIT		= 0x10,
	/// New index frequency measurement available
	DISCFERRET_STATUS_NEW_INDEX_MEAS	= 0x08,
	/// Bit Mask for STATUS_ACQ_xxx bits
	DISCFERRET_STATUS_ACQSTATUS_MASK	= 0x07,
	/// Currently writing to disc
	DISCFERRET_STATUS_ACQ_WRITING		= 0x04,
	/// Waiting for trigger
	DISCFERRET_STATUS_ACQ_WAITING		= 0x02,
	/// Currently reading (acquiring data) from disc
	DISCFERRET_STATUS_ACQ_ACQUIRING		= 0x01,
	/// Idle
	DISCFERRET_STATUS_ACQ_IDLE			= 0x00,
	/// 1=Index pulse active (pin 8)
	DISCFERRET_STATUS_INDEX				= 0x8000,
	/// 1=Track 0 active (pin 26)
	DISCFERRET_STATUS_TRACK0			= 0x4000,
	/// 1=Disc write protected (pin 28)
	DISCFERRET_STATUS_WRITE_PROTECT		= 0x2000,
	/// 1=Disc changed (pin 34)
	DISCFERRET_STATUS_DISC_CHANGE		= 0x1000,
	/// 1=Density pin state (pin 2)
	DISCFERRET_STATUS_DENSITY			= 0x0800,
	/// 1=Stepping controller currently stepping in/out
	DISCFERRET_STATUS_STEPPING			= 0x0400,
	/// RAM empty (pointer = 0 and not full)
	DISCFERRET_STATUS_RAM_EMPTY			= 0x0200,
	/// RAM full (last RAM write caused RAM pointer wraparound)
	DISCFERRET_STATUS_RAM_FULL			= 0x0100
};

#endif // DISCFERRET_REGISTERS_H

// vim: ts=4
