
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef RESOURCE_H__
#define RESOURCE_H__

#include "intern.h"
#include "pak.h"

struct MemEntry {
	uint8_t status;        // 0x0
	uint8_t type;          // 0x1, Resource::ResType
	uint8_t *bufPtr;       // 0x2
	uint8_t rankNum;       // 0x6
	uint8_t bankNum;       // 0x7
	uint32_t bankPos;      // 0x8
	uint32_t packedSize;   // 0xC
	uint32_t unpackedSize; // 0x12
};

struct AmigaMemEntry {
	uint8_t type;
	uint8_t bank;
	uint32_t offset;
	uint32_t packedSize;
	uint32_t unpackedSize;
};

struct Serializer;
struct Video;

struct Resource {
	enum ResType {
		RT_SOUND  = 0,
		RT_MUSIC  = 1,
		RT_VIDBUF = 2, // full screen video buffer, size=0x7D00
		RT_PAL    = 3, // palette (1024=vga + 1024=ega), size=2048
		RT_SCRIPT = 4,
		RT_VBMP   = 5,
		RT_UNK    = 6,
	};

	enum DataType {
		DT_DOS,
		DT_AMIGA,
		DT_15TH_EDITION,
	};

	enum {
		MEM_BLOCK_SIZE = 600 * 1024,
		ENTRIES_COUNT = 146,
	};

	enum {
		STATUS_NULL,
		STATUS_LOADED,
		STATUS_TOLOAD,
	};

	static const uint16_t _memListAudio[];
	static const uint16_t _memListParts[][4];
	static const AmigaMemEntry _memListAmigaFR[ENTRIES_COUNT];
	static const AmigaMemEntry _memListAmigaEN[ENTRIES_COUNT];

	Video *_vid;
	const char *_dataDir;
	MemEntry _memList[ENTRIES_COUNT + 1];
	uint16_t _numMemList;
	uint16_t _curPtrsId, _newPtrsId;
	uint8_t *_memPtrStart, *_scriptBakPtr, *_scriptCurPtr, *_vidBakPtr, *_vidCurPtr;
	bool _useSegVideo2;
	uint8_t *_segVideoPal;
	uint8_t *_segCode;
	uint8_t *_segVideo1;
	uint8_t *_segVideo2;
	const char *_bankPrefix;
	Pak _pak;
	DataType _dataType;

	Resource(Video *vid, const char *dataDir);

	DataType getDataType() const { return _dataType; }
	
	void readBank(const MemEntry *me, uint8_t *dstBuf);
	void readEntries();
	void readEntriesAmiga(const AmigaMemEntry *entries, int count);
	void load();
	void invalidateAll();
	void invalidateRes();	
	void update(uint16_t num);
	void loadBmp(int num);
	uint8_t *loadDat(int num);
	uint8_t *loadWav(int num);
	void setupPtrs(uint16_t ptrId);
	void allocMemBlock();
	void freeMemBlock();
};

#endif