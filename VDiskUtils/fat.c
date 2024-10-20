#include "vdisk.h"
#include "console.h"
#include "fat_internal.h"

void static __inline deletePath(_In_ LPWSTR path);

static BSTATUS computeFAT32FreeClusters(_In_ PFAT_DRIVER_IMPL drv) {

}

void static fatDriverDelete(PFAT_DRIVER_IMPL driver) {
    if (driver == 0)return;
    for (DWORD i = 0; i < FAT_MAX_HANDLES; i++) {
        PFAT_HANDLE hdl = &(driver->handle_table[i]);
        if ((hdl->flags & HANDLE_FLAG_OPEN)) {
            if ((hdl->flags & HANDLE_FLAG_QUERY) != 0) {
                VirtualFree(hdl->query->dir_cache, 0, MEM_RELEASE);
                memset(hdl->query, 0, ((size_t)hdl->query->recursion_limit + 3) << 2);
                HeapFree(proc_heap, 0, hdl->query);
            }
            else {
                deletePath(hdl->path);
            }
        }
    }
    VirtualFree(driver->handle_table, 0, MEM_RELEASE);
    memset(driver, 0, sizeof(FAT_DRIVER_IMPL));
    HeapFree(proc_heap, 0, driver);
}

BSTATUS createFATDriver(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _Out_ PFS_DRIVER* driver) {
	void* buffer = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (buffer == 0) {
		return FALSE;
	}
    PFAT_DRIVER_IMPL drv = 0;
	if (partitionRead(vdisk, partition_index, 0, 0x200, buffer)) {
		PFAT_BS bs = (PFAT_BS)buffer;
		PFAT_BPB bpb = (PFAT_BPB)bs;
		if (!isValidJMP(bpb)) { 
			dbgPrintf("FAT: Invalid jmp instructions\n\r");
			goto fat_is_p_exit;
		}
        if (!isValidBPS(bpb)) {
            dbgPrintf("FAT: Invalid bytes per sector\n\r");
            goto fat_is_p_exit;
        }
        if (!isValidSPC(bpb)) {
            dbgPrintf("FAT: Invalid sectors per cluster\n\r");
            goto fat_is_p_exit;
        }
        if (!isValidNRES(bpb)) {
            dbgPrintf("FAT: Invalid number of reserved sectors\n\r");
            goto fat_is_p_exit;
        }
        if (!isValidTOTSECS(bpb)) {
            dbgPrintf("FAT: Invalid number of total sectors\n\r");
            goto fat_is_p_exit;
        }
		if (bs->fat12.boot_signature != BOOT_SIGNATURE) {
            dbgPrintf("FAT: Invalid Boot Signature!\n\r");
		}
        size_t rtDirS = (((size_t)bpb->n_root_entries * 32) + ((size_t)bpb->bytes_per_sector - 1)) / (size_t)bpb->bytes_per_sector;
        size_t fatsz = 0;
        if (bpb->sectors_per_fat != 0) {
            fatsz = bpb->sectors_per_fat;
        }
        else {
            fatsz = ((PFAT32)bpb)->sectors_per_fat;
        }
        size_t ts = 0;
        if (bpb->n_sectors != 0) {
            ts = bpb->n_sectors;
        }
        else {
            ts = bpb->n_large_sectors;
        }
        size_t dataSec = ts - (bpb->n_reserved_sectors + (bpb->n_fats * fatsz) + rtDirS);
        size_t coc = dataSec / (size_t)bpb->sectors_per_cluster;
        drv = HeapAlloc(proc_heap, 0, sizeof(FAT_DRIVER_IMPL));
        if (drv == 0) {
            errPrintf("HeapAlloc failed:%x!\n\r", GetLastError());
            goto fat_is_p_exit;
        }
        const size_t bps = (size_t)(bpb->bytes_per_sector);
        drv->params.bytes_per_sector = bpb->bytes_per_sector;
        drv->params.sectors_per_cluster = bpb->sectors_per_cluster;
        drv->params.n_fats = bpb->n_fats;
        drv->params.n_res_sectors = bpb->n_reserved_sectors;
        drv->params.n_sectors = bpb->n_sectors == 0 ? bpb->n_large_sectors : bpb->n_sectors;
        drv->params.bytes_per_cluster = bps * (size_t)(bpb->sectors_per_cluster);
        if (coc < 65525) {
            drv->params.sectors_per_fat = bpb->sectors_per_fat;
            drv->params.bytes_per_fat = (size_t)(bpb->sectors_per_fat) * bps;
            drv->params.root_offset = drv->params.bytes_per_fat * bpb->n_fats + ((size_t)(drv->params.n_res_sectors) * bps);
            drv->params.root_size = ((size_t)(bpb->n_root_entries)) << 5;
            drv->params.root_cluster = 0; // unused for fat12/fat16
            drv->params.data_offset = drv->params.root_offset + drv->params.root_size;
            drv->params.data_size = (((size_t)(drv->params.n_sectors) - (size_t)(drv->params.n_res_sectors) - ((size_t)(drv->params.n_fats) * (size_t)(drv->params.sectors_per_fat))) * bps) - drv->params.root_size;
            drv->start_looking = 2;
            drv->n_free_clus = ~0;
            if (coc < 4085) {
                //FAT12
                drv->params.clusters_per_fat = (UINT32)((drv->params.bytes_per_fat * 3) >> 1); // 1.5 bytes per entry
                drv->flags = FAT_FLAG_FAT12;
                drv->interf.fs_type = FS_DRIVER_TYPE_FAT12;
                drv->interf.open = fat12Open;
                drv->interf.close = fat12Close;
                drv->interf.create = fat12Create;
                drv->interf.del = fat12Delete;
                drv->interf.read = fat12Read;
                drv->interf.write = fat12Write;
                drv->interf.get_info = fat12GetInfo;
                drv->interf.set_info = fat12SetInfo;
            }
            else {
                //FAT16
                drv->params.clusters_per_fat = (UINT32)(drv->params.bytes_per_fat / sizeof(UINT16));
                drv->flags = FAT_FLAG_FAT16;
                drv->interf.fs_type = FS_DRIVER_TYPE_FAT16;
                drv->interf.open = fat16Open;
                drv->interf.close = fat16Close;
                drv->interf.create = fat16Create;
                drv->interf.del = fat16Delete;
                drv->interf.read = fat16Read;
                drv->interf.write = fat16Write;
                drv->interf.get_info = fat16GetInfo;
                drv->interf.set_info = fat16SetInfo;
            }
        }
        else {
            //FAT32
            drv->params.sectors_per_fat = bs->fat32.sectors_per_fat;
            drv->params.bytes_per_fat = bps * (size_t)(bs->fat32.sectors_per_fat);
            drv->params.clusters_per_fat = (UINT32)(drv->params.bytes_per_fat / sizeof(UINT32));
            drv->params.root_cluster = bs->fat32.root_sector;
            drv->params.root_offset = ((size_t)(drv->params.n_res_sectors) * bps) + ((size_t)(drv->params.n_fats) * (drv->params.bytes_per_fat)) + ((size_t)(drv->params.root_cluster - 2) * drv->params.bytes_per_cluster);
            drv->params.root_size = 0; // unused for fat32
            drv->params.data_offset = drv->params.bytes_per_fat * bpb->n_fats + ((size_t)(drv->params.n_res_sectors) * bps);
            drv->params.data_size = ((size_t)(drv->params.n_sectors) - (size_t)(drv->params.n_res_sectors) - (size_t)(drv->params.n_fats) * (size_t)(drv->params.sectors_per_fat)) * bps;
            drv->start_looking = 2; // TODO: get start_looking from fs_info
            drv->n_free_clus = ~0;  // TODO: get free_clusters from fs_info
            drv->flags = FAT_FLAG_FAT32;
            drv->interf.fs_type = FS_DRIVER_TYPE_FAT32;
            drv->interf.open = fat32Open;
            drv->interf.close = fat32Close;
            drv->interf.create = fat32Create;
            drv->interf.del = fat32Delete;
            drv->interf.read = fat32Read;
            drv->interf.write = fat32Write;
            drv->interf.get_info = fat32GetInfo;
            drv->interf.set_info = fat32SetInfo;
        }

        drv->handle_table = VirtualAlloc(0, FAT_MAX_HANDLES * sizeof(FAT_HANDLE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (drv->handle_table == 0) {
            goto fat_is_p_exit;
        }
        drv->interf.exit = fatDriverDelete;
        drv->interf.partition_index = partition_index;
        drv->interf.vdisk = vdisk;
        drv->flags |= FAT_FLAG_VALID;
        *driver = (PFS_DRIVER)drv;
        return TRUE;
	}
	else {
		errPrintf("Failed to read Bootsector!\n\r");
		goto fat_is_p_exit;
	}
fat_is_p_exit:
	VirtualFree(buffer, 0, MEM_RELEASE);
    if (drv) {
        memset(drv, 0, sizeof(FAT_DRIVER_IMPL));
        HeapFree(proc_heap, 0, drv);
    }
	return FALSE;
}

// UTILITY

DWORD static __inline allocateHandle(_In_ PFAT_DRIVER_IMPL drv) {
    for (DWORD i = 0; i < FAT_MAX_HANDLES; i++) {
        if (!(drv->handle_table[i].flags & HANDLE_FLAG_OPEN)) {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

LPWSTR static __inline storePath(_In_ const LPCWSTR str) {
    size_t len = (wcslen(str) + 1) << 1;
    LPWSTR out = HeapAlloc(proc_heap, 0, len);
    if (out == 0) {
        errPrintf("HeapAlloc failed.%x\n\r", GetLastError());
        return 0;
    }
    memcpy(out, str, len);
    return out;
}

void static __inline deletePath(_In_ LPWSTR path) {
    size_t len = wcslen(path);
    memset(path, 0, len);
    HeapFree(proc_heap, 0, path);
}

// All Functions are only used internally and are parameter checked by the calling functions
#pragma warning(disable : 6054)
#pragma warning(disable : 6386)

BSTATUS static parsePath(_In_ LPCWSTR usr_path, _Out_ LPWSTR* path, _Out_ LPWSTR** tokens, _Out_ LPDWORD n_tokens) {
    LPWSTR pth = storePath(usr_path);
    if (pth == 0) {
        return FALSE;
    }
    LPWSTR token_buf = storePath(usr_path);
    size_t buflen = wcslen(token_buf) << 1;
    if (token_buf == 0) {
        deletePath(pth);
        return FALSE;
    }
    LPWSTR pt = token_buf;
    DWORD n_tok = 0;
    while (*pt != 0) {
        if (*pt == L'/') {
            ++n_tok;
        }
        ++pt;
    }
    if (*pt != '/') {
        ++n_tok;
    }
    size_t tokl = ((size_t)n_tok) << 3;
    LPWSTR* toks = HeapAlloc(proc_heap, 0, tokl);
    if (toks == 0) {
        deletePath(pth);
        deletePath(token_buf);
        return FALSE;
    }
    pt = token_buf;
    LPWSTR lt = token_buf;
    n_tok = 0;
    while (*pt != 0) {
        if (*pt == L'/') {
            *pt = 0;
            toks[n_tok] = lt;
            ++pt;
            ++n_tok;
            lt = pt;
        }
        else {
            ++pt;
        }
    }
    if (*lt != 0) {
        toks[n_tok] = lt;
        n_tok++;
    }
    *path = pth;
    *tokens = toks;
    *n_tokens = n_tok;
    return TRUE;
}

#pragma warning(disable : 6101) // following function too complex for intellisense to get it right
// 8.3 Filename Generation:
/*
* Convert to uppercase (not always)
* Convert to OEM: if(not valid character) {
*   replace with '_'
*   set lossy conversion
* }
* strip leading and embedded spaces
* strip all leading periods
* copy basis name to buffer
* find last period in long name
* if(last period found): {
*   copy extension to buffer
* }
*
* Params:
*   name: the long name
*   b_name: buffer for the base name
*   b_extend: buffer for the extension
*   is_83: if the name is a valid 8.3-Name
*   lossy: if the conversion was flagged lossy and a numeric tail should be added
* returns: TRUE if the name can be converted to a 8.3-Name
*/
BSTATUS static make8_3Name(_In_ LPCWSTR name, _Out_ char* b_name, _Out_ char* b_extend, _Out_ PBOOL is_83, _Out_ PBOOL lossy) {
    *is_83 = TRUE;
    *lossy = FALSE;
    // find last period
    size_t l = 0;
    LPCWSTR tmp = name;
    size_t len = 0;
    while (*tmp != 0) {
        if (*tmp == L'.') l = len;
        ++len;
        ++tmp;
    }
    if (len == 0) {
        return FALSE;
    }
    uint8_t cs = 0; // case of all the letters (valid 8.3 names are either all upper or all lowercase)
    // make basename
    size_t i = 0; // string index
    size_t ci = 0; // copy index
    while(ci < 8) {
        wchar_t c = name[i];
        if (c == 0) {
            if (ci == 0) { // all spaces
                *is_83 = FALSE;
                return FALSE;
            }
            goto _mk83_pad_all; // reached end
        }
        else if (c == L'.') {
            if (ci == 0) { // all spaces, . ist the first char which is invalid
                *is_83 = FALSE;
                return FALSE;
            }
            else if (i == l) {
                for (size_t j = ci; j < 8; j++) {
                    b_name[j] = ' ';
                }
                break;
            }
            *is_83 = FALSE;
            // strip periods -> no copy index increase
        }
        else if (c == L' ') {
            *is_83 = FALSE;
            // strip spaces -> no copy index increase
        }
        else if (INVALID_FAT_CHAR(c)) {
            b_name[ci] = '_'; // replace invalid char with '_'
            *is_83 = FALSE;
            *lossy = TRUE; // set lossy conversion
            ++ci;
        }
        else {
            // convert to uppercase
            if ((c > 96) && (c < 123)) {
                c -= 32;
                if (cs == 2) *is_83 = FALSE;
                cs = 1;
            }
            else if ((c > 223) && (c < 255) && (c != 247)) {
                c -= 32;
                if (cs == 1) *is_83 = FALSE;
                cs = 2;
            }
            b_name[ci] = (unsigned char)(c);
            ++ci;
        }
        ++i;
    }

    if (l == 0) { // no point found -> no extension present
        goto _mk83_pad_ext;
    }
    if (l > i) {
        *lossy = TRUE;
        *is_83 = FALSE;
    }
    i = 0; // new copy index
    ++l; // new string index
    while (i != 3) {
        wchar_t c = name[l];
        if (c == 0) {
            b_extend[i] = ' '; // fill for each missing if end of string reached
            ++i;
        }
        else if (c == L' ') {
            *is_83 = FALSE;
            *lossy = TRUE; // set lossy conversion
            // skip spaces
            ++l;
        }
        else if (INVALID_FAT_CHAR(c)) {
            b_extend[i] = L'_';
            *is_83 = FALSE;
            *lossy = TRUE; // set lossy conversion
            ++i;
            ++l;
        }
        else {
            // convert to uppercase
            if ((c > 96) && (c < 123)) {
                c -= 32;
                if (cs == 2) *is_83 = FALSE;
                cs = 1;
            }
            else if ((c > 223) && (c < 255) && (c != 247)) {
                c -= 32;
                if (cs == 1) *is_83 = FALSE;
                cs = 2;
            }
            b_extend[i] = (unsigned char)(c);
            ++i;
            ++l;
        }
    }
    // there are more than 3 possible characters in the extension
    if (l < len) {
        *lossy = TRUE;
        *is_83 = FALSE;
    }
    return TRUE;

_mk83_pad_all:
    // pad base name
    for (size_t j = ci; j < 8; j++) {
        b_name[j] = ' ';
    }
_mk83_pad_ext:
    // pas extension
    b_extend[0] = ' ';
    b_extend[1] = ' ';
    b_extend[2] = ' ';
    return TRUE;
#pragma warning(default : 6101)
}

BSTATUS static copyLFNEnt(_In_ PFAT_DIR_LONG ldir, _Out_ PWSTR chbuf) {
    uint32_t seq = ldir->seq;
    uint32_t seqn = (uint32_t)(seq & SEQ_NUM_MASK);
    --seqn;
    if (seqn > 19) {
        return FALSE;
    }
    if ((ldir->first_chars[0]) == 0) {
        return FALSE;
    }
    uint32_t off = (uint32_t)seqn * 13;
    for (uint32_t k = 0; k < 5; k++, off++) {
        if ((ldir->first_chars[k]) == 0) {
            return (seq & SEQ_FLAG_FIRST_LAST) != 0;
        }
        chbuf[off] = ldir->first_chars[k];
    }
    for (uint32_t k = 0; k < 6; k++, off++) {
        if (off >= 256) {
            return FALSE;
        }
        else if ((ldir->next_chars[k]) == 0) {
            return (seq & SEQ_FLAG_FIRST_LAST) != 0;
        }
        chbuf[off] = ldir->next_chars[k];
    }
    for (uint32_t k = 0; k < 2; k++, off++) {
        if (off >= 256) {
            return FALSE;
        }
        else if ((ldir->final_chars[k]) == 0) {
            return (seq & SEQ_FLAG_FIRST_LAST) != 0;
        }
        chbuf[off] = ldir->final_chars[k];
    }
    return TRUE;
}

static BOOL _8_3_NameCmp(_In_ PFAT_DIR83 ent, _In_ char* name, _In_ char* extension) {
    for (int i = 0; i < 8; i++) {
        if (ent->filename[i] != name[i])return FALSE;
    }
    return (ent->extension[0] == extension[0]) && (ent->extension[1] == extension[1]) && (ent->extension[2] == extension[2]);
}

#pragma warning(disable : 6101)
NTSTATUS static getLongName(_In_ PFAT_DIR_LONG ents, _In_opt_ size_t index, _Out_ PWSTR buf) {
    if (index == 0) {
        return STATUS_NOT_FOUND;
    }
    uint32_t cache = 0;
    do {
        --index;
        PFAT_DIR_LONG entry = &(ents[index]);
        if (entry->seq == 0) {
            return STATUS_DISK_CORRUPT_ERROR;
        }
        else if(entry->seq == 0xE5) {
            continue;
        }
        else if (entry->attrib != 0x0F) {
            break;
        }
        else {
            if (!copyLFNEnt(entry, buf)) {
                return STATUS_DISK_CORRUPT_ERROR;
            }
            uint32_t msk = 1 << ((entry->seq - 1) & SEQ_NUM_MASK);
            if ((cache & msk) != 0) {
                return STATUS_DISK_CORRUPT_ERROR;
            }
            cache |= msk;
        }
    } while (index != 0);

    if ((cache & (cache + 1)) != 0) {
        return STATUS_DISK_CORRUPT_ERROR;
    }

    return STATUS_SUCCESS;
}
#pragma warning(default: 6101)

// All Functions are only used internally and are parameter checked by the calling functions
#pragma warning(default : 6386)
#pragma warning(default : 6054)

// FAT12

NTSTATUS fat12Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_dir, _Out_ PHANDLE handle) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle) {
    if ((drv == 0) || (handle == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12) || ((size_t)handle >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    PFAT_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
    if ((hdl->flags & HANDLE_FLAG_OPEN)) {
        if ((hdl->flags & HANDLE_FLAG_QUERY) != 0) {
            VirtualFree(hdl->query->dir_cache, 0, MEM_RELEASE);
            memset(hdl->query, 0, ((size_t)hdl->query->recursion_limit + 3) << 2);
            HeapFree(proc_heap, 0, hdl->query);
        }
        else {
            deletePath(hdl->path);
        }
        memset(hdl, 0, sizeof(FAT_HANDLE));
        return 0;
    }
    return STATUS_INVALID_HANDLE;
}

NTSTATUS fat12Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_dir, _Out_ PHANDLE handle, _In_opt_ UINT32 flags) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat12SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT12)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

// FAT16

NTSTATUS fat16Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_dir, _Out_ PHANDLE handle) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle) {
    if ((drv == 0) || (handle == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16) || ((size_t)handle >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    PFAT_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
    if ((hdl->flags & HANDLE_FLAG_OPEN)) {
        if ((hdl->flags & HANDLE_FLAG_QUERY) != 0) {
            VirtualFree(hdl->query->dir_cache, 0, MEM_RELEASE);
            memset(hdl->query, 0, ((size_t)hdl->query->recursion_limit + 3) << 2);
            HeapFree(proc_heap, 0, hdl->query);
        }
        else {
            deletePath(hdl->path);
        }
        memset(hdl, 0, sizeof(FAT_HANDLE));
        return 0;
    }
    return STATUS_INVALID_HANDLE;
}

NTSTATUS fat16Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_dir, _Out_ PHANDLE handle, _In_opt_ UINT32 flags) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat16SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT16)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

// FAT32

BSTATUS static getFat32FileSize(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t cluster_index, _Out_ size_t* filesize) {
    size_t n_clus = ((size_t)(drv->params.sectors_per_fat) * (size_t)(drv->params.bytes_per_sector)) / sizeof(UINT32);
    if (cluster_index >= n_clus || cluster_index < 2) {
        return FALSE;
    }
    size_t fat_off = (drv->params.n_res_sectors) * (size_t)(drv->params.bytes_per_sector);
    size_t size = drv->params.bytes_per_cluster;
    size_t start_clus = cluster_index;
    UINT32* fat_ents = HeapAlloc(proc_heap, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    if (fat_ents == 0) {
        errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
        return FALSE;
    }
    UINT32* current = fat_ents;
    if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (cluster_index << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
        goto _ret_err;
    }
    while (1) {
        size_t cc = (*current) & 0x0FFFFFFF;
        if (cc < 2) {
            goto _ret_err;
        }
        if (cc == 0x0FFFFFF7) {
            goto _ret_err;
        }
        else if (cc > 0x0FFFFFF7) {
            break;
        }
        else if (cc >= n_clus) {
            goto _ret_err;
        }
        size += drv->params.bytes_per_cluster;
        if (size > 0x100000000) {
            goto _ret_err;
        }
        size_t dif = cc - start_clus;
        if (dif < FAT_CACHE_SIZE) {
            current = &(fat_ents[dif]);
        }
        else {
            if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (cc << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
                goto _ret_err;
            }
            start_clus = cc;
            current = fat_ents;
        }
    }
    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    *filesize = size;
    return TRUE;
_ret_err:
    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    return FALSE;
}

// Continuos Allocator
NTSTATUS static FAT32ContAlloc(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t n_clusters, _In_opt_ size_t link_from, _Out_ size_t* first_allocated_cluster) {
    size_t n_clus = ((size_t)(drv->params.sectors_per_fat) * (size_t)(drv->params.bytes_per_sector)) / sizeof(UINT32);
    if ((link_from != 0) && ((link_from >= n_clus) || (link_from < 2))) {
        return STATUS_INVALID_PARAMETER;
    }
    if (link_from >= drv->n_free_clus) {
        return STATUS_DISK_FULL;
    }

    const size_t fat_off = (drv->params.n_res_sectors) * (size_t)(drv->params.bytes_per_sector);
    size_t start_clus = drv->start_looking;
    UINT32* fat_ents = HeapAlloc(proc_heap, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    if (fat_ents == 0) {
        errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
        return STATUS_INTERNAL_ERROR;
    }
    NTSTATUS status = 0;
    UINT32* current = fat_ents;
    if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (start_clus << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
        status = STATUS_INTERNAL_ERROR;
        goto _not_found;
    }
    size_t free_c = 0;
    size_t cc = 0;
    size_t curclus = start_clus;
    size_t fstart = 0;
    BOOL isFirst = TRUE;
    BOOL ff = FALSE;
    while (free_c < n_clusters) {
        if (((*current) & 0x0FFFFFFF) == 0) {
            if (isFirst) {
                fstart = curclus;
                isFirst = FALSE;
            }
            ++free_c;
        }
        else {
            if (!isFirst) {
                ff = TRUE;
            }
            isFirst = TRUE;
        }
        ++curclus;

        if (cc == FAT_CACHE_SIZE) {
            if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (curclus << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
                status = STATUS_INTERNAL_ERROR;
                goto _not_found;
            }
            current = fat_ents;
            cc = 0;
        }
        else {
            ++cc;
            ++current;
        }

        if (curclus >= n_clus) {
            if (ff)
                status = STATUS_FREE_SPACE_TOO_FRAGMENTED;
            else
                status = STATUS_DISK_FULL;
            goto _not_found;
        }
    }

    size_t i = 0;
    size_t cnt = n_clusters / FAT_CACHE_SIZE;
    size_t rmn = n_clusters & (FAT_CACHE_SIZE - 1);
    curclus = fstart;
    while (i < cnt) {
        size_t tmpc = curclus << 2;
        for (UINT32 j = 0; j < FAT_CACHE_SIZE; j++) {
            ++curclus;
            fat_ents[j] = (UINT32)curclus;
        }
        
        for (UINT8 k = 0; k < drv->params.n_fats; k++) {
            if (!partitionWrite(drv->interf.vdisk, drv->interf.partition_index, fat_off + (k * drv->params.bytes_per_fat) + tmpc, FAT_CACHE_SIZE << 2, fat_ents)) {
                status = STATUS_INTERNAL_ERROR;
                goto _not_found;
            }
        }
        ++i;
    }
    size_t tmpc = curclus << 2;
    for (size_t j = 0; j < rmn; j++) {
        ++curclus;
        fat_ents[j] = (UINT32)curclus;
    }

    for (UINT8 k = 0; k < drv->params.n_fats; k++) {
        if (!partitionWrite(drv->interf.vdisk, drv->interf.partition_index, fat_off + (k * drv->params.bytes_per_fat) + tmpc, rmn << 2, fat_ents)) {
            status = STATUS_INTERNAL_ERROR;
            goto _not_found;
        }
    }

    if (link_from != 0) {
        for (UINT8 k = 0; k < drv->params.n_fats; k++) {
            if (!partitionWrite(drv->interf.vdisk, drv->interf.partition_index, fat_off + (k * drv->params.bytes_per_fat) + (link_from << 2), 4, fat_ents)) {
                status = STATUS_INTERNAL_ERROR;
                goto _not_found;
            }
        }
    }

    *first_allocated_cluster = fstart;

    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    return STATUS_SUCCESS;
_not_found:
    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    if (status >= 0) {
        status = -status;
    }
    return status;
}

// Fragmented Allocator
NTSTATUS static FAT32FragAlloc(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t n_clusters, _In_opt_ size_t link_from, _Out_ size_t* first_allocated_cluster) {

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS static FAT32Alloc(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t n_clusters, _In_opt_ size_t link_from, _Out_ size_t* first_cluster) {
    if (n_clusters == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    NTSTATUS status = FAT32ContAlloc(drv, n_clusters, link_from, first_cluster);
    if (status != 0) {
        status = FAT32FragAlloc(drv, n_clusters, link_from, first_cluster);
        if (status < 0) {
            // TODO: defrag
            return status;
        }
    }
    return status;
}

NTSTATUS static readFat32Data(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t cluster_start, _Out_ PVOID* data, _Out_ size_t* length) {
    size_t n_clus = ((size_t)(drv->params.sectors_per_fat) * (size_t)(drv->params.bytes_per_sector)) / sizeof(UINT32);
    if (cluster_start >= n_clus || cluster_start < 2) {
        return STATUS_INVALID_PARAMETER;
    }
    size_t sz = 0;
    if (!getFat32FileSize(drv, cluster_start, &sz)) {
        return STATUS_INTERNAL_ERROR;
    }
    void* dta = VirtualAlloc(0, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    void* dta_orig = dta;
    if (dta == 0) {
        errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
        return STATUS_INTERNAL_ERROR;
    }
    size_t fat_off = (drv->params.n_res_sectors) * (size_t)(drv->params.bytes_per_sector);
    size_t start_clus = cluster_start;
    UINT32* fat_ents = HeapAlloc(proc_heap, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    if (fat_ents == 0) {
        errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
        VirtualFree(dta, 0, MEM_RELEASE);
        return STATUS_INTERNAL_ERROR;
    }
    NTSTATUS status = STATUS_SUCCESS;
    UINT32* current = fat_ents;
    if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (cluster_start << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
        status = STATUS_INTERNAL_ERROR;
        goto _ret_err;
    }
    const size_t bpc = drv->params.bytes_per_cluster;

    size_t lc = cluster_start; // last cluster
    size_t fc = cluster_start; // first cluster in continuous block
    while (1) {
        size_t cc = (*current) & 0x0FFFFFFF; // current cluster
        if (cc == 0x0FFFFFF7) {
            status = STATUS_BAD_CLUSTERS;
            goto _ret_err; // encountered bad cluster
        }
        else if (cc > 0x0FFFFFF7) {
            // end of chain, read last continuous block
            size_t len = (fc - lc + 1) * bpc;
            if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, drv->params.data_offset + ((fc - 2) * bpc), len, dta)) {
                status = STATUS_INTERNAL_ERROR;
                goto _ret_err;
            }
            dta = (void*)(((size_t)dta) + len);
            break;
        }
        else if (cc >= n_clus || cc < 2) {
            status = STATUS_INDEX_OUT_OF_BOUNDS;
            goto _ret_err; // cluster out of range
        }
        else if (cc != (lc + 1)) {
            // read continuous block
            size_t len = (fc - lc + 1) * bpc;
            if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, drv->params.data_offset + ((fc - 2) * bpc), len, dta)) {
                status = STATUS_INTERNAL_ERROR;
                goto _ret_err;
            }
            dta = (void*)(((size_t)dta) + len);
            fc = cc;
        }
        lc = cc;

        size_t dif = cc - start_clus;
        if (dif < FAT_CACHE_SIZE) {
            // read from cache
            current = &(fat_ents[dif]);
        }
        else {
            // reached end of FAT-cache, read next block
            if (!partitionRead(drv->interf.vdisk, drv->interf.partition_index, fat_off + (cc << 2), FAT_CACHE_SIZE * sizeof(UINT32), fat_ents)) {
                status = STATUS_INTERNAL_ERROR;
                goto _ret_err;
            }
            start_clus = cc;
            current = fat_ents;
        }
    }
    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    *data = dta_orig;
    *length = sz;
    return 0;
_ret_err:
    memset(fat_ents, 0, FAT_CACHE_SIZE * sizeof(UINT32));
    HeapFree(proc_heap, 0, fat_ents);
    VirtualFree(dta_orig, 0, MEM_RELEASE);
    return status;
}

NTSTATUS static fat32UserRead(_In_ PFAT_DRIVER_IMPL drv, _In_ size_t cluster_start, _In_opt_ size_t offset, _In_ size_t length, _In_ PVOID data) {

    // TODO

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS static __inline fat32GetRootDir(_In_ PFAT_DRIVER_IMPL drv, _Out_ PFAT_DIR83* buffer, _Out_ LPDWORD n_ents) {
    PVOID bf;
    size_t len;
    NTSTATUS status = readFat32Data(drv, drv->params.root_cluster, &bf, &len);
    if (status >= 0) {
        *buffer = bf;
        *n_ents = (DWORD)(len >> 5);
    }
    return status;
}

NTSTATUS static fat32GetFile(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR* tokens, _In_ DWORD n_tokens, _In_opt_ HANDLE current_path, _Out_ LPDWORD out_index, _Out_ LPDWORD out_cluster, _Out_ LPDWORD out_parent_cluster, _Out_ LPDWORD out_size, _Out_ LPBOOL out_is_dir) {
    DWORD n_ents;
    PFAT_DIR83 dir = 0;
    wchar_t* chbuf = 0;
    BOOL is_dir = TRUE;
    DWORD index = 0xFFFFFFFF;
    DWORD cluster;
    DWORD size;
    DWORD parent_cluster = 0xFFFFFFFF;
    NTSTATUS status = STATUS_SUCCESS;
    if ((current_path == 0) || (current_path == INVALID_HANDLE_VALUE)) {
        status = fat32GetRootDir(drv, &dir, &n_ents);
        cluster = drv->params.root_cluster;
        size = n_ents << 5;
    }
    else {
        if ((size_t)current_path >= FAT_MAX_HANDLES) {
            status = STATUS_INVALID_HANDLE;
            goto fat32GetFile_err;
        }
        PFAT_HANDLE hdl = &(drv->handle_table[(size_t)current_path]);
        if (((hdl->flags & HANDLE_FLAG_OPEN) == 0) || ((hdl->flags & HANDLE_FLAG_DIR) == 0) || ((hdl->flags & HANDLE_FLAG_QUERY) != 0)) {
            status = STATUS_INVALID_HANDLE;
            goto fat32GetFile_err;
        }
        size_t len;
        status = readFat32Data(drv, hdl->cluster_index, &dir, &len);
        n_ents = (DWORD)(len >> 5);
        cluster = hdl->cluster_index;
        parent_cluster = hdl->parent_dir_start_cluster;
        size = n_ents << 5;
    }
    if (status < 0) {
        goto fat32GetFile_err;
    }
    if (n_tokens > 0) {
        chbuf = HeapAlloc(proc_heap, 0, 512);
        if (chbuf == 0) {
            errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
            status = STATUS_INTERNAL_ERROR;
            goto fat32GetFile_err;
        }

        for (DWORD i = 0; i < n_tokens; i++) {
            if (*(tokens[i]) == 0)break;
            LPWSTR buf_current = chbuf;
            size_t bi = 0;
            size_t maxlen = wcslen(tokens[i]) + 1;
            if (maxlen > 256) {
                status = STATUS_INVALID_PARAMETER;
                goto fat32GetFile_err;
            }
            char name[8];
            char ext[3];
            BOOL is8_3;
            BOOL lossy;
            if (wcscmp(tokens[i], L".") == 0) {
                continue;
            }
            else if (wcscmp(tokens[i], L"..") == 0) {
                is8_3 = TRUE;
                index = 1;
                goto _found_dir;
            }
            else if (!make8_3Name(tokens[i], name, ext, &is8_3, &lossy)) {
                status = -1;
                goto fat32GetFile_err;
            }
            index = 0xFFFFFFFF;
            BOOL hasLFN = FALSE;
            for (DWORD j = 0; j < n_ents; j++) {
                if (dir[j].filename[0] == 0)break;
                if (dir[j].filename[0] == (char)0xE5)continue;
                if (dir[j].attribute != FAT_ATTRIB_LFN) {
                    if (hasLFN) {
                        if (wcsncmp(chbuf, tokens[i], 256) == 0) {
                            index = j;
                            break;
                        }
                    }
                    else if (is8_3 && _8_3_NameCmp(&(dir[j]), name, ext)) {
                        index = j;
                        break;
                    }
                    hasLFN = FALSE;
                }
                else {
                    PFAT_DIR_LONG ldir = (PFAT_DIR_LONG)dir;
                    UINT8 seq = ldir[j].seq;
                    if ((seq & SEQ_FLAG_FIRST_LAST)) {
                        memset(chbuf, 0, 512);
                        hasLFN = TRUE;
                    }
                    else if (!hasLFN)continue;
                    if (!copyLFNEnt(&(ldir[j]), chbuf)) {
                        hasLFN = FALSE;
                    }
                }
            }
            if (index == 0xFFFFFFFF) {
                status = STATUS_NO_SUCH_FILE;
                goto fat32GetFile_err;
            }
            else {
            _found_dir:
                parent_cluster = cluster;
                is_dir = (dir[index].attribute & FAT_ATTRIB_DIRECTORY) != 0;
                if (i + 1 == n_tokens) {
                    cluster = ((UINT32)(dir[index].cluster_high) << 16) | (UINT32)(dir[index].cluster_low);
                    if (cluster == 0)
                        cluster = drv->params.root_cluster;
                    size = dir[index].filesize;
                    break;
                }
                else {
                    if (is_dir) {
                        cluster = ((UINT32)(dir[index].cluster_high) << 16) | (UINT32)(dir[index].cluster_low);
                        if (cluster == 0)
                            cluster = drv->params.root_cluster;
                        size_t len;
                        VirtualFree(dir, 0, MEM_RELEASE);
                        dir = 0;
                        status = readFat32Data(drv, cluster, &dir, &len);
                        if (status < 0) {
                            goto fat32GetFile_err;
                        }
                        n_ents = (DWORD)(len >> 5);
                    }
                    else {
                        status = STATUS_OBJECT_PATH_INVALID;
                        goto fat32GetFile_err;
                    }
                }
            }

        }
        memset(chbuf, 0, 512);
        HeapFree(proc_heap, 0, chbuf);
        chbuf = 0;
    }

    if (is_dir) {
        // search for '..' entry
        size_t len;
        PFAT_DIR83 dirs;
        status = readFat32Data(drv, cluster, &dirs, &len);
        if (status < 0) {
            goto fat32GetFile_err;
        }
        
        char name[8];
        char ext[3];
        name[0] = '.';
        name[1] = '.';
        name[2] = ' ';
        name[3] = ' ';
        name[4] = ' ';
        name[5] = ' ';
        name[6] = ' ';
        name[7] = ' ';
        ext[0] = ' ';
        ext[1] = ' ';
        ext[2] = ' ';

        if (_8_3_NameCmp(&(dirs[1]), name, ext)) {
            UINT32 clst = ((UINT32)(dirs[2].cluster_high) << 16) | (UINT32)(dirs[2].cluster_low);;
            if (clst != 0)
                *out_parent_cluster = clst;
            else
                *out_parent_cluster = drv->params.root_cluster;
        }
        else {
            // assume root dir
            *out_parent_cluster = 0xFFFFFFFF;
        }

        VirtualFree(dirs, 0, MEM_RELEASE);
    }
    else {
        *out_parent_cluster = parent_cluster;
    }
    *out_cluster = cluster;
    *out_index = index;
    *out_size = size;
    *out_is_dir = is_dir;

    return 0;

fat32GetFile_err:
    if(dir) VirtualFree(dir, 0, MEM_RELEASE);
    if (chbuf != 0) {
        memset(chbuf, 0, 512);
        HeapFree(proc_heap, 0, chbuf);
    }
    return status;
}

NTSTATUS fat32Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR usr_path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle) {
    if ((drv == 0) || (usr_path == 0) || (handle == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32)) {
        return STATUS_INVALID_PARAMETER;
    }
    NTSTATUS status;
    LPWSTR* tokens;
    DWORD n_tokens;
    LPWSTR working_path;
    if ((!parsePath(usr_path, &working_path, &tokens, &n_tokens)) || (n_tokens == 0)) {
        return STATUS_INVALID_PARAMETER;
    }
    BOOL add = FALSE;
    if ((*(*tokens)) == 0) {
        ++tokens;
        --n_tokens;
        current_path = 0;
        add = TRUE;
    }

    DWORD index;
    DWORD cluster;
    DWORD parent_cluster;
    DWORD size;
    BOOL is_dir;
    status = fat32GetFile(drv, tokens, n_tokens, current_path, &index, &cluster, &parent_cluster, &size, &is_dir);
    if (status < 0) {
        goto fat32Open_err;
    }

    DWORD hi = allocateHandle(drv);
    if (hi == 0xFFFFFFFF) {
        status = STATUS_INTERNAL_ERROR;
        goto fat32Open_err;
    }

    PFAT_HANDLE h = &(drv->handle_table[hi]);
    h->flags = HANDLE_FLAG_OPEN | (is_dir ? HANDLE_FLAG_DIR : 0);
    h->cluster_index = cluster;
    h->parent_dir_start_cluster = parent_cluster;
    h->dir_ent_index = index;
    h->size = size;
    h->depth = 0;
    h->path = working_path;

    if (add) {
        --tokens;
        ++n_tokens;
    }

    size_t buflen = wcslen(working_path) << 1;
    memset(*tokens, 0, buflen);
    HeapFree(proc_heap, 0, *tokens);
    memset(tokens, 0, ((size_t)n_tokens) << 3);
    HeapFree(proc_heap, 0, tokens);

    *handle = (HANDLE)((size_t)hi);
    return 0;

fat32Open_err:
    if (add) {
        --tokens;
        ++n_tokens;
    }
    buflen = wcslen(working_path) << 1;
    deletePath(working_path);
    memset(*tokens, 0, buflen);
    HeapFree(proc_heap, 0, *tokens);
    memset(tokens, 0, ((size_t)n_tokens) << 3);
    HeapFree(proc_heap, 0, tokens);

    return status;
}

NTSTATUS fat32Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle) {
    if ((drv == 0) || (handle == 0) || (handle == INVALID_HANDLE_VALUE) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32) || ((size_t)handle >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    PFAT_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
    if ((hdl->flags & HANDLE_FLAG_OPEN)) {
        if ((hdl->flags & HANDLE_FLAG_QUERY) != 0) {
            VirtualFree(hdl->query->dir_cache, 0, MEM_RELEASE);
            memset(hdl->query, 0, ((size_t)hdl->query->recursion_limit + 3) << 2);
            HeapFree(proc_heap, 0, hdl->query);
        }
        else {
            deletePath(hdl->path);
        }
        memset(hdl, 0, sizeof(FAT_HANDLE));
        return 0;
    }
    return STATUS_INVALID_HANDLE;
}

NTSTATUS fat32Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle, _In_opt_ UINT32 flags) {
    if ((drv == 0) || (path == 0) || (handle == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat32Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file) {
    if ((drv == 0) || (file == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32) || ((size_t)file >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat32Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer) {
    if ((drv == 0) || (file == 0) || (file == INVALID_HANDLE_VALUE) || (length == 0) || (buffer == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32) || ((size_t)file >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    PFAT_HANDLE hdl = &(drv->handle_table[(size_t)file]);
    if (!(hdl->flags & HANDLE_FLAG_OPEN) || ((hdl->flags & HANDLE_FLAG_DIR) != 0) || ((hdl->flags & HANDLE_FLAG_QUERY) != 0)) {
        return STATUS_INVALID_HANDLE;
    }
    if (hdl->size <= (offset + length)) {
        return STATUS_INVALID_PARAMETER;
    }
    return fat32UserRead(drv, hdl->cluster_index, offset, length, buffer);
}

NTSTATUS fat32Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer) {
    if ((drv == 0) || (file == 0) || (length == 0) || (buffer == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32) || ((size_t)file >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fat32GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || (handle == 0) || (handle == INVALID_HANDLE_VALUE) || (info >= FSInfoMax) || (query_data == 0) || (data_size == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32) || ((size_t)handle >= FAT_MAX_HANDLES)) {
        return STATUS_INVALID_PARAMETER;
    }
    PFAT_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
    if (((hdl->flags & HANDLE_FLAG_OPEN) == 0) || ((hdl->flags & HANDLE_FLAG_QUERY) != 0)) {
        return STATUS_INVALID_HANDLE;
    }
    if (info == FSName) {
        if (data_size < sizeof(FS_NAME_INFO)) return STATUS_INVALID_PARAMETER;
        PFS_NAME_INFO name = (PFS_NAME_INFO)query_data;
        if ((name->name_8_3_max_length == 0) || (name->name_8_3 == 0) || (name->long_name_max_length == 0) || (name->long_name == 0)) {
            return STATUS_INVALID_PARAMETER;
        }
        PFAT_DIR83 ents = 0;
        size_t len = 0;
        NTSTATUS status = readFat32Data(drv, hdl->parent_dir_start_cluster, &ents, &len);
        if (status != 0) {
            return status;
        }
        PWSTR lfn = HeapAlloc(proc_heap, 0, 512);
        if (lfn == 0) {
            errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
            status = STATUS_INTERNAL_ERROR;
            goto _exit1_get_name;
        }
        memset(lfn, 0, 512);
        status = getLongName((PFAT_DIR_LONG)ents, hdl->dir_ent_index, lfn);
        if (status != 0) {
            goto _exit_get_name;
        }

        size_t l = wcsnlen(lfn, 256);
        if (l > name->long_name_max_length) {
            status = STATUS_BUFFER_TOO_SMALL;
            name->long_name_length = (uint32_t)l;
            goto _exit_get_name;
        }

        size_t base_len = 8;
        size_t i = 8;
        while(i != 0) {
            --i;
            if (ents[hdl->dir_ent_index].filename[i] != ' ') {
                break;
            }
            --base_len;
        }
        size_t ext_len;
        if (ents[hdl->dir_ent_index].extension[2] != ' ') {
            ext_len = 4;
        }
        else if (ents[hdl->dir_ent_index].extension[1] != ' ') {
            ext_len = 3;
        }
        else if (ents[hdl->dir_ent_index].extension[0] != ' ') {
            ext_len = 2;
        }
        else {
            ext_len = 0;
        }

        size_t fl = base_len + ext_len;
        name->name_8_3_length = (uint32_t)fl+1;

        if (fl >= name->name_8_3_max_length) {
            status = STATUS_BUFFER_TOO_SMALL;
            goto _exit_get_name;
        }
        memcpy(name->name_8_3, &(ents[hdl->dir_ent_index].filename), base_len);

        if (ext_len != 0) {
            name->name_8_3[base_len] = '.';
            memcpy(&(name->name_8_3[base_len + 1]), &(ents[hdl->dir_ent_index].extension), ext_len - 1);
        }
        name->name_8_3[fl] = 0;

        name->long_name_length = (uint32_t)l;
        memcpy(name->long_name, lfn, l << 1);

        status = STATUS_SUCCESS;

    _exit_get_name:
       HeapFree(proc_heap, 0, lfn);
    _exit1_get_name:
        VirtualFree(ents, 0, MEM_RELEASE);
        return status;
    }
    else if (info == FSSize) {
        if (data_size < sizeof(FS_FILESIZE_INFO)) return STATUS_INVALID_PARAMETER;
        PFS_FILESIZE_INFO filesize = (PFS_FILESIZE_INFO)query_data;
        size_t fsz = 0;
        if (!getFat32FileSize(drv, hdl->cluster_index, &fsz)) {
            return STATUS_INTERNAL_ERROR;
        }
        filesize->size = hdl->size;
        filesize->size_on_disk = fsz;
    }
    else if (info == FSAttribute) {
        if (data_size < sizeof(FS_ATTRIBUTE_INFO)) return STATUS_INVALID_PARAMETER;

        if (hdl->parent_dir_start_cluster == 0xFFFFFFFF) {
            if (hdl->cluster_index != drv->params.root_cluster) {
                // corrupt handle because only the root dir cannot have a parent directory
                return STATUS_INVALID_HANDLE;
            }
            // return proxy attributes for the root dir
            *((PFS_ATTRIBUTE_INFO)query_data) = FS_ATTRIBUTE_DIR | FS_ATTRIBUTE_SYSTEM;
            return 0;
        }

        PFAT_DIR83 dir = 0;
        size_t len;
        NTSTATUS status = readFat32Data(drv, hdl->parent_dir_start_cluster, &dir, &len);
        if (status != 0) {
            return status;
        }
        if (hdl->dir_ent_index >= (len >> 5)) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            status = 0;
            UINT32 attrib = (UINT32)(dir[hdl->dir_ent_index].attribute);
            UINT32 attr = 0;
            if ((attrib & FAT_ATTRIB_READ_ONLY)) {
                attr |= FS_ATTRIBUTE_READ_ONLY;
            }
            if ((attrib & FAT_ATTRIB_HIDDEN)) {
                attr |= FS_ATTRIBUTE_HIDDEN;
            }
            if ((attrib & FAT_ATTRIB_SYSTEM)) {
                attr |= FS_ATTRIBUTE_SYSTEM;
            }
            if ((attrib & FAT_ATTRIB_ARCHIVE)) {
                attr |= FS_ATTRIBUTE_ARCHIVE;
            }
            if ((attrib & FAT_ATTRIB_DIRECTORY)) {
                attr |= FS_ATTRIBUTE_DIR;
            }
            *((PFS_ATTRIBUTE_INFO)query_data) = attr;
        }
        VirtualFree(dir, 0, MEM_RELEASE);
        return status;
    }
    else if (info == FSDateInfo) {
        if (data_size < sizeof(FS_DATE_INFO)) return STATUS_INVALID_PARAMETER;
        PFAT_DIR83 dir = 0;
        size_t len;
        NTSTATUS status = readFat32Data(drv, hdl->parent_dir_start_cluster, &dir, &len);
        if (status != 0) {
            return status;
        }
        if (hdl->dir_ent_index >= (len >> 5)) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            PFS_DATE_INFO date = (PFS_DATE_INFO)query_data;
            status = 0;
            SYSTEMTIME systime = { 0 };
            FILETIME creation;
            FILETIME modified;
            FILETIME accessed;
            PFAT_DIR83 dt = &(dir[hdl->dir_ent_index]);

            systime.wMilliseconds = ((WORD)(dt->creation_seconds) % 100) * 10;
            systime.wSecond = ((WORD)(dt->creation_seconds) / 100) + (((dt->creation_time) & 0x1F) << 1);
            systime.wMinute = ((dt->creation_time) & 0x7E0) >> 5;
            systime.wHour = ((dt->creation_time) & 0xF800) >> 11;
            systime.wDayOfWeek = 0; // not needed
            systime.wDay = ((dt->creation_date) & 0x1F);
            systime.wMonth = (((dt->creation_date) & 0x1E0) >> 5);
            systime.wYear = (((dt->creation_date) & 0xFE00) >> 9) + 1980;
            if (!SystemTimeToFileTime(&systime, &creation)) {
                errPrintf("error converting creation date:%x\n\r", GetLastError());
                status = STATUS_INTERNAL_ERROR;
                goto _date_info_exit;
            }

            systime.wMilliseconds = 0;
            systime.wSecond = ((dt->last_mod_time) & 0x1F) << 1;
            systime.wMinute = ((dt->last_mod_time) & 0x7E0) >> 5;
            systime.wHour = ((dt->last_mod_time) & 0xF800) >> 11;
            systime.wDayOfWeek = 0; // not needed
            systime.wDay = ((dt->last_mod_date) & 0x1F);
            systime.wMonth = (((dt->last_mod_date) & 0x1E0) >> 5);
            systime.wYear = (((dt->last_mod_date) & 0xFE00) >> 9) + 1980;
            if (!SystemTimeToFileTime(&systime, &modified)) {
                errPrintf("error converting modified date:%x\n\r", GetLastError());
                status = STATUS_INTERNAL_ERROR;
                goto _date_info_exit;
            }

            systime.wMilliseconds = 0;
            systime.wSecond = 0;
            systime.wMinute = 0;
            systime.wHour = 0;
            systime.wDayOfWeek = 0; // not needed
            systime.wDay = ((dt->last_accessed_date) & 0x1F);
            systime.wMonth = (((dt->last_accessed_date) & 0x1E0) >> 5);
            systime.wYear = (((dt->last_accessed_date) & 0xFE00) >> 9) + 1980;
            if (!SystemTimeToFileTime(&systime, &accessed)) {
                errPrintf("error converting modified date:%x\n\r", GetLastError());
                status = STATUS_INTERNAL_ERROR;
                goto _date_info_exit;
            }

            date->creation = creation;
            date->last_modified = modified;
            date->last_accessed = accessed;
        }
    _date_info_exit:
        VirtualFree(dir, 0, MEM_RELEASE);
        return status;
    }
    else if (info == FSGetFirst) {
        if (data_size < sizeof(FS_GET_FIRST_INFO)) return STATUS_INVALID_PARAMETER;
        PFS_GET_FIRST_INFO info = (PFS_GET_FIRST_INFO)query_data;
        HANDLE handle = info->h_query;
        if ((handle == 0) || (handle == INVALID_HANDLE_VALUE)) {
            return STATUS_INVALID_HANDLE;
        }
        PFAT_HANDLE hdl = &(drv->handle_table[(size_t)handle]);
        if (((hdl->flags & HANDLE_FLAG_OPEN) == 0) || ((hdl->flags & HANDLE_FLAG_DIR) == 0) || ((hdl->flags & HANDLE_FLAG_QUERY) != 0)) {
            return STATUS_INVALID_HANDLE;
        }

        PFAT_DIR83 dirs;
        size_t len;
        NTSTATUS status = readFat32Data(drv, hdl->cluster_index, &dirs, &len);
        if (status < 0) {
            return status;
        }

        

        return STATUS_NOT_IMPLEMENTED;
    }
    else if (info == FSGetNext) {
        if (data_size < sizeof(HANDLE)) return STATUS_INVALID_PARAMETER;

        // TODO

        return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_SUCCESS;
}

NTSTATUS fat32SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size) {
    if ((drv == 0) || (handle == 0) || (info > FSDateInfo) || (query_data == 0) || (data_size == 0) || ((drv->flags & 0xff) != FAT_FLAG_VALID) || ((drv->flags & FAT_MASK_FAT) != FAT_FLAG_FAT32)) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_NOT_IMPLEMENTED;
}