#pragma once

#define isInvalidJMP(boot) ((boot->jump[0] != 0xE9) && (boot->jump[0] != 0xEB || boot->jump[2] != 0x90))

#define MAX_NTFS_HANDLES 0x200

#define NTFS_HANDLE_OPEN 0x80000000

typedef struct _NTFS_HANDLE {
	UINT32 flags;
	NTFS_FILE_REF ref;
	PNTFS_FILE_RECORD file_record;
} NTFS_HANDLE, * PNTFS_HANDLE;

#define NTFS_SIGNATURE_VALID 0x01020304
#define NTFS_SIGNATURE_MASK  0x0F0F0F0F

#define IS_VALID_DRV(drv) ((drv != 0) && ((drv->signature & NTFS_SIGNATURE_MASK) == NTFS_SIGNATURE_VALID))

#define NTFS_NEXT_ATTRIB(attrib) (PNTFS_STD_ATTRIB_HEADER)(((size_t)attrib) + (size_t)(((PNTFS_STD_ATTRIB_HEADER)attrib)->length))
#define NTFS_RESIDENT_ATTR_DATA(attrib) (PVOID)(((size_t)attrib) + (size_t)(((PNTFS_STD_ATTRIB_HEADER)attrib)->resident.attrib_offset))
#define NTFS_NON_RESIDENT_DATA_RUNS(attrib) (PVOID)(((size_t)attrib) + (size_t)(((PNTFS_STD_ATTRIB_HEADER)attrib)->non_resident.data_runs_offset))

#define NTFS_INDEX_VALUE_NEXT(val) (PNTFS_INDEX_VALUE)(((size_t)val) + (size_t)(((PNTFS_INDEX_VALUE)val)->value_size))
#define NTFS_INDEX_VALUE_SUBNODE_VCN(val) (UINT64*)(((size_t)val) + (size_t)(((PNTFS_INDEX_VALUE)val)->value_size) - 8);

#define NTFS_DATA_RUN_FLAG_SPARSE     0x40000000
#define NTFS_DATA_RUN_FLAG_COMPRESSED 0x80000000

typedef struct _NTFS_DATA_RUN {
	UINT32 flags;
	UINT32 unused;
	UINT64 length;
	UINT64 offset;
} NTFS_DATA_RUN, * PNTFS_DATA_RUN;

typedef struct _NTFS_DATA_RUNS {
	UINT32 n;
	UINT32 n_parsed;
	UINT64 total_length;
	NTFS_DATA_RUN runs[ANYSIZE_ARRAY];
} NTFS_DATA_RUNS, * PNTFS_DATA_RUNS;

typedef struct _NTFS_DRIVER {
	FS_DRIVER interf;
	UINT32 signature;
	UINT32 flags;
	UINT32 bytes_per_sector;
	UINT32 bytes_per_cluster;
	UINT64 lcn_mft;
	UINT64 lcn_mirr_mft;
	UINT32 bytes_per_mft;
	UINT32 bytes_per_ind;
	PNTFS_HANDLE handle_table;
} NTFS_DRIVER, * PNTFS_DRIVER;

NTSTATUS NTFSOpen(_In_ PNTFS_DRIVER this_handle, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle);
NTSTATUS NTFSClose(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE handle);
NTSTATUS NTFSCreate(_In_ PNTFS_DRIVER this_handle, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle, _In_opt_ UINT32 flags);
NTSTATUS NTFSDelete(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE file);
NTSTATUS NTFSReadFile(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
NTSTATUS NTFSWriteFile(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer);
NTSTATUS NTFSGetInfo(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_opt_(data_size) PVOID query_data, _In_opt_ DWORD data_size);
NTSTATUS NTFSSetInfo(_In_ PNTFS_DRIVER this_handle, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);