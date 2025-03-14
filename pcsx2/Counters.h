// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "MemoryTypes.h"

struct EECNT_MODE
{
	// 0 - BUSCLK
	// 1 - 1/16th of BUSCLK
	// 2 - 1/256th of BUSCLK
	// 3 - External Clock (hblank!)
	u32 ClockSource:2;

	// Enables the counter gate (turns counter on/off as according to the
	// h/v blank type set in GateType).
	u32 EnableGate:1;

	// 0 - hblank!  1 - vblank!
	// Note: the hblank source type is disabled when ClockSel = 3
	u32 GateSource:1;

	// 0 - count when the gate signal is low
	// 1 - reset at the signal's rising edge (h/v blank start)
	// 2 - reset at the signal's falling edge (h/v blank end)
	// 3 - reset at both signal edges
	u32 GateMode:2;

	// Counter cleared to zero when target reached.
	// The PS2 only resets if the TargetInterrupt is enabled - Tested on PS2
	u32 ZeroReturn:1;

	// General count enable/status.  If 0, no counting happens.
	// This flag is set/unset by the gates.
	u32 IsCounting:1;

	// Enables target interrupts.
	u32 TargetInterrupt:1;

	// Enables overflow interrupts.
	u32 OverflowInterrupt:1;

	// Set to true by the counter when the target is reached.
	// Flag is set only when TargetInterrupt is enabled.
	u32 TargetReached:1;

	// Set to true by the counter when the target has overflowed.
	// Flag is set only when OverflowInterrupt is enabled.
	u32 OverflowReached:1;
};

struct Counter
{
	u32 count;
	union
	{
		u32 modeval;		// the mode as a 32 bit int (for optimized combination masks)
		EECNT_MODE mode;
	};
	u32 target, hold;
	u32 rate, interrupt;
	u32 startCycle;		// delta values should be signed.
};

struct SyncCounter
{
	u32 Mode;
	u32 startCycle;					// start cycle of timer
	s32 deltaCycles;
};

//------------------------------------------------------------------
// SPEED HACKS!!! (1 is normal) (They have inverse affects, only set 1 at a time)
//------------------------------------------------------------------
#define HBLANK_COUNTER_SPEED	1 //Set to '3' to double the speed of games like KHII
//#define HBLANK_TIMER_SLOWDOWN	1 //Set to '2' to increase the speed of games like God of War (FPS will be less, but game will be faster)

#define SCANLINES_TOTAL_1080	1125 // total number of scanlines for 1080I mode
//------------------------------------------------------------------
// NTSC Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define FRAMERATE_NTSC			29.97 // frames per second

#define SCANLINES_TOTAL_NTSC_I	525 // total number of scanlines (Interlaced)
#define SCANLINES_TOTAL_NTSC_NI	526 // total number of scanlines (Interlaced)
#define SCANLINES_VSYNC_NTSC	3   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_NTSC	240 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_NTSC	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_NTSC	20  // scanlines used for vblank2 (odd interlace)

//------------------------------------------------------------------
// PAL Timing Information!!! (some scanline info is guessed)
//------------------------------------------------------------------
#define FRAMERATE_PAL			25.0// frames per second * 100 (25)

#define SCANLINES_TOTAL_PAL_I	625 // total number of scanlines per frame (Interlaced)
#define SCANLINES_TOTAL_PAL_NI	628 // total number of scanlines per frame (Not Interlaced)
#define SCANLINES_VSYNC_PAL		5   // scanlines that are used for syncing every half-frame
#define SCANLINES_VRENDER_PAL	288 // scanlines in a half-frame (because of interlacing)
#define SCANLINES_VBLANK1_PAL	19  // scanlines used for vblank1 (even interlace)
#define SCANLINES_VBLANK2_PAL	20  // scanlines used for vblank2 (odd interlace)

//------------------------------------------------------------------
// vSync and hBlank Timing Modes
//------------------------------------------------------------------
#define MODE_VRENDER	0x0		//Set during the Render/Frame Scanlines
#define MODE_VBLANK		0x1		//Set during the Blanking Scanlines
#define MODE_GSBLANK	0x2		//Set during the Syncing Scanlines (Delayed GS CSR Swap)

#define MODE_HRENDER	0x0		//Set for ~5/6 of 1 Scanline
#define MODE_HBLANK		0x1		//Set for the remaining ~1/6 of 1 Scanline

extern const char* ReportVideoMode();
extern const char* ReportInterlaceMode();
extern Counter counters[4];
extern SyncCounter hsyncCounter;
extern SyncCounter vsyncCounter;

extern s32 nextDeltaCounter;		// delta until the next counter event (must be signed)
extern u32 nextStartCounter;
extern uint g_FrameCount;

extern void rcntUpdate_hScanline();
extern void rcntUpdate_vSync();
extern bool rcntCanCount(int i);
extern void rcntSyncCounter(int i);
extern void rcntUpdate();

extern void rcntInit();
extern u32	rcntRcount(int index);
template< uint page > extern bool rcntWrite32( u32 mem, mem32_t& value );
template< uint page > extern u16 rcntRead32( u32 mem );		// returns u16 by design! (see implementation for details)

extern void UpdateVSyncRate(bool force);

