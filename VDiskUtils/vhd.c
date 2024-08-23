#include "vdisk.h"
#include "console.h"
#include <stdlib.h>











// TODO: byteswap where needed














typedef struct _VHD_FIXED_DRIVER {
	VDISK_GET_SIZE getSize;
	VDISK_READ read;
	VDISK_WRITE write;
	VDISK_DRIVER_EXIT exit;
	PVDISK vdisk;
	size_t length;
} VHD_FIXED_DRIVER, * PVHD_FIXED_DRIVER;

BSTATUS getFixedSize(_In_ PVDISK vdisk, _Inout_ size_t* length) {
	PVHD_FIXED_DRIVER drv = (PVHD_FIXED_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	*length = drv->length;
	return TRUE;
}

BSTATUS fixedVHDRead(_Inout_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer) {
	PVHD_FIXED_DRIVER drv = (PVHD_FIXED_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED) == 0) {
		errPrintf("VDISK not mapped!\n\r");
		return FALSE;
	}
	if ((offset + length) > drv->length) {
		errPrintf("Read out of bounds!\n\r");
		return FALSE;
	}
	memcpy(buffer, (void*)(((uint64_t)(vdisk->buffer)) + offset), length);
	return TRUE;
}

BSTATUS fixedVHDWrite(_Inout_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
	PVHD_FIXED_DRIVER drv = (PVHD_FIXED_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	if ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_MAPPED) == 0) {
		errPrintf("VDISK not mapped!\n\r");
		return FALSE;
	}
	if ((offset + length) > drv->length) {
		errPrintf("Write out of bounds!\n\r");
		return FALSE;
	}
	memcpy((void*)(((uint64_t)(vdisk->buffer)) + offset), buffer, length);
	return TRUE;
}

void exitFixedVHDDriver(_In_ PVHD_FIXED_DRIVER t) {
	memset(t, 0, sizeof(VHD_FIXED_DRIVER));
	HeapFree(proc_heap, 0, t); // deallocate this driver because its an instance created for a specific vdisk
}

typedef struct _VHD_DYNAMIC_DRIVER {
	VDISK_GET_SIZE getSize;
	VDISK_READ read;
	VDISK_WRITE write;
	VDISK_DRIVER_EXIT exit;
	PVDISK vdisk;
	uint64_t footer_offset;
	uint32_t blocksize;
	uint32_t max_entries;
	uint64_t bat_offset;
} VHD_DYNAMIC_DRIVER, * PVHD_DYNAMIC_DRIVER;

BSTATUS getDynamicVHDSize(_In_ PVDISK vdisk, _Inout_ size_t* length) {
	PVHD_DYNAMIC_DRIVER drv = (PVHD_DYNAMIC_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	*length = VDISK_GET_STRUCT(PVHD_FOOTER, vdisk, drv->footer_offset)->current_size;
	return TRUE;
}

BSTATUS dynamicVHDRead(_Inout_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer) {
	PVHD_DYNAMIC_DRIVER drv = (PVHD_DYNAMIC_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	if (length == 0) {
		errPrintf("length cannot be zero!\n\r");
		return FALSE;
	}
	uint64_t end = offset + length - 1;
	uint64_t f_off = drv->footer_offset;
	if (end > VDISK_GET_STRUCT(PVHD_FOOTER, vdisk, f_off)->current_size) {
		errPrintf("Read out of bounds!\n\r");
		return FALSE;
	}
	--end;
	const uint64_t bs = (uint64_t)(drv->blocksize);
	uint64_t block_s = offset / bs;
	uint64_t block_e = end / bs;
	if (block_e >= (uint64_t)(drv->max_entries)) {
		errPrintf("Read out of bounds!\n\r");
		return FALSE;
	}

	const uint64_t btmp_tmp = (bs >> 12) - 1;
	const uint64_t btmp_s = btmp_tmp + 0x200 - (btmp_tmp & 0x1ff);

	if (block_e > block_s) {
		errPrintf("Negative read length not possible!\n\r");
		return FALSE;
	}
	else if (block_e == block_s) {
		uint32_t b_num = _byteswap_ulong(VDISK_GET_STRUCT(uint32_t*, vdisk, drv->bat_offset)[block_s]);
		if (b_num == 0xFFFFFFFF) {
			memset(buffer, 0, length);
			return TRUE;
		}
		else {
			uint64_t b_off = offset & (bs - 1);
			uint64_t data_off = ((uint64_t)b_num) << 9;
			if (data_off >= f_off || data_off <= drv->bat_offset) {
				errPrintf("Invalid data block offset!\n\r");
				return FALSE;
			}
			memcpy(buffer, VDISK_GET_STRUCT(void*, vdisk, b_off + data_off + btmp_s), length);
			return TRUE;
		}
	}
	else {
		uint64_t b_off_s = offset & (bs - 1);
		uint64_t b_off_e = end & (bs - 1);

		uint32_t* bat_temp = VDISK_GET_STRUCT(uint32_t*, vdisk, drv->bat_offset + (block_s << 2));
		uint64_t cnt = block_e - block_s;

		uint64_t ptr = (uint64_t)buffer;

		uint64_t start_s = bs - b_off_s;
		if (*bat_temp == 0xFFFFFFFF) {
			memset((void*)ptr, 0, start_s);
		}
		else {
			memcpy((void*)ptr, VDISK_GET_STRUCT(void*, vdisk, ((uint64_t)(_byteswap_ulong(*bat_temp)) << 9) + b_off_s + btmp_s), start_s);
		}
		ptr += start_s;
		++bat_temp;
		--cnt;

		while (cnt) {

			if (*bat_temp == 0xFFFFFFFF) {
				memset((void*)ptr, 0, bs);
			}
			else {
				memcpy((void*)ptr, VDISK_GET_STRUCT(void*, vdisk, ((uint64_t)(_byteswap_ulong(*bat_temp)) << 9) + btmp_s), bs);
			}

			ptr += bs;
			++bat_temp;
			--cnt;
		}

		if (*bat_temp == 0xFFFFFFFF) {
			memset((void*)ptr, 0, b_off_e);
		}
		else {
			memcpy((void*)ptr, VDISK_GET_STRUCT(void*, vdisk, ((uint64_t)(_byteswap_ulong(*bat_temp)) << 9) + btmp_s), b_off_e);
		}

		return TRUE;

	}
	
	return FALSE;
}

// unt(ru/e)sted -> pray for it working!
BSTATUS dynamicVHDWrite(_Inout_ PVDISK vdisk, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
	PVHD_DYNAMIC_DRIVER drv = (PVHD_DYNAMIC_DRIVER)(vdisk->driver);
	if (vdisk != drv->vdisk) {
		errPrintf("Wrong vdisk for this driver!\n\r");
		return FALSE;
	}
	if (length == 0) {
		errPrintf("length cannot be zero!\n\r");
		return FALSE;
	}
	const uint64_t bs = (uint64_t)(drv->blocksize);
	uint64_t end = offset + length - 1;
	uint64_t f_off = drv->footer_offset;
	if (end > VDISK_GET_STRUCT(PVHD_FOOTER, vdisk, f_off)->current_size) {
		errPrintf("Read out of bounds!\n\r");
		return FALSE;
	}
	--end;
	uint64_t block_s = offset / bs;
	uint64_t block_e = end / bs;
	if (block_e >= (uint64_t)(drv->max_entries)) {
		errPrintf("Read out of bounds!\n\r");
		return FALSE;
	}

	const uint64_t btmp_tmp = (bs >> 12) - 1;
	const uint64_t btmp_s = btmp_tmp + 0x200 - (btmp_tmp & 0x1ff);

	if (block_e > block_s) {
		errPrintf("Negative write length not possible!\n\r");
		return FALSE;
	}
	else if (block_e == block_s) {
		uint32_t* b_num = VDISK_GET_STRUCT(uint32_t*, vdisk, drv->bat_offset + (block_s << 2));
		if (*b_num == 0xFFFFFFFF) {
			uint64_t exp = bs + btmp_s;
			if (((f_off + exp)>> 9) > 0xffffffff) {
				errPrintf("Disk too big!\n\r");
				return FALSE;
			}
			if (expandVDISKFile(vdisk, exp)) {
				memcpy(VDISK_GET_STRUCT(void*, vdisk, f_off + exp), VDISK_GET_STRUCT(void*, vdisk, f_off), 0x200);
				memset(VDISK_GET_STRUCT(void*, vdisk, f_off), 0, 0x200);
				uint64_t off = (drv->footer_offset) >> 9;
				*b_num = _byteswap_ulong((uint32_t)off);
				drv->footer_offset += exp;
			}
			else {
				errPrintf("Abort?: Disk expansion failed!\n\r");
				// idk what to do now (Abort because driver is destructed...?)
				return FALSE;
			}
			//TODO: init bitmap
		}
		uint64_t b_off = offset & (bs - 1);
		uint64_t data_off = ((uint64_t)(_byteswap_ulong(*b_num)) << 9) + btmp_s;
		if (data_off >= f_off || data_off <= drv->bat_offset) {
			errPrintf("Invalid data block offset!\n\r");
			return FALSE;
		}
		memcpy(VDISK_GET_STRUCT(void*, vdisk, b_off + data_off), buffer, length);
		return TRUE;
	}
	else {
		uint64_t b_off_s = offset & (bs - 1);
		uint64_t b_off_e = end & (bs - 1);

		uint32_t* bat_temp = VDISK_GET_STRUCT(uint32_t*, vdisk, drv->bat_offset + (block_s << 2));
		uint64_t cnt = block_e - block_s;

		uint64_t alloc_c = 0;
		while (cnt) {
			if (*bat_temp == 0xFFFFFFFF) {
				++alloc_c;
			}
			++bat_temp;
			--cnt;
		}

		uint64_t exp = 0;
		uint32_t alloc_current = (uint32_t)(f_off >> 9);
		if (alloc_c != 0) {
			exp = alloc_c * (bs + btmp_s);
			if (((f_off + exp) >> 9) > 0xffffffff) {
				errPrintf("Disk too big!\n\r");
				return FALSE;
			}
			if (expandVDISKFile(vdisk, exp)) {
				memcpy(VDISK_GET_STRUCT(void*, vdisk, f_off + exp), VDISK_GET_STRUCT(void*, vdisk, f_off), 0x200);
				memset(VDISK_GET_STRUCT(void*, vdisk, f_off), 0, 0x200);
				drv->footer_offset += exp;
			}
			else {
				errPrintf("Abort?: Disk expansion failed!\n\r");
				// idk what to do now (Abort because driver is destructed?...)
				return FALSE;
			}
		}

		bat_temp = VDISK_GET_STRUCT(uint32_t*, vdisk, drv->bat_offset + (block_s << 2));
		cnt = block_e - block_s;

		uint64_t ptr = (uint64_t)buffer;
		const uint64_t alloc_sz = (bs + btmp_s) >> 9;

		uint64_t start_s = bs - b_off_s;
		if (*bat_temp == 0xFFFFFFFF) {
			*bat_temp = _byteswap_ulong(alloc_current);
			alloc_current += alloc_sz;
		}
		memcpy(VDISK_GET_STRUCT(void*, vdisk, (((uint64_t)_byteswap_ulong(*bat_temp)) << 9) + b_off_s + btmp_s), (void*)ptr, start_s);
		ptr += start_s;
		++bat_temp;
		--cnt;

		while (cnt) {

			if (*bat_temp == 0xFFFFFFFF) {
				*bat_temp = _byteswap_ulong(alloc_current);
				alloc_current += alloc_sz;
				//TODO: intit bitmap
			}
			memcpy(VDISK_GET_STRUCT(void*, vdisk, (((uint64_t)_byteswap_ulong(*bat_temp)) << 9) + btmp_s), (void*)ptr, bs);

			ptr += bs;
			++bat_temp;
			--cnt;
		}

		if (*bat_temp == 0xFFFFFFFF) {
			*bat_temp = _byteswap_ulong(alloc_current);
			alloc_current += alloc_sz;
		}
		memcpy(VDISK_GET_STRUCT(void*, vdisk, (((uint64_t)_byteswap_ulong(*bat_temp)) << 9) + btmp_s), (void*)ptr, b_off_e);

		return TRUE;

	}

	return FALSE;
}

void exitDynamicVHDDriver(_In_ PVHD_FIXED_DRIVER t) {
	memset(t, 0, sizeof(VHD_DYNAMIC_DRIVER));
	HeapFree(proc_heap, 0, t); // deallocate this driver because its an instance created for a specific vdisk
}

BOOL isVHDPresent(_In_ PVDISK vdisk) {
	if (!mapVDISKFile(vdisk)) {
		return FALSE;
	}
	PVHD_FOOTER footer = (PVHD_FOOTER)(((uint64_t)(vdisk->buffer)) + vdisk->length - 512);
	if (footer->cookie != VHD_COOKIE) {
		dbgPrintf("Invalid VHD cookie\n\r");
		return FALSE;
	}
	if ((footer->features & VHD_FEATURES_RESERVED) == 0) {
		dbgPrintf("Invalid VHD features\n\r");
		return FALSE;
	}
	if (footer->file_format_version != VHD_VERSION) {
		dbgPrintf("Invalid VHD format version\n\r");
		return FALSE;
	}
	uint32_t type = footer->disk_type;
	if ((type != VHD_DISK_TYPE_FIXED) && (type != VHD_DISK_TYPE_DYNAMIC) && (type != VHD_DISK_TYPE_DIFF)) {
		dbgPrintf("Invalid VHD type\n\r");
		return FALSE;
	}
	uint64_t offset = _byteswap_uint64(footer->offset);
	if ((type == VHD_DISK_TYPE_FIXED) && (offset != 0xFFFFFFFFFFFFFFFF)) {
		dbgPrintf("Invalid VHD data offset\n\r");
		return FALSE;
	}
	uint32_t checksum = 0;
	for (int i = 0; i < 64; i++) {
		checksum += (uint32_t)(((uint8_t*)footer)[i]);
	}
	for (int i = 68; i < 85; i++) {
		checksum += (uint32_t)(((uint8_t*)footer)[i]);
	}
	checksum = _byteswap_ulong(~checksum);
	if (checksum != footer->checksum) {
		dbgPrintf("Invalid VHD checksum\n\r");
		return FALSE;
	}
	uint64_t size = _byteswap_uint64(footer->current_size);
	if (type == VHD_DISK_TYPE_FIXED) {
		if (size > vdisk->length - 512) {
			dbgPrintf("Invalid VHD size\n\r");
			return FALSE;
		}
	}
	else {
		if (memcmp(footer, vdisk->buffer, 512) != 0) {
			dbgPrintf("Inalid VHD footer copy\n\r");
			return FALSE;
		}
		if (((offset + 512) >= vdisk->length) || (offset < 512) || ((offset & 0x1FF) != 0)) {
			dbgPrintf("Invalid VHD data offset\n\r");
			return FALSE;
		}
		PVHD_DYNAMIC_HEADER vhd = (PVHD_DYNAMIC_HEADER)(((uint64_t)(vdisk->buffer)) + offset);
		if (vhd->cookie != VHD_DYNAMIC_COOKIE) {
			dbgPrintf("Invalid Dynamic VHD cookie\n\r");
			return FALSE;
		}
		if (vhd->data_offset != 0xFFFFFFFFFFFFFFFF) {
			dbgPrintf("Invalid Dynamic VHD data offset\n\r");
			return FALSE;
		}
		if (vhd->header_version != VHD_DYNAMIC_VERSION) {
			dbgPrintf("Ivalid Dynamic VHD header version\n\r");
			return FALSE;
		}
		uint32_t bs = vhd->block_size;
		if (((bs - 1) & bs) != 0) {
			errPrintf("Invalid blocksize:%x (must be power of two)\n\r", bs);
			return FALSE;
		}
		checksum = 0;
		for (int i = 0; i < 36; i++) {
			checksum += (uint32_t)(((uint8_t*)vhd)[i]);
		}
		for (int i = 40; i < 1024; i++) {
			checksum += (uint32_t)(((uint8_t*)vhd)[i]);
		}
		checksum = _byteswap_ulong(~checksum);
		if (checksum != vhd->checkusm) {
			dbgPrintf("Invalid Dynamic VHD checksum\n\r");
			return FALSE;
		}
	}
	return TRUE;
}

BSTATUS setVHDDriver(_Inout_ PVDISK vdisk) {
	size_t datalen = vdisk->length - 512;
	PVHD_FOOTER footer = (PVHD_FOOTER)(((uint64_t)(vdisk->buffer)) + datalen);
	if (footer->disk_type == VHD_DISK_TYPE_FIXED) {
		PVHD_FIXED_DRIVER fixed = HeapAlloc(proc_heap, 0, sizeof(VHD_FIXED_DRIVER));
		if (!fixed) {
			errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
			return FALSE;
		}
		fixed->getSize = getFixedSize;
		fixed->read = fixedVHDRead;
		fixed->write = fixedVHDWrite;
		fixed->exit = exitFixedVHDDriver;
		fixed->vdisk = vdisk;
		fixed->length = datalen;
		if(vdisk->driver)
			vdisk->driver->exit(vdisk->driver);
		vdisk->driver = (PVDISK_DRIVER)fixed;
		return TRUE;
	}
	else if (footer->disk_type == VHD_DISK_TYPE_DYNAMIC) {
		PVHD_DYNAMIC_DRIVER dyn = HeapAlloc(proc_heap, 0, sizeof(VHD_DYNAMIC_DRIVER));
		if (!dyn) {
			errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
			return FALSE;
		}
		dyn->getSize = getDynamicVHDSize;
		dyn->read = dynamicVHDRead;
		dyn->write = dynamicVHDWrite;
		dyn->exit = exitDynamicVHDDriver;
		dyn->vdisk = vdisk;
		dyn->footer_offset = datalen;
		PVHD_DYNAMIC_HEADER hdr = (PVHD_DYNAMIC_HEADER)(((uint64_t)(vdisk->buffer)) + _byteswap_uint64(footer->offset));
		dyn->blocksize = _byteswap_ulong(hdr->block_size);
		dyn->max_entries = _byteswap_ulong(hdr->max_table_entries);
		dyn->bat_offset = _byteswap_uint64(hdr->table_offset);
		if (vdisk->driver)
			vdisk->driver->exit(vdisk->driver);
		vdisk->driver = (PVDISK_DRIVER)dyn;
		return TRUE;
	}
	errPrintf("Usupported VHD type\n\r");
	return FALSE;
}