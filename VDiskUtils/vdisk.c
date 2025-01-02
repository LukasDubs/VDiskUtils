#include "vdisk.h"
#include "console.h"

PVDISK* vdisk_list = 0;
size_t n_vdisk = 0;
size_t currentVDISK = 0;
size_t currentPartition = 0xFFFFFFFFFFFFFFFF;
PVDISK_DRIVER raw_driver = 0;

BSTATUS getRawSize(_In_ PVDISK vdisk, _Inout_ size_t* length) {
	*length = vdisk->length;
	return TRUE;
}

BSTATUS rawRead(_In_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer) {
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED)) {
		if (vdisk->length < (offset + length)) {
			errPrintf("Read exceeds bounds!\n\r");
			return FALSE;
		}
		memcpy(buffer, (void*)(((uint64_t)(vdisk->buffer)) + offset), length);
		return TRUE;
	}
	else {
		errPrintf("Buffer not mapped!\n\r");
		return FALSE;
	}
}

BSTATUS rawWrite(_In_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED)) {
		if (vdisk->length < (offset + length)) {
			errPrintf("Write exceeds bounds!\n\r");
			return FALSE;
		}
		memcpy((void*)(((uint64_t)(vdisk->buffer)) + offset), buffer, length);
		return TRUE;
	}
	else {
		errPrintf("Buffer not mapped!\n\r");
		return FALSE;
	}
}

void exitRawDriver(_In_ PVDISK_DRIVER this_ptr) {
	// global driver, nothing to do
}

BOOL initVDISKs() {
	raw_driver = HeapAlloc(proc_heap, 0, sizeof(VDISK_DRIVER));
	if (!raw_driver) {
		return FALSE;
	}
	raw_driver->get_size = getRawSize;
	raw_driver->read = rawRead;
	raw_driver->write = rawWrite;
	raw_driver->exit = exitRawDriver;
	vdisk_list = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	return vdisk_list != 0;
}

BSTATUS static closeVDISKInternal(_In_ PVDISK vdisk) {
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0) {
		errPrintf("Error: File already closed!\n\r");
		return FALSE;
	}
	else {
		if (vdisk->driver) {
			vdisk->driver->exit(vdisk->driver);
			vdisk->driver = 0;
		}
		if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED)) {
			UnmapViewOfFile(vdisk->buffer);
			CloseHandle(vdisk->mapping);
			vdisk->buffer = 0;
			vdisk->mapping = 0;
		}
		CloseHandle(vdisk->file);
		vdisk->file = INVALID_HANDLE_VALUE;
		vdisk->length = 0;
		vdisk->attributes = 0;
		return TRUE;
	}
}

BSTATUS static dectectVDISKContent(_In_ PVDISK vdisk) {
	if (isVHDPresent(vdisk)) {
		if (!setVHDDriver(vdisk)) {
			closeVDISKInternal(vdisk);
			return FALSE;
		}
	}
	if (isMBRPresent(vdisk)) {
		if (isGPTPresent(vdisk)) {
			vdisk->attributes |= VDISK_ATTRIBUTE_FLAG_GPT;
		}
		else {
			vdisk->attributes |= VDISK_ATTRIBUTE_FLAG_MBR;
		}
	}
	else {
		exePrintf("No format detected! Type y to enter raw read mode or anything else to continue:");
		wchar_t c = 0;
		DWORD read = 0;
		rdCon(&c, 1, &read);
		if (c == L'y' || c == L'Y') {
			vdisk->attributes |= VDISK_ATTRIBUTE_FLAG_RAW;
		}
	}
	return TRUE;
}

BSTATUS openVdiskP(_In_ LPWSTR file) {
	PVDISK vdisk;
	for (size_t i = 0; i < n_vdisk; i++) {
		if (wcscmp(file, vdisk_list[i]->path) == 0) {
			if (vdisk_list[i]->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) {
				errPrintf("Error: File already open!\n\r");
				return FALSE;
			}
			else {
				HANDLE f = CreateFileW(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				if (f == INVALID_HANDLE_VALUE) {
					errPrintf("Opening file:%ws win32:%x\n\r", file, GetLastError());
					return FALSE;
				}
				if (!GetFileSizeEx(f, (PLARGE_INTEGER)&(vdisk_list[i]->length))) {
					CloseHandle(f);
					errPrintf("Error getting file size!\n\r");
					return FALSE;
				}
				vdisk_list[i]->file = f;
				vdisk_list[i]->attributes |= VDISK_ATTRIBUTE_FLAG_OPEN;
				vdisk = vdisk_list[i];
				vdisk->driver = raw_driver;
				goto _detect;
			}
		}
	}
	HANDLE f = CreateFileW(file, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (f == INVALID_HANDLE_VALUE) {
		errPrintf("Error on opening file:%ws win32:%x\n\r", file, GetLastError());
		return FALSE;
	}
	uint64_t size = 0; 
	if (!GetFileSizeEx(f, (PLARGE_INTEGER)&size)) {
		CloseHandle(f);
		errPrintf("Error getting file size!\n\r");
		return FALSE;
	}
	vdisk = HeapAlloc(proc_heap, 0, sizeof(VDISK));
	if (vdisk == 0) {
		errPrintf("Error on allocating vdisk handle! win32:%x\n\r", GetLastError());
		CloseHandle(f);
		return FALSE;
	}
	size_t len = (wcslen(file) + 1) << 1;
	PVOID strbuf = HeapAlloc(proc_heap, 0, len);
	if (strbuf == 0) {
		CloseHandle(f);
		HeapFree(proc_heap, 0, vdisk);
		return FALSE;
	}
	memset(vdisk, 0, sizeof(VDISK));
	memcpy(strbuf, file, len);
	vdisk->file = f;
	vdisk->path = strbuf;
	vdisk->attributes = VDISK_ATTRIBUTE_FLAG_OPEN;
	vdisk->length = size;
	vdisk->driver = raw_driver;
	vdisk_list[n_vdisk++] = vdisk;

_detect:
	return dectectVDISKContent(vdisk);
}

BSTATUS openVdiskI(_In_ size_t i) {
	if (i == (size_t)-1) {
		if (currentVDISK < n_vdisk) {
			i = currentVDISK;
		}
		else {
			errPrintf("Error: no valid vdisk selected!\n\r");
			return FALSE;
		}
	}
	else if (i >= n_vdisk) {
		errPrintf("Error: Invalid Index:%llu\n\r", i);
		return FALSE;
	}
	if (vdisk_list[i]->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) {
		errPrintf("Error: File already open!\n\r");
		return FALSE;
	}
	else {
		HANDLE f = CreateFileW(vdisk_list[i]->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, 0);
		if (f == INVALID_HANDLE_VALUE) {
			errPrintf("Opening file:%ws win32:%x\n\r", vdisk_list[i]->path, GetLastError());
			return FALSE;
		}
		if (!GetFileSizeEx(f, (PLARGE_INTEGER)&(vdisk_list[i]->length))) {
			CloseHandle(f);
			errPrintf("Error getting file size!\n\r");
			return FALSE;
		}
		vdisk_list[i]->file = f;
		vdisk_list[i]->attributes |= VDISK_ATTRIBUTE_FLAG_OPEN;
		vdisk_list[i]->driver = raw_driver;
	}

	return dectectVDISKContent(vdisk_list[i]);
}

BSTATUS closeVDISK(_In_ size_t i) {
	if (i == (size_t)-1) {
		if (currentVDISK < n_vdisk) {
			i = currentVDISK;
		}
		else {
			errPrintf("Error: no valid vdisk selected!\n\r");
			return FALSE;
		}
	}
	else if (i >= n_vdisk) {
		errPrintf("Error: Invalid Index:%llu\n\r", i);
		return FALSE;
	}
	PVDISK vdisk = vdisk_list[i];
	return closeVDISKInternal(vdisk);
}

void listVDISKs() {
	exePrintf("Vdisks:\n\r");
	for (size_t i = 0; i < n_vdisk; i++) {
		exePrintf("\t%llu file:%ws attributes:%x\n\r", i, vdisk_list[i]->path, vdisk_list[i]->attributes);
	}
	exePrintf("\n\r");
}

BSTATUS selectVdiskI(_In_ size_t index) {
	if (index >= n_vdisk) {
		errPrintf("Invalid Index!\n\r");
		return FALSE;
	}
	currentVDISK = index;
	return TRUE;
}

BSTATUS selectVdiskP(_In_ LPCWSTR path) {
	for (size_t i = 0; i < n_vdisk; i++) {
		if (wcscmp(path, vdisk_list[i]->path) == 0) {
			if (i != currentVDISK) {
				currentVDISK = i;
			}
			return TRUE;
		}
	}
	errPrintf("Error: Path not found:%ws\n\r", path);
	return FALSE;
}

BSTATUS mapVDISKFile(_Inout_ PVDISK vdisk) {
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0) {
		errPrintf("File not open!\n\r");
		return FALSE;
	}
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED) != 0)return TRUE;
	HANDLE mapping = CreateFileMapping(vdisk->file, 0, PAGE_READWRITE, 0, 0, 0);
	if (mapping == INVALID_HANDLE_VALUE || mapping == 0) {
		errPrintf("Error on creating file mapping:%x\n\r", GetLastError());
		return FALSE;
	}
	vdisk->mapping = mapping;

	void* buffer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, vdisk->length);
	if (buffer == 0) {
		errPrintf("Error on mapping file:%x\n\r", GetLastError());
		CloseHandle(mapping);
		vdisk->mapping = 0;
		return FALSE;
	}
	vdisk->buffer = buffer;
	vdisk->attributes |= VDISK_ATTRIBUTE_FLAG_MAPPED;
	return TRUE;
}

_Check_return_ BSTATUS expandVDISKFile(_Inout_ PVDISK vdisk, _In_opt_ uint64_t amount) {
	if (amount == 0)return TRUE;
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0) {
		errPrintf("File not open!\n\r");
		return FALSE;
	}
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED) != 0) {
		UnmapViewOfFile(vdisk->buffer);
		CloseHandle(vdisk->mapping);

		BSTATUS status = FALSE;
		if (setEOF(vdisk->file, vdisk->length + amount)) {
			vdisk->length += amount;
			status = TRUE;
		}

		HANDLE mapping = CreateFileMapping(vdisk->file, 0, PAGE_READWRITE, 0, 0, 0);
		if (mapping == INVALID_HANDLE_VALUE || mapping == 0) {
			errPrintf("Error on creating file mapping:%x\n\r", GetLastError());
			vdisk->buffer = 0;
			vdisk->mapping = 0;
			vdisk->attributes &= ~VDISK_ATTRIBUTE_FLAG_MAPPED;
			return FALSE;
		}
		vdisk->mapping = mapping;

		void* buffer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, vdisk->length);
		if (buffer == 0) {
			errPrintf("Error on mapping file:%x\n\r", GetLastError());
			CloseHandle(mapping);
			vdisk->mapping = 0;
			vdisk->buffer = 0;
			vdisk->attributes &= ~VDISK_ATTRIBUTE_FLAG_MAPPED;
			return FALSE;
		}
		vdisk->buffer = buffer;

		return status;
	}
	else {
		if (setEOF(vdisk->file, vdisk->length + amount)) {
			vdisk->length += amount;
			return TRUE;
		}
		else {
			errPrintf("Error expanding file:%x\n\r", GetLastError());
			return FALSE;
		}
	}
}

void listPartitions() {
	if (currentVDISK >= n_vdisk || ((vdisk_list[currentVDISK]->attributes) & VDISK_ATTRIBUTE_FLAG_OPEN) == 0) {
		errPrintf("Invalid VDISk selected!\n\r");
		return;
	}
	PVDISK vdisk = vdisk_list[currentVDISK];
	uint32_t flags = ((vdisk->attributes & (VDISK_ATTRIBUTE_FLAG_GPT | VDISK_ATTRIBUTE_FLAG_MBR)));
	if (flags == VDISK_ATTRIBUTE_FLAG_GPT) {
		PPARTITION parts = vdisk_list[currentVDISK]->partitions;
		uint32_t num = vdisk_list[currentVDISK]->n_partitions;
		if (parts == 0) {
			if (!getGPTPartitions(vdisk, &parts, &num)) {
				errPrintf("Couldn't get Partitions!\n\r");
				return;
			}
			vdisk->n_partitions = num;
			vdisk->partitions = parts;
		}
		exePrintf("Paritions:\n\r");
		PWSTR guidbuf1 = HeapAlloc(proc_heap, 0, 80);
		if (guidbuf1 == 0) {
			errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
			return;
		}
		PWSTR guidbuf2 = HeapAlloc(proc_heap, 0, 80);
		if (guidbuf2 == 0) {
			HeapFree(proc_heap, 0, guidbuf1);
			errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
			return;
		}
		for (uint32_t i = 0; i < num; i++) {
			StrFromGUID(guidbuf1, 40, &(((PGPT_PART_ENTRY)(parts[i].attributes))->part_type_guid));
			StrFromGUID(guidbuf2, 40, &(((PGPT_PART_ENTRY)(parts[i].attributes))->part_guid));
			exePrintf("\t%u: start:%llx length:%llx type:%ws partitions:%ws name:%ws\n\r", i, parts[i].start, parts[i].length, guidbuf1, guidbuf2, &(((PGPT_PART_ENTRY)(parts[i].attributes))->name));
		}
		HeapFree(proc_heap, 0, guidbuf1);
		HeapFree(proc_heap, 0, guidbuf2);
	}
	else if(flags == VDISK_ATTRIBUTE_FLAG_MBR) {
		
		errPrintf("MBR partitioning currently not supported!\n\r");
	}
	else {
		errPrintf("Disk not partitioned!\n\r");
	}
}

BSTATUS selectPartition(_In_ size_t index) {
	PVDISK vdisk = 0;
	if (currentVDISK >= n_vdisk || (((vdisk = vdisk_list[currentVDISK])->attributes) & VDISK_ATTRIBUTE_FLAG_OPEN) == 0) {
		errPrintf("Invalid VDISk selected!\n\r");
		return FALSE;
	}
	if (vdisk->n_partitions == 0) {
		errPrintf("Currently no partitions to be selected!\n\r");
		return FALSE;
	}
	if (index >= vdisk->n_partitions) {
		errPrintf("Invalid Partition Index!\n\r");
		return FALSE;
	}
	currentPartition = index;
	return TRUE;
}

BSTATUS partitionRead(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer) {
	if (partition_index < vdisk->n_partitions) {
		PPARTITION part = (PPARTITION)(((size_t)vdisk->partitions) + (size_t)partition_index * sizeof(PARTITION));
		if (offset + length > part->length) {
			errPrintf("Read out of bounds!\n\r");
			return FALSE;
		}
		return vdisk->driver->read(vdisk, part->start + offset, length, buffer);
	}
	else {
		if (vdisk->n_partitions == 0) {
			if ((vdisk->attributes & VDISK_ATTRIBUTE_MASK_VDISK_PARTITION) != VDISK_ATTRIBUTE_FLAG_RAW) {
				errPrintf("Error: Invalid Partition index!\n\r");
				return FALSE;
			}
			else {
				return vdisk->driver->read(vdisk, offset, length, buffer);
			}
		}
		else {
			errPrintf("Error: Invalid Partition index!\n\r");
			return FALSE;
		}
	}
}

BSTATUS partitionWrite(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
	if (partition_index < vdisk->n_partitions) {
		PPARTITION part = (PPARTITION)(((size_t)vdisk->partitions) + (size_t)partition_index * sizeof(PARTITION));
		if (offset + length > part->length) {
			errPrintf("Write out of bounds!\n\r");
			return FALSE;
		}
		return vdisk->driver->write(vdisk, part->start + offset, length, buffer);
	}
	else {
		if (vdisk->n_partitions == 0) {
			if ((vdisk->attributes & VDISK_ATTRIBUTE_MASK_VDISK_PARTITION) != VDISK_ATTRIBUTE_FLAG_RAW) {
				errPrintf("Error: Invalid Partition index!\n\r");
				return FALSE;
			}
			else {
				return vdisk->driver->write(vdisk, offset, length, buffer);
			}
		}
		else {
			errPrintf("Error: Invalid Partition index!\n\r");
			return FALSE;
		}
	}
}

PFS_DRIVER createDriver(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index) {
	PFS_DRIVER drv = 0;
	if (createFATDriver(vdisk, partition_index, &drv)) {
		exePrintf("Successfully created FAT driver\n\r");
		return drv;
	}
	else if (createNTFSDriver(vdisk, partition_index, &drv)) {
		exePrintf("Successfully created NTFS driver\n\r");
		return drv;
	}
	return 0;
}

PFS_DRIVER getSelDrv() {
	if (currentVDISK >= n_vdisk) {
		errPrintf("No valid VDISK selected!\n\r");
		return 0;
	}
	PFS_DRIVER driver = 0;
	return createDriver(vdisk_list[currentVDISK], (DWORD)currentPartition);
}