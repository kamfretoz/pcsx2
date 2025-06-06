// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "CDVDdiscReader.h"
#include "CDVD/CDVD.h"
#include "Host.h"
#include "common/Console.h"

#include "common/Error.h"

#include "fmt/format.h"

#include <condition_variable>
#include <mutex>
#include <thread>

void (*newDiscCB)();

static std::mutex s_keepalive_lock;
static std::condition_variable s_keepalive_cv;
static std::thread s_keepalive_thread;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// State Information                                                         //

int curDiskType;
int curTrayStatus;

static u32 csector;
int cmode;

static int lastReadInNewDiskCB = 0;
static u8 directReadSectorBuffer[2448];

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Utility Functions                                                         //

static u8 dec_to_bcd(u8 dec)
{
	return ((dec / 10) << 4) | (dec % 10);
}

static void lsn_to_msf(u8* minute, u8* second, u8* frame, u32 lsn)
{
	*frame = dec_to_bcd(lsn % 75);
	lsn /= 75;
	*second = dec_to_bcd(lsn % 60);
	lsn /= 60;
	*minute = dec_to_bcd(lsn % 100);
}

// TocStuff
void cdvdParseTOC()
{
	tracks.fill(cdvdTrack{});

	if (!src->GetSectorCount())
	{
		curDiskType = CDVD_TYPE_NODISC;
		strack = 1;
		etrack = 0;
		return;
	}

	if (src->GetMediaType() >= 0)
	{
		tracks[1].type = CDVD_MODE1_TRACK;

		strack = 1;
		etrack = 1;
		return;
	}

	strack = 0xFF;
	etrack = 0;

	for (auto& entry : src->ReadTOC())
	{
		const u8 track = entry.track;
		if (track < 1 || track >= tracks.size())
		{
			Console.Warning("CDVD: Invalid track index %u, ignoring\n", track);
			continue;
		}
		strack = std::min(strack, track);
		etrack = std::max(etrack, track);
		tracks[track].start_lba = entry.lba;
		if ((entry.control & 0x0C) == 0x04)
		{
			std::array<u8, 2352> buffer;
			// Byte 15 of a raw CD data sector determines the track mode
			if (src->ReadSectors2352(entry.lba, 1, buffer.data()) && (buffer[15] & 3) == 2)
			{
				tracks[track].type = CDVD_MODE2_TRACK;
			}
			else
			{
				tracks[track].type = CDVD_MODE1_TRACK;
			}
		}
		else
		{
			tracks[track].type = CDVD_AUDIO_TRACK;
		}
#ifdef PCSX2_DEBUG
		DevCon.WriteLn("cdvdParseTOC: Track %u: LBA %u, Type %u\n", track, tracks[track].start_lba, tracks[track].type);
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CDVD processing functions                                                 //

std::atomic<bool> s_keepalive_is_open;
bool disc_has_changed = false;
bool weAreInNewDiskCB = false;

std::unique_ptr<IOCtlSrc> src;

extern u32 g_last_sector_block_lsn;

///////////////////////////////////////////////////////////////////////////////
// keepAliveThread throws a read event regularly to prevent drive spin down  //

static void keepAliveThread()
{
	u8 throwaway[2352];

	printf(" * CDVD: KeepAlive thread started...\n");
	std::unique_lock<std::mutex> guard(s_keepalive_lock);

	while (!s_keepalive_cv.wait_for(guard, std::chrono::seconds(30),
		[]() { return !s_keepalive_is_open; }))
	{

		//printf(" * keepAliveThread: polling drive.\n");
		if (src->GetMediaType() >= 0)
			src->ReadSectors2048(g_last_sector_block_lsn, 1, throwaway);
		else
			src->ReadSectors2352(g_last_sector_block_lsn, 1, throwaway);
	}

	printf(" * CDVD: KeepAlive thread finished.\n");
}

bool StartKeepAliveThread()
{
	if (s_keepalive_is_open == false)
	{
		s_keepalive_is_open = true;
		s_keepalive_thread = std::thread(keepAliveThread);
	}

	return s_keepalive_is_open;
}

void StopKeepAliveThread()
{
	if (!s_keepalive_thread.joinable())
		return;

	{
		std::lock_guard<std::mutex> guard(s_keepalive_lock);
		s_keepalive_is_open = false;
	}
	s_keepalive_cv.notify_one();
	s_keepalive_thread.join();
}

static bool DISCopen(std::string filename, Error* error)
{
	std::string drive = filename;
	GetValidDrive(drive);
	if (drive.empty())
	{
		Error::SetString(error, fmt::format("Failed to get drive for {}", filename));
		return false;
	}

	// open device file
	src = std::make_unique<IOCtlSrc>(std::move(drive));
	if (!src->Reopen(error))
	{
		src.reset();
		return false;
	}

	//setup threading manager
	cdvdStartThread();
	StartKeepAliveThread();

	cdvdRefreshData();
	return true;
}

static bool DISCprecache(ProgressCallback* progress, Error* error)
{
	Error::SetStringView(error, TRANSLATE_SV("CDVD", "Precaching is not supported for discs."));
	return false;
}

static void DISCclose()
{
	StopKeepAliveThread();
	cdvdStopThread();
	//close device
	src.reset();
}

static s32 DISCreadTrack(u32 lsn, int mode)
{
	csector = lsn;
	cmode = mode;

	if (weAreInNewDiskCB)
	{
		int ret = cdvdDirectReadSector(lsn, mode, directReadSectorBuffer);
		if (ret == 0)
			lastReadInNewDiskCB = 1;
		return ret;
	}

	cdvdRequestSector(lsn, mode);

	return 0;
}

static s32 DISCgetBuffer(u8* dest)
{
	// Do nothing for out of bounds disc sector reads. It prevents some games
	// from hanging (All-Star Baseball 2005, Hello Kitty: Roller Rescue,
	// Hot Wheels: Beat That! (NTSC), Ratchet & Clank 3 (PAL),
	// Test Drive: Eve of Destruction, etc.).
	if (csector >= src->GetSectorCount())
		return 0;

	int csize = 2352;
	switch (cmode)
	{
		case CDVD_MODE_2048:
			csize = 2048;
			break;
		case CDVD_MODE_2328:
			csize = 2328;
			break;
		case CDVD_MODE_2340:
			csize = 2340;
			break;
	}

	if (lastReadInNewDiskCB)
	{
		lastReadInNewDiskCB = 0;

		memcpy(dest, directReadSectorBuffer, csize);
		return 0;
	}

	memcpy(dest, cdvdGetSector(csector, cmode), csize);

	return 0;
}

static s32 DISCreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	// the formatted subq command returns:  control/adr, track, index, trk min, trk sec, trk frm, 0x00, abs min, abs sec, abs frm

	if (lsn >= src->GetSectorCount())
		return -1;

	memset(subq, 0, sizeof(cdvdSubQ));

	lsn_to_msf(&subq->discM, &subq->discS, &subq->discF, lsn + 150);

	u8 i = strack;
	while (i < etrack && lsn >= tracks[i + 1].start_lba)
		++i;

	lsn -= tracks[i].start_lba;

	lsn_to_msf(&subq->trackM, &subq->trackS, &subq->trackF, lsn);

	subq->ctrl = tracks[i].type;

	// It's important to note that we do _not_ use the current MSF values
	// from the host's device. We use the MSF values from the lsn.
	// An easy way to test an implementation is to see if the OSDSYS
	// CD player can display the correct minute and second values.
	// From my testing, the IOCTL returns 0 for ctrl. This also breaks
	// the OSDSYS player. The only "safe" values to receive from the IOCTL
	// are ADR, trackNum and trackIndex.
	if (!src->ReadTrackSubQ(subq))
	{
		subq->adr = 1;
		subq->trackNum = i;
		subq->trackIndex = 1;
	}

	return 0;
}

static s32 DISCgetTN(cdvdTN* Buffer)
{
	Buffer->strack = strack;
	Buffer->etrack = etrack;
	return 0;
}

static s32 DISCgetTD(u8 Track, cdvdTD* Buffer)
{
	if (Track == 0)
	{
		if (src == nullptr)
			return -1;

		Buffer->lsn = src->GetSectorCount();
		Buffer->type = 0;
		return 0;
	}

	if (Track < strack)
		return -1;
	if (Track > etrack)
		return -1;

	Buffer->lsn = tracks[Track].start_lba;
	Buffer->type = tracks[Track].type;
	return 0;
}

static s32 DISCgetTOC(void* toc)
{
	u8* tocBuff = static_cast<u8*>(toc);
	if (curDiskType == CDVD_TYPE_NODISC)
		return -1;

	if (curDiskType == CDVD_TYPE_DETCTDVDS || curDiskType == CDVD_TYPE_DETCTDVDD)
	{
		memset(tocBuff, 0, 2048);

		s32 mt = src->GetMediaType();

		if (mt < 0)
			return -1;

		if (mt == 0)
		{ //single layer
			// Single Layer - Values are fixed.
			tocBuff[0] = 0x04;
			tocBuff[1] = 0x02;
			tocBuff[2] = 0xF2;
			tocBuff[3] = 0x00;
			tocBuff[4] = 0x86;
			tocBuff[5] = 0x72;

			// These values are fixed on all discs, except position 14 which is the OTP/PTP flags which are 0 in single layer.
			tocBuff[12] = 0x01;
			tocBuff[13] = 0x02;
			tocBuff[14] = 0x01; // Single layer.
			tocBuff[15] = 0x00;

			// Values are fixed.
			tocBuff[16] = 0x00; // first sector for layer 0
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			cdvdTD trackInfo;

			if (DISCgetTD(0, &trackInfo) == -1)
				trackInfo.lsn = 0;
			// Max LSN in the TOC is calculated as the blocks + 0x30000, then - 1.
			// same as layer 1 start.
			const s32 maxlsn = trackInfo.lsn + (0x30000 - 1);
			tocBuff[20] = maxlsn >> 24;
			tocBuff[21] = (maxlsn >> 16) & 0xff;
			tocBuff[22] = (maxlsn >> 8) & 0xff;
			tocBuff[23] = (maxlsn >> 0) & 0xff;
		}
		else if (mt == 1)
		{ //PTP
			const s32 layer1start = src->GetLayerBreakAddress() + 0x30000;

			// dual sided
			tocBuff[0] = 0x24;
			tocBuff[1] = 0x02;
			tocBuff[2] = 0xF2;
			tocBuff[3] = 0x00;
			tocBuff[4] = 0x41;
			tocBuff[5] = 0x95;

			// These values are fixed on all discs, except position 14 which is the OTP/PTP flags.
			tocBuff[12] = 0x01;
			tocBuff[13] = 0x02;
			tocBuff[14] = 0x21; // PTP
			tocBuff[15] = 0x10;

			// Values are fixed.
			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			tocBuff[20] = (layer1start >> 24);
			tocBuff[21] = (layer1start >> 16) & 0xff;
			tocBuff[22] = (layer1start >> 8) & 0xff;
			tocBuff[23] = (layer1start >> 0) & 0xff;
		}
		else
		{ //OTP
			const s32 layer1start = src->GetLayerBreakAddress() + 0x30000;

			// dual sided
			tocBuff[0] = 0x24;
			tocBuff[1] = 0x02;
			tocBuff[2] = 0xF2;
			tocBuff[3] = 0x00;
			tocBuff[4] = 0x41;
			tocBuff[5] = 0x95;

			// These values are fixed on all discs, except position 14 which is the OTP/PTP flags.
			tocBuff[12] = 0x01;
			tocBuff[13] = 0x02;
			tocBuff[14] = 0x31; // OTP
			tocBuff[15] = 0x10;

			// Values are fixed.
			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			tocBuff[24] = (layer1start >> 24);
			tocBuff[25] = (layer1start >> 16) & 0xff;
			tocBuff[26] = (layer1start >> 8) & 0xff;
			tocBuff[27] = (layer1start >> 0) & 0xff;
		}
	}
	else if (curDiskType == CDVD_TYPE_DETCTCD)
	{
		// cd toc
		// (could be replaced by 1 command that reads the full toc)
		u8 min, sec, frm, i;
		s32 err;
		cdvdTN diskInfo;
		cdvdTD trackInfo;
		memset(tocBuff, 0, 1024);
		if (DISCgetTN(&diskInfo) == -1)
		{
			diskInfo.etrack = 0;
			diskInfo.strack = 1;
		}
		if (DISCgetTD(0, &trackInfo) == -1)
			trackInfo.lsn = 0;

		tocBuff[0] = 0x41;
		tocBuff[1] = 0x00;

		//Number of FirstTrack
		tocBuff[2] = 0xA0;
		tocBuff[7] = dec_to_bcd(diskInfo.strack);

		//Number of LastTrack
		tocBuff[12] = 0xA1;
		tocBuff[17] = dec_to_bcd(diskInfo.etrack);

		//DiskLength
		lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
		tocBuff[22] = 0xA2;
		tocBuff[27] = dec_to_bcd(min);
		tocBuff[28] = dec_to_bcd(sec);
		tocBuff[29] = dec_to_bcd(frm);

		fprintf(stderr, "Track 0: %u mins %u secs %u frames\n", min, sec, frm);

		for (i = diskInfo.strack; i <= diskInfo.etrack; i++)
		{
			err = DISCgetTD(i, &trackInfo);
			lba_to_msf(trackInfo.lsn, &min, &sec, &frm);

			const u8 tocIndex = i - diskInfo.strack;
			tocBuff[tocIndex * 10 + 30] = trackInfo.type;
			tocBuff[tocIndex * 10 + 32] = err == -1 ? 0 : dec_to_bcd(i); //number
			tocBuff[tocIndex * 10 + 37] = dec_to_bcd(min);
			tocBuff[tocIndex * 10 + 38] = dec_to_bcd(sec);
			tocBuff[tocIndex * 10 + 39] = dec_to_bcd(frm);
			fprintf(stderr, "Track %u: %u mins %u secs %u frames\n", i, min, sec, frm);
		}
	}
	else
		return -1;

	return 0;
}

static s32 DISCgetDiskType()
{
	return curDiskType;
}

static s32 DISCgetTrayStatus()
{
	return curTrayStatus;
}

static s32 DISCctrlTrayOpen()
{
	curTrayStatus = CDVD_TRAY_OPEN;
	return 0;
}

static s32 DISCctrlTrayClose()
{
	curTrayStatus = CDVD_TRAY_CLOSE;
	return 0;
}

static void DISCnewDiskCB(void (*callback)())
{
	newDiscCB = callback;
}

static s32 DISCreadSector(u8* buffer, u32 lsn, int mode)
{
	return cdvdDirectReadSector(lsn, mode, buffer);
}

static s32 DISCgetDualInfo(s32* dualType, u32* _layer1start)
{
	if (src == nullptr)
		return -1;
	switch (src->GetMediaType())
	{
		case 1:
			*dualType = 1;
			*_layer1start = src->GetLayerBreakAddress() + 1;
			return 0;
		case 2:
			*dualType = 2;
			*_layer1start = src->GetLayerBreakAddress() + 1;
			return 0;
		case 0:
			*dualType = 0;
			*_layer1start = 0;
			return 0;
	}
	return -1;
}

const CDVD_API CDVDapi_Disc =
	{
		DISCclose,
		DISCopen,
		DISCprecache,
		DISCreadTrack,
		DISCgetBuffer,
		DISCreadSubQ,
		DISCgetTN,
		DISCgetTD,
		DISCgetTOC,
		DISCgetDiskType,
		DISCgetTrayStatus,
		DISCctrlTrayOpen,
		DISCctrlTrayClose,

		DISCnewDiskCB,

		DISCreadSector,
		DISCgetDualInfo,
};
