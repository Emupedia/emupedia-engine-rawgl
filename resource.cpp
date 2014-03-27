
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "resource.h"
#include "file.h"
#include "unpack.h"
#include "video.h"


Resource::Resource(Video *vid, const char *dataDir) 
	: _vid(vid), _dataDir(dataDir), _curPtrsId(0), _newPtrsId(0), _pak(dataDir), _dataType(DT_DOS) {
	_bankPrefix = "bank";
}

void Resource::readBank(const MemEntry *me, uint8_t *dstBuf) {
	uint16_t n = me - _memList;
	debug(DBG_BANK, "Resource::readBank(%d)", n);
#ifdef USE_UNPACKED_DATA
	char bankEntryName[64];
	sprintf(bankEntryName, "ootw-%02X-%d.dump", n, me->type);
	File f;
	if (!f.open(bankEntryName, _dataDir)) {
		error("Resource::readBank() unable to open '%s' file\n", bankEntryName);
	}
	f.read(dstBuf, me->unpackedSize);
#else
	char name[10];
	snprintf(name, sizeof(name), "%s%02x", _bankPrefix, me->bankNum);
	File f;
	if (f.open(name, _dataDir)) {
		f.seek(me->bankPos);
		if (me->packedSize == me->unpackedSize) {
			f.read(dstBuf, me->packedSize);
		} else {
			f.read(dstBuf, me->packedSize);
			bool ret = delphine_unpack(dstBuf, dstBuf, me->packedSize);
			if (!ret) {
				error("Resource::readBank() failed to unpack resource %d", n);
			}
		}
	} else {
		error("Resource::readBank() unable to open '%s'", name);
	}
#endif
}

void Resource::readEntries() {
	_pak.readEntries();
	if (_pak._entriesCount != 0) {
		_dataType = DT_15TH_EDITION;
		debug(DBG_INFO, "Using 15th anniversary edition data");
		return;
	}
	// DOS datafiles
	File f;
	if (f.open("demo01", _dataDir)) {
		_bankPrefix = "demo";
	}
	if (!f.open("memlist.bin", _dataDir)) {
		warning("Resource::readEntries() unable to open 'memlist.bin' file");
		static const uint32_t bank01Sizes[] = { 244674, 244868, 0 };
		static const AmigaMemEntry *entries[] = { _memListAmigaFR, _memListAmigaEN, 0 };
		if (f.open("bank01", _dataDir)) {
			for (int i = 0; bank01Sizes[i] != 0; ++i) {
				if (f.size() == bank01Sizes[i]) {
					debug(DBG_INFO, "Using Amiga data files");
					_dataType = DT_AMIGA;
					readEntriesAmiga(entries[i], 146);
					return;
				}
			}
		}
		error("No data files found in '%s'", _dataDir);
		return;
	}
	debug(DBG_INFO, "Using DOS data files");
	_numMemList = 0;
	MemEntry *me = _memList;
	while (1) {
		assert(_numMemList < ARRAYSIZE(_memList));
		me->status = f.readByte();
		me->type = f.readByte();
		me->bufPtr = 0; f.readUint32BE();
		me->rankNum = f.readByte();
		me->bankNum = f.readByte();
		me->bankPos = f.readUint32BE();
		me->packedSize = f.readUint32BE();
		me->unpackedSize = f.readUint32BE();
		if (me->status == 0xFF) {
			break;
		}
		++_numMemList;
		++me;
	}
}

void Resource::readEntriesAmiga(const AmigaMemEntry *entries, int count) {
	_numMemList = count;
	memset(_memList, 0, sizeof(_memList));
	for (int i = 0; i < count; ++i) {
		_memList[i].type = entries[i].type;
		_memList[i].bankNum = entries[i].bank;
		_memList[i].bankPos = entries[i].offset;
		_memList[i].packedSize = entries[i].packedSize;
		_memList[i].unpackedSize = entries[i].unpackedSize;
	}
	_memList[count].status = 0xFF;
}

void Resource::load() {
	while (1) {
		MemEntry *it = _memList;
		MemEntry *me = 0;

		// get resource with max rankNum
		uint8_t maxNum = 0;
		uint16_t i = _numMemList;
		while (i--) {
			if (it->status == STATUS_TOLOAD && maxNum <= it->rankNum) {
				maxNum = it->rankNum;
				me = it;
			}
			++it;
		}
		if (me == 0) {
			break; // no entry found
		}

		uint8_t *memPtr = 0;
		if (me->type == 2) {
			memPtr = _vidCurPtr;
		} else {
			memPtr = _scriptCurPtr;
			if (me->unpackedSize > uint32_t(_vidBakPtr - _scriptCurPtr)) {
				warning("Resource::load() not enough memory");
				me->status = STATUS_NULL;
				continue;
			}
		}
		if (me->bankNum == 0) {
			warning("Resource::load() ec=0x%X (me->bankNum == 0)", 0xF00);
			me->status = STATUS_NULL;
		} else {
			debug(DBG_BANK, "Resource::load() bufPos=%X size=%X type=%X pos=%X bankNum=%X", memPtr - _memPtrStart, me->packedSize, me->type, me->bankPos, me->bankNum);
			readBank(me, memPtr);
			if(me->type == 2) {
				_vid->copyPagePtr(_vidCurPtr);
				me->status = STATUS_NULL;
			} else {
				me->bufPtr = memPtr;
				me->status = STATUS_LOADED;
				_scriptCurPtr += me->unpackedSize;
			}
		}
	}
}

void Resource::invalidateRes() {
	MemEntry *me = _memList;
	uint16_t i = _numMemList;
	while (i--) {
		if (me->type <= 2 || me->type > 6) {
			me->status = STATUS_NULL;
		}
		++me;
	}
	_scriptCurPtr = _scriptBakPtr;
}

void Resource::invalidateAll() {
	MemEntry *me = _memList;
	uint16_t i = _numMemList;
	while (i--) {
		me->status = STATUS_NULL;
		++me;
	}
	_scriptCurPtr = _memPtrStart;
}

const uint16_t Resource::_memListAudio[] = { 
	8, 0x10, 0x61, 0x66, 0xFFFF
};

static const int _memListBmp[] = {
	145, 144, 73, 72, 70, 69, 68, 67, -1
};

void Resource::update(uint16_t num) {
	if (_dataType == DT_15TH_EDITION) {
		if (num > 16000) {
			_newPtrsId = num;
		} else if (num >= 3000) {
			loadBmp(num);
		} else {
			for (int i = 0; _memListBmp[i] != -1; ++i) {
				if (num == _memListBmp[i]) {
					loadBmp(num);
					break;
				}
			}
		}
		return;
	}
	if (num > _numMemList) {
		_newPtrsId = num;
	} else {
		for (const uint16_t *ml = _memListAudio; *ml != 0xFFFF; ++ml) {
			if (*ml == num)
				return;
		}		
		MemEntry *me = &_memList[num];
		if (me->status == STATUS_NULL) {
			me->status = STATUS_TOLOAD;
			load();
		}
	}
}

void Resource::loadBmp(int num) {
	char name[16];
	if (num >= 3000) {
		snprintf(name, sizeof(name), "e%04d.bmp", num);
	} else {
		snprintf(name, sizeof(name), "file%03d.bmp", num);
	}
	const PakEntry *e = _pak.find(name);
	if (e) {
		uint8_t *tmp = (uint8_t *)malloc(e->size);
		if (tmp) {
			uint32_t size;
			_pak.loadData(e, tmp, &size);
			_vid->copyBitmapPtr(tmp);
			free(tmp);
		}
	}
}

uint8_t *Resource::loadDat(int num) {
	if (_memList[num].status == STATUS_LOADED) {
		return _memList[num].bufPtr;
	}
	uint8_t *p = 0;
	char name[16];
	snprintf(name, sizeof(name), "file%03d.dat", num);
	const PakEntry *e = _pak.find(name);
	if (e) {
		uint32_t size;
		_pak.loadData(e, _scriptCurPtr, &size);
		p = _scriptCurPtr;
		_scriptCurPtr += size;
		_memList[num].bufPtr = p;
		_memList[num].status = STATUS_LOADED;
	} else {
		warning("Unable to load '%s'", name);
	}
	return p;
}

uint8_t *Resource::loadWav(int num) {
	if (_memList[num].status == STATUS_LOADED) {
		return _memList[num].bufPtr;
	}
	uint8_t *p = 0;
	char name[16];
	snprintf(name, sizeof(name), "file%03d.wav", num);
	const PakEntry *e = _pak.find(name);
	if (e) {
		uint32_t size;
		_pak.loadData(e, _scriptCurPtr, &size);
		p = _scriptCurPtr;
		_scriptCurPtr += size;
		_memList[num].bufPtr = p;
		_memList[num].status = STATUS_LOADED;
	}
	return p;
}

void Resource::setupPtrs(uint16_t ptrId) {
	if (_dataType == DT_15TH_EDITION) {
		if (ptrId >= 16001 && ptrId <= 16009) {
			invalidateAll();
			static const int order[] = { 0, 1, 2, 3 };
			uint8_t **segments[4] = { &_segVideoPal, &_segCode, &_segVideo1, &_segVideo2 };
			for (int i = 0; i < 4; ++i) {
				const int num = _memListParts[ptrId - 16000][order[i]];
				if (num != 0) {
					*segments[order[i]] = loadDat(num);
				}
			}
			_curPtrsId = ptrId;
		}
		_scriptBakPtr = _scriptCurPtr;
		return;
	}
	if (ptrId != _curPtrsId) {
		uint8_t ipal = 0;
		uint8_t icod = 0;
		uint8_t ivd1 = 0;
		uint8_t ivd2 = 0;
		if (ptrId >= 16000 && ptrId <= 16009) {
			uint16_t part = ptrId - 16000;
			ipal = _memListParts[part][0];
			icod = _memListParts[part][1];
			ivd1 = _memListParts[part][2];
			ivd2 = _memListParts[part][3];
		} else {
			error("Resource::setupPtrs() ec=0x%X invalid ptrId", 0xF07);
		}
		invalidateAll();
		_memList[ipal].status = STATUS_TOLOAD;
		_memList[icod].status = STATUS_TOLOAD;
		_memList[ivd1].status = STATUS_TOLOAD;
		if (ivd2 != 0) {
			_memList[ivd2].status = STATUS_TOLOAD;
		}
		load();
		_segVideoPal = _memList[ipal].bufPtr;
		_segCode = _memList[icod].bufPtr;
		_segVideo1 = _memList[ivd1].bufPtr;
		if (ivd2 != 0) {
			_segVideo2 = _memList[ivd2].bufPtr;
		}
		_curPtrsId = ptrId;
	}
	_scriptBakPtr = _scriptCurPtr;	
}

void Resource::allocMemBlock() {
	_memPtrStart = (uint8_t *)malloc(MEM_BLOCK_SIZE);
	_scriptBakPtr = _scriptCurPtr = _memPtrStart;
	_vidBakPtr = _vidCurPtr = _memPtrStart + MEM_BLOCK_SIZE - 0x800 * 16;
	_useSegVideo2 = false;
}

void Resource::freeMemBlock() {
	free(_memPtrStart);
}