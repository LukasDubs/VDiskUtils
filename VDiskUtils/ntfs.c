#include "vdisk.h"
#include "console.h"
#include "ntfs_internal.h"

void static NTFSDriverExit(PNTFS_DRIVER drv) {
	VirtualFree(drv->handle_table, 0, MEM_RELEASE);
	memset(drv, 0, sizeof(NTFS_DRIVER));
	HeapFree(proc_heap, 0, drv);
}

size_t static __inline allocHandle(PNTFS_DRIVER drv) {
	for (size_t i = 0; i < MAX_NTFS_HANDLES; i++) {
		if ((drv->handle_table[i].flags & NTFS_HANDLE_OPEN) == 0) {
			return i;
		}
	}
	return ~(size_t)0;
}

BSTATUS createNTFSDriver(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _Out_ PFS_DRIVER* driver) {

	void* buffer = HeapAlloc(proc_heap, 0, 0x200);
	if (buffer == 0) {
		errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
		return FALSE;
	}
	BSTATUS status = FALSE;
	if (partitionRead(vdisk, partition_index, 0, 0x200, buffer)) {
		PNTFS_BOOT boot = buffer;
		if (isInvalidJMP(boot)) {
			dbgPrintf("Invalid jmp instruction\n\r");
			goto _ntfs_drv_create_exit;
		}
		if (boot->oem_int != NTFS_BOOT_OEM_ID) {
			dbgPrintf("Invalid OEM_ID\n\r");
			goto _ntfs_drv_create_exit;
		}
		uint32_t bps = (uint32_t)(boot->bytes_per_setor);
		if ((bps < 256) || (bps > 4096) || ((bps & (bps - 1)) != 0)) {
			dbgPrintf("Invalid bytes per sector\n\r");
			goto _ntfs_drv_create_exit;
		}
		uint32_t spc = (uint32_t)(boot->sectors_per_cluster);
		if (((spc > 128) && (spc < 244)) || ((spc < 128) && ((spc & (spc - 1)) != 0))) {
			dbgPrintf("Invalid sectors per cluster\n\r");
			goto _ntfs_drv_create_exit;
		}
		if (boot->signature != NTFS_BOOT_SIGNATURE) {
			dbgPrintf("Invalid NTFS-Signature\n\r");
			goto _ntfs_drv_create_exit;
		}
		if (boot->boot_signature != BOOT_SIGNATURE) {
			dbgPrintf("Invalid Boot-Signature\n\r");
			goto _ntfs_drv_create_exit;
		}
		PNTFS_DRIVER drv = HeapAlloc(proc_heap, 0, sizeof(NTFS_DRIVER));
		if (drv == 0) {
			errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
			goto _ntfs_drv_create_exit;
		}
		drv->handle_table = VirtualAlloc(0, MAX_NTFS_HANDLES * sizeof(NTFS_HANDLE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (drv->handle_table == 0) {
			errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
			goto _ntfs_drv_create_exit;
		}

		drv->bytes_per_sector = bps;
		drv->bytes_per_cluster = ((spc > 0x80) ? (1 << (0x100 - spc)) : spc) * bps;
		drv->bytes_per_ind = boot->cluster_per_ind;
		drv->bytes_per_mft = (boot->cluster_per_mft > 0x80) ? (1 << (0x100 - boot->cluster_per_mft)) : (boot->cluster_per_mft * drv->bytes_per_cluster);
		drv->lcn_mft = boot->lcn_of_mft;
		drv->lcn_mirr_mft = boot->lcn_of_mftmirr;
		drv->interf.exit = NTFSDriverExit;
		drv->interf.open = NTFSOpen;
		drv->interf.close = NTFSClose;
		drv->interf.create = NTFSCreate;
		drv->interf.del = NTFSDelete;
		drv->interf.read = NTFSReadFile;
		drv->interf.write = NTFSWriteFile;
		drv->interf.get_info = NTFSGetInfo;
		drv->interf.set_info = NTFSSetInfo;
		drv->interf.vdisk = vdisk;
		drv->interf.partition_index = partition_index;
		drv->interf.fs_type = FS_DRIVER_TYPE_NTFS;
		*driver = (PFS_DRIVER)drv;
		drv->signature = NTFS_SIGNATURE_VALID;
		status = TRUE;
	}
_ntfs_drv_create_exit:
	memset(buffer, 0, 0x200);
	HeapFree(proc_heap, 0, buffer);
	return status;
}

NTSTATUS static readMFTEntry(_In_ PNTFS_DRIVER drv, _In_ size_t entry, _Out_ PNTFS_FILE_RECORD* file) {
	PNTFS_FILE_RECORD buf = VirtualAlloc(0, drv->bytes_per_mft, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (buf == 0) {
		errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
		return STATUS_INTERNAL_ERROR;
	}
	if (partitionRead(drv->interf.vdisk, drv->interf.partition_index, drv->lcn_mft * (size_t)drv->bytes_per_cluster + (size_t)drv->bytes_per_mft * entry, drv->bytes_per_mft, buf)) {
		*file = buf;
		return STATUS_SUCCESS;
	}
	VirtualFree(buf, 0, MEM_RELEASE);
	return STATUS_INTERNAL_ERROR;
}

UINT64 static __inline parseRun(_In_ UINT8* data, _In_ UINT8 n) {
	UINT64 out = 0;
	for (int i = 0; i < n; i++) {
		out |= ((UINT64)(*data)) << (i << 3);
	}
	if ((out & ((UINT64)1 << ((n << 3) - 1)))) {
		out |= 0xFFFFFFFFFFFFFFFF << (n << 3);
	}
	return out;
}

NTSTATUS static __inline analyzeDataRuns(_Inout_ PNTFS_DATA_RUNS runs) {
	if (runs->n == 0)return STATUS_INVALID_PARAMETER;
	UINT64 vcn = 0;
	UINT32 si = 0xffffffff;
	for (UINT32 i = 0; i < runs->n; i++) {
		
		if (((~(runs->runs[i].flags)) & (NTFS_DATA_RUN_FLAG_SPARSE)) == 0) {

			if ((vcn != 0) && (si != 0xffffffff)) {
				for (UINT32 j = si; j < i; j++) {
					runs->runs[j].flags |= NTFS_DATA_RUN_FLAG_COMPRESSED; // mark previous clusters as compressed
				}
				si = 0xffffffff;
			}

			vcn += runs->runs[i].length;
			vcn &= 0xf;
		}
		else {

			vcn += runs->runs[i].length;

			if ((vcn & 0xf) != 0) {
				if ((vcn > 0x10) || (si == 0xffffffff)) {
					si = i;
				}
			}
			else {
				si = 0xFFFFFFFF;
			}

			vcn &= 0xf;
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS static parseDataRuns(_In_ UINT8* data_runs, _Out_ PNTFS_DATA_RUNS* data) {
	UINT32 n_runs = 0;
	UINT8 rd = *data_runs;
	UINT8* rc = data_runs;
	UINT64 total_length = 0;
	while (rd) {
		rc += (size_t)(rd & 0xF) + (size_t)((rd >> 4) & 0xf) + 1;
		rd = *rc;
		n_runs++;
	}
	PNTFS_DATA_RUNS dr = HeapAlloc(proc_heap, 0, (size_t)(n_runs -  ANYSIZE_ARRAY) * sizeof(NTFS_DATA_RUN) + sizeof(NTFS_DATA_RUNS));
	if (dr == 0) {
		errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
		return STATUS_INTERNAL_ERROR;
	}

	dr->n = n_runs;

	if (n_runs == 0) {
		*data = dr;
		return STATUS_SUCCESS;
	}

	UINT64 offset = 0;
	rc = data_runs;
	UINT64 length;
	UINT8 l;
	UINT8 h;

	for (UINT32 i = 0; i < n_runs; i++) {
		rd = *rc;
		++rc;
		l = rd & 0xf;
		h = (rd >> 4) & 0xf;

		length = parseRun(rc, l);
		rc += l;
		offset += parseRun(rc, h);
		rc += h;

		total_length += length;
		dr->runs[i].length = length;
		if (h == 0) {
			dr->runs[i].flags |= NTFS_DATA_RUN_FLAG_SPARSE;
		}
		else {
			dr->runs[i].offset = offset;
		}
	}
	NTSTATUS status = analyzeDataRuns(dr);

	dr->total_length = total_length;

	if (status < 0) {
		return status;
	}

	*data = dr;

	return STATUS_SUCCESS;
}

NTSTATUS static dataRunRead(_In_ PNTFS_DRIVER drv, _In_ PNTFS_DATA_RUNS runs, _Out_ PVOID* data) {
	PVOID alloc = VirtualAlloc(0, runs->total_length * (UINT64)drv->bytes_per_cluster, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	UINT8* dt = alloc;
	if (dt == 0) {
		errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
		return STATUS_INTERNAL_ERROR;
	}
	const UINT64 bpc = drv->bytes_per_cluster;
	for (UINT32 i = 0; i < runs->n; i++) {
		const UINT64 cl = runs->runs[i].length * bpc;
		if ((runs->runs[i].flags & NTFS_DATA_RUN_FLAG_SPARSE) != 0) {
			// do nothing
		}
		else if ((runs->runs[i].flags & NTFS_DATA_RUN_FLAG_COMPRESSED) != 0) {
			// decompress, well fuck...
		}
		else {
			if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, runs->runs[i].offset * bpc, cl, dt)) {
				VirtualFree(dt, 0, MEM_RELEASE);
				return STATUS_INTERNAL_ERROR;
			}
		}
		dt += cl;
	}
	*data = alloc;
	return STATUS_SUCCESS;
}

NTSTATUS static NTFSGetDirEntries(_In_ PNTFS_DRIVER drv, _In_ PNTFS_FILE_RECORD file, _Out_ PNTFS_INDEX_VALUE* vals, _Out_ PULONG64 length) {
	NTSTATUS status;
	PNTFS_STD_ATTRIB_HEADER attr = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
	PNTFS_INDEX_ROOT root = 0; // TODO: for multiple INDEX_ALLOCATION attributes and for $Bitmap attribute
	UINT8* dr = 0;

	while (attr->id != NTFS_END_MARKER) {
		if (attr->id == NTFS_INDEX_ROOT_ID) {
			if ((attr->non_resident_flag != 0) || (root != 0)) {
				return STATUS_DISK_CORRUPT_ERROR;
			}
			root = NTFS_RESIDENT_ATTR_DATA(attr);
		}
		else if (attr->id == NTFS_INDEX_ALLOCATION_ID) {
			if ((attr->non_resident_flag != 1) || (dr != 0)) {
				return STATUS_DISK_CORRUPT_ERROR;
			}
			dr = NTFS_NON_RESIDENT_DATA_RUNS(attr);
		}
		else if (attr->id == NTFS_BITMAP_ID) {

		}
		attr = NTFS_NEXT_ATTRIB(attr);
	}

	if (root == 0) {
		return STATUS_DISK_CORRUPT_ERROR;
	}
	if (dr == 0) {
		*vals = (PNTFS_INDEX_VALUE)(((size_t)root) + (size_t)root->node.values_offset + sizeof(NTFS_INDEX_ROOT_HEADER));
		return STATUS_SUCCESS;
	}

	PNTFS_DATA_RUNS data_runs;
	status = parseDataRuns(dr, &data_runs);
	if (status < 0) {
		memset(data_runs, 0, (size_t)(data_runs->n - ANYSIZE_ARRAY) * sizeof(NTFS_DATA_RUN) + sizeof(NTFS_DATA_RUNS));
		HeapFree(proc_heap, 0, data_runs);
		return status;
	}

	PNTFS_INDEX_ENTRY entries = 0;
	status = dataRunRead(drv, data_runs, &entries);
	if (status < 0) {
		memset(data_runs, 0, (size_t)(data_runs->n - ANYSIZE_ARRAY) * sizeof(NTFS_DATA_RUN) + sizeof(NTFS_DATA_RUNS));
		HeapFree(proc_heap, 0, data_runs);
		return status;
	}

	*vals = (PNTFS_INDEX_VALUE)(((size_t)entries) + (size_t)entries->node.values_offset + sizeof(NTFS_INDEX_ENTRY_HEADER));
	*length = data_runs->total_length;
	memset(data_runs, 0, (size_t)(data_runs->n - ANYSIZE_ARRAY) * sizeof(NTFS_DATA_RUN) + sizeof(NTFS_DATA_RUNS));
	HeapFree(proc_heap, 0, data_runs);
	return STATUS_SUCCESS;

}

NTSTATUS static NTFSGetFile(_In_ PNTFS_DRIVER drv, _In_ LPCWSTR* tokens, _In_ DWORD n_toks, _In_opt_ HANDLE currentPath, _Out_ PNTFS_FILE_REF mft_index, PNTFS_FILE_RECORD* file_rec) {
	PNTFS_FILE_RECORD file = 0;
	NTFS_FILE_REF ref = { 0 };
	NTSTATUS status;
	if (**tokens == 0) {
		ref.mft_index = 0x5;
		ref.sequence_num = 0x5;
		status = readMFTEntry(drv, 5, &file); // at 0x14d5b400 on my testdisk
		if (status < 0) {
			return status;
		}
	}
	else {
		if ((size_t)currentPath >= MAX_NTFS_HANDLES) {
			return STATUS_INVALID_HANDLE;
		}
		PNTFS_HANDLE hdl = &(drv->handle_table[(size_t)currentPath]);
		if ((hdl->flags & NTFS_HANDLE_OPEN) == 0) {
			return STATUS_INVALID_HANDLE;
		}
		ref = hdl->ref;
		file = VirtualAlloc(0, drv->bytes_per_mft, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (file == 0) {
			errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
			return STATUS_INTERNAL_ERROR;
		}
		memcpy(file, hdl->file_record, drv->bytes_per_mft);
	}

	PNTFS_INDEX_VALUE val;
	ULONG64 len;
	status = NTFSGetDirEntries(drv, file, &val, &len);
	if (status < 0) {
		VirtualFree(file, 0, MEM_RELEASE);
		return status;
	}
	DWORD i = 0;
	while (i < n_toks) {
		size_t toklen = wcslen(tokens[i]);
		if ((toklen == 0) && (i == 0)) {
			++i;
			continue;
		}
		if (toklen == 2) {
			if (wcsncmp(L"..", tokens[i], 2) == 0) {
				PNTFS_STD_ATTRIB_HEADER attrib = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
				while (attrib->id != NTFS_END_MARKER) {
					if (attrib->id == NTFS_FILE_NAME_ID) {
						break;
					}
					attrib = NTFS_NEXT_ATTRIB(attrib);
				}
				if (attrib->id == NTFS_END_MARKER) {
					status = STATUS_OBJECT_PATH_INVALID;
					goto _ntfs_get_file_exit;
				}
				else {
					PNTFS_FILE_NAME name = NTFS_RESIDENT_ATTR_DATA(attrib);
					ref = name->parent_dir_file_ref;
					goto _next_file;
				}
			}
		}
		BOOL found = FALSE;
		do {
			PNTFS_FILE_NAME name = (PNTFS_FILE_NAME)(((size_t)val) + sizeof(NTFS_INDEX_VALUE));
			dbgPrintf("name:%.*ws\n\r", name->n_chars, (PWCHAR)(((size_t)name) + sizeof(NTFS_FILE_NAME)));
			if (name->n_chars == toklen) {
				if (wcsncmp((PCWCHAR)(((size_t)name) + sizeof(NTFS_FILE_NAME)), tokens[i], name->n_chars) == 0) {
					ref = val->file_ref;
					goto _next_file;
				}
			}
			val = NTFS_INDEX_VALUE_NEXT(val);
		} while ((val->value_flags & NTFS_INDEX_VALUE_FLAGS_IS_LAST) == 0);
		status = STATUS_OBJECT_PATH_NOT_FOUND;
		goto _ntfs_get_file_exit;
	_next_file:
		VirtualFree(file, 0, MEM_RELEASE);
		file = 0;
		status = readMFTEntry(drv, ref.mft_index, &file);
		if (status < 0) {
			goto _ntfs_get_file_exit_nofile;
		}
		if (i < (n_toks - 1)) {
			if ((file->flags & NTFS_FILE_RECORD_FLAG_IS_DIR) == 0) {
				status = STATUS_OBJECT_PATH_INVALID;
				goto _ntfs_get_file_exit;
			}
			else {
				status = NTFSGetDirEntries(drv, file, &val, &len);
				if (status < 0) {
					VirtualFree(file, 0, MEM_RELEASE);
					return status;
				}
			}
		}
		++i;
	}
	status = STATUS_SUCCESS;
	*mft_index = ref;
	*file_rec = file;
_ntfs_get_file_exit_nofile:
	VirtualFree(val, 0, MEM_RELEASE);
	return status;
_ntfs_get_file_exit:
	VirtualFree(file, 0, MEM_RELEASE);
	VirtualFree(val, 0, MEM_RELEASE);
	return status;
}

NTSTATUS NTFSOpen(_In_ PNTFS_DRIVER drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle) {
	if (!(IS_VALID_DRV(drv) && (path != 0) && (handle != 0))) {
		return STATUS_INVALID_PARAMETER;
	}

	size_t i = allocHandle(drv);
	if (i == (~(size_t)0)) {
		return STATUS_INTERNAL_ERROR;
	}
	PNTFS_HANDLE hdl = &(drv->handle_table[i]);
	hdl->flags |= NTFS_HANDLE_OPEN;

	LPWSTR wp;
	LPWSTR* tokens;
	DWORD n_toks;
	if (!parsePath(path, &wp, &tokens, &n_toks)) {
		hdl->flags = 0;
		return STATUS_INVALID_PARAMETER;
	}
	NTFS_FILE_REF ref;
	PNTFS_FILE_RECORD rec;
	NTSTATUS status = NTFSGetFile(drv, tokens, n_toks, current_path, &ref, &rec);
	if (status >= 0) {
		hdl->file_record = rec;
		hdl->ref = ref;
		*handle = (HANDLE)i;
	}
	else {
		hdl->flags = 0;
	}
	return status;
}

NTSTATUS NTFSClose(_In_ PNTFS_DRIVER drv, _In_ HANDLE handle) {
	if (IS_VALID_DRV(drv)) {
		if ((size_t)handle < MAX_NTFS_HANDLES) {
			PNTFS_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
			if ((hdl->flags & NTFS_HANDLE_OPEN) != 0) {
				hdl->flags = 0;
				VirtualFree(hdl->file_record, 0, MEM_RELEASE);
				hdl->file_record = 0;
				hdl->ref.mft_index = 0;
				hdl->ref.sequence_num = 0;
				return STATUS_SUCCESS;
			}
			else {
				return STATUS_INVALID_HANDLE;
			}
		}
		else {
			return STATUS_INVALID_HANDLE;
		}
	}
	else {
		return STATUS_INVALID_PARAMETER;
	}
}

NTSTATUS NTFSCreate(_In_ PNTFS_DRIVER drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle, _In_opt_ UINT32 flags) {
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTFSDelete(_In_ PNTFS_DRIVER drv, _In_ HANDLE file) {
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTFSReadFile(_In_ PNTFS_DRIVER drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTFSWriteFile(_In_ PNTFS_DRIVER drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer) {
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTFSGetInfo(_In_ PNTFS_DRIVER drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_opt_(data_size) PVOID query_data, _In_opt_ DWORD data_size) {
	if ((!IS_VALID_DRV(drv)) || (info >= FSInfoMax) || (query_data == 0 && data_size != 0)) {
		return STATUS_INVALID_PARAMETER;
	}
	if ((size_t)handle >= MAX_NTFS_HANDLES) {
		return STATUS_INVALID_HANDLE;
	}
	PNTFS_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
	if ((hdl->flags & NTFS_HANDLE_OPEN) == 0) {
		return STATUS_INVALID_HANDLE;
	}
	PNTFS_FILE_RECORD file = hdl->file_record;
	if (info == FSName) {
		if (data_size < sizeof(FS_NAME_INFO)) {
			return STATUS_INVALID_PARAMETER;
		}
		PFS_NAME_INFO name = query_data;
		name->name_8_3_length = 0;
		PNTFS_STD_ATTRIB_HEADER attr = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
		while (attr->id != NTFS_END_MARKER) {
			if (attr->id == NTFS_FILE_NAME_ID) {
				PNTFS_FILE_NAME fs_name = NTFS_RESIDENT_ATTR_DATA(attr);

				UINT32 l = fs_name->n_chars;
				if (name->long_name_max_length <= l) {
					return STATUS_BUFFER_TOO_SMALL;
				}
				memcpy(name->long_name, (PVOID)(((size_t)fs_name) + sizeof(NTFS_FILE_NAME)), (size_t)l << 1);
				name->long_name[l] = 0;
				name->long_name_length = l;

				return STATUS_SUCCESS;
			}
			attr = NTFS_NEXT_ATTRIB(attr);
		}
		return STATUS_NOT_FOUND;
	}
	else if (info == FSSize) {
		if (data_size < sizeof(FS_FILESIZE_INFO)) {
			return STATUS_INVALID_PARAMETER;
		}
		PFS_FILESIZE_INFO size = query_data;
		PNTFS_STD_ATTRIB_HEADER attr = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
		while (attr->id != NTFS_END_MARKER) {
			if (attr->id == NTFS_FILE_NAME_ID) {
				PNTFS_FILE_NAME fs_name = NTFS_RESIDENT_ATTR_DATA(attr);

				size->size = fs_name->file_size;
				size->size_on_disk = fs_name->file_alloc_size;

				return STATUS_SUCCESS;
			}
			attr = NTFS_NEXT_ATTRIB(attr);
		}
		return STATUS_NOT_FOUND;
	}
	else if (info == FSAttribute) {

		if (data_size < sizeof(FS_ATTRIBUTE_INFO)) {
			return STATUS_INVALID_PARAMETER;
		}

		UINT32 attribs = 0;
		if ((file->flags & NTFS_FILE_RECORD_FLAG_IS_DIR) != 0) {
			attribs |= FS_ATTRIBUTE_DIR;
		}

		PFS_ATTRIBUTE_INFO out_attr = query_data;
		PNTFS_STD_ATTRIB_HEADER attr = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
		while (attr->id != NTFS_END_MARKER) {
			if (attr->id == NTFS_FILE_NAME_ID) {
				PNTFS_FILE_NAME fs_name = NTFS_RESIDENT_ATTR_DATA(attr);

				UINT32 flags = fs_name->flags;

				attribs |= flags & 0x7;

				if ((flags & NTFS_FILE_FLAG_ARCHIVE) != 0) {
					attribs |= FS_ATTRIBUTE_ARCHIVE;
				}

				*out_attr = attribs;

				return STATUS_SUCCESS;
			}
			attr = NTFS_NEXT_ATTRIB(attr);
		}
		return STATUS_NOT_FOUND;
	}
	else if (info == FSDateInfo) {
		if (data_size < sizeof(FS_DATE_INFO)) {
			return STATUS_INVALID_PARAMETER;
		}
		PFS_DATE_INFO dates = query_data;
		PNTFS_STD_ATTRIB_HEADER attr = (PNTFS_STD_ATTRIB_HEADER)(((size_t)file) + file->attribs_offset);
		while (attr->id != NTFS_END_MARKER) {
			if (attr->id == NTFS_STANDARD_INFORMATION_ID) {
				PNTFS_STD_ATTRIBUTE std = NTFS_RESIDENT_ATTR_DATA(attr);

				dates->creation_u64      = std->creation_time;
				dates->last_modified_u64 = std->altered_time;
				dates->last_accessed_u64 = std->read_time;


				return STATUS_SUCCESS;
			}
			attr = NTFS_NEXT_ATTRIB(attr);
		}
		return STATUS_NOT_FOUND;
	}
	else if (info == FSGetPath) {

	}
	else if (info == FSGetFirst) {

	}
	else if (info == FSGetNext) {

	}
	else if (info == FSGetPhys) {

	}
	else if (info == FSCustomAction) {

	}
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTFSSetInfo(_In_ PNTFS_DRIVER drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
	return STATUS_NOT_IMPLEMENTED;
}