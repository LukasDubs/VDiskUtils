#pragma once

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
#include <stdint.h>

typedef _Return_type_success_(return != 0) BOOL BSTATUS;

typedef BSTATUS(*VDISK_GET_SIZE)(_In_ void* vdisk, _Inout_ size_t* length);
typedef BSTATUS(*VDISK_READ)(_Inout_ void* vdisk, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer);
typedef BSTATUS(*VDISK_WRITE)(_Inout_ void* vdisk, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
typedef void(*VDISK_DRIVER_EXIT)(_In_ void* this_struct);

typedef struct _VDISK_DRIVER {
	VDISK_GET_SIZE get_size;
	VDISK_READ read;
	VDISK_WRITE write;
	VDISK_DRIVER_EXIT exit;
	// driver can and will store implementation specific info here so NEVER rely on the size of this struct definition
} VDISK_DRIVER, * PVDISK_DRIVER;

extern PVDISK_DRIVER raw_driver;

#define VDISK_ATTRIBUTE_FLAG_OPEN				0x80000000
#define VDISK_ATTRIBUTE_FLAG_MAPPED				0x40000000
#define VDISK_ATTRIBUTE_FLAG_GPT				0x00000004
#define VDISK_ATTRIBUTE_FLAG_MBR				0x00000008
#define VDISK_ATTRIBUTE_FLAG_RAW                0x0000000C

#define VDISK_ATTRIBUTE_MASK_VDISK_TYPE			0x00000003
#define VDISK_ATTRIBUTE_MASK_VDISK_PARTITION	0x0000000C

#define VDISK_TYPE_NONE							0x00000000
#define VDISK_TYPE_VHD_FIXED					0x00000001
#define VDISK_TYPE_VHD_DYNAMIC					0x00000002
#define VDISK_TYPE_VHD_DIFFERENTIATING			0x00000003

#define VDISK_GET_STRUCT(type, vdisk, offset) ((type)(((uint64_t)(vdisk->buffer)) + (uint64_t)(offset)))

typedef struct _VDISK {
	LPCWSTR path;         // path to the vdisk file
	UINT32 attributes;    // attributes
	UINT32 n_partitions;  // number of partitions
	void* partitions;     // partitions pointer
	HANDLE file;          // file handle
	UINT64 length;        // length of the file (not disk space)
	HANDLE mapping;       // handle for mapping object
	void* buffer;         // buffer for raw access
	PVDISK_DRIVER driver; // driver for virtual harddisk formats
} VDISK, * PVDISK;

typedef struct _PARTITION {
	void* attributes; // pointer to partition mechanism specific info
	PVDISK parent;    // pointer to parent vdisk
	UINT64 start;     // starting byte offset
	UINT64 length;    // length of partition in bytes
} PARTITION, * PPARTITION;

BSTATUS openVdiskP(_In_ LPWSTR file);
BSTATUS openVdiskI(_In_ size_t vdisk);
BSTATUS closeVDISK(_In_ size_t vdisk);
void listVDISKs();
BSTATUS selectVdiskI(_In_ size_t index);
BSTATUS selectVdiskP(_In_ LPCWSTR path);

// maps the vdisk to memory
BSTATUS mapVDISKFile(_Inout_ PVDISK vdisk);

// expands the file size of the disk
_Check_return_ BSTATUS expandVDISKFile(_Inout_ PVDISK vdisk, _In_opt_ uint64_t amount);

void listPartitions();
BSTATUS selectPartition(_In_ size_t index);

BSTATUS partitionRead(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_all_(length) void* buffer);
BSTATUS partitionWrite(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);

typedef void(*FS_DRIVER_EXIT)(_In_ void* this_handle);

typedef enum _FS_INFO_TYPE {
	FSName,      // gets/sets the name
	FSSize,      // gets/sets the size
	FSAttribute, // gets/sets the attribute
	FSDateInfo,  // gets/sets the date
	FSGetFirst,  // gets the first file in directory
	FSGetNext,   // gets the next file in the directory
	FSInfoMax
} FS_INFO_TYPE, * PFS_INFO_TYPE;

typedef struct _FS_NAME_INFO {
	UINT32 name_8_3_length;      // length of 8.3 name
	UINT32 name_8_3_max_length;  // max length of 8.3 name
	PSTR name_8_3;              // 8.3 name buffer
	UINT32 long_name_length;     // LFN length
	UINT32 long_name_max_length; // LFN max length
	PWSTR long_name;             // LFN buffer
} FS_NAME_INFO, * PFS_NAME_INFO;

typedef struct _FS_FILESIZE_INFO {
	size_t size;
	size_t size_on_disk;
} FS_FILESIZE_INFO, * PFS_FILESIZE_INFO;

#define FS_ATTRIBUTE_READ_ONLY 0x1
#define FS_ATTRIBUTE_HIDDEN 0x2
#define FS_ATTRIBUTE_SYSTEM 0x4
#define FS_ATTRIBUTE_ARCHIVE 0x8
#define FS_ATTRIBUTE_DIR 0x10 // read-only attribute

typedef UINT32 FS_ATTRIBUTE_INFO, * PFS_ATTRIBUTE_INFO; // attribute: FS_ATTRIBUTE_...

typedef struct _FS_DATE_INFO {
	FILETIME creation;      // creation time as FILETIME
	FILETIME last_modified; // last modified time as FILETIME
	FILETIME last_accessed; // last accessed time as FILETIME
} FS_DATE_INFO, * PFS_DATE_INFO;

typedef struct _FS_GET_FIRST_INFO {
	BOOL recursive;			// get subdirs too
	UINT32 recursion_limit; // recursion limit
	HANDLE h_query;			// input the starting direcory, returns the first handle that can be used for get_next
} FS_GET_FIRST_INFO, * PFS_GET_FIRST_INFO;

#define FS_CREATE_FLAG_CREATE_NEW    0x0 // creates only if the file doesn't exist
#define FS_CREATE_FLAG_CREATE_ALWAYS 0x1 // creates always a new empty file, even if the file already exists
#define FS_CREATE_FLAG_OPEN_EXISTING 0x2 // open existing file (should use the open function to not have all the overhead)
#define FS_CREATE_FLAG_OPEN_ALWAYS   0x3 // open always and create a new one if the file doesn't exist
#define FS_CREATE_FLAG_DIRECTORY     0x4 // creates a directory instead of a file

typedef NTSTATUS(*FS_OPEN_FILE)(_In_ void* this_handle, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle);
typedef NTSTATUS(*FS_CLOSE_FILE)(_In_ void* this_handle, _In_ HANDLE handle);
typedef NTSTATUS(*FS_CREATE_FILE)(_In_ void* this_handle, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE, _In_opt_ UINT32 flags);
typedef NTSTATUS(*FS_DELETE_FILE)(_In_ void* this_handle, _In_ HANDLE file);
typedef NTSTATUS(*FS_READ_FILE)(_In_ void* this_handle, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
typedef NTSTATUS(*FS_WRITE_FILE)(_In_ void* this_handle, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer);
typedef NTSTATUS(*FS_GET_FILE_INFO)(_In_ void* this_handle, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);
typedef NTSTATUS(*FS_SET_FILE_INFO)(_In_ void* this_handle, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);

#define FS_DRIVER_TYPE_FAT12 0x12
#define FS_DRIVER_TYPE_FAT16 0x16
#define FS_DRIVER_TYPE_FAT32 0x32
#define FS_DRIVER_TYPE_EXFAT 0x33
#define FS_DRIVER_TYPE_NTFS  0x64

typedef struct _FS_DRIVER {
	UINT32 partition_index;
	UINT32 fs_type; // FS_DRIVER_TYPE_...
	PVDISK vdisk;
	FS_DRIVER_EXIT exit;
	FS_OPEN_FILE open;
	FS_CLOSE_FILE close;
	FS_CREATE_FILE create;
	FS_DELETE_FILE del;
	FS_READ_FILE read;
	FS_WRITE_FILE write;
	FS_GET_FILE_INFO get_info;
	FS_SET_FILE_INFO set_info;
} FS_DRIVER, * PFS_DRIVER;

PFS_DRIVER createDriver(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index);
PFS_DRIVER getSelDrv();

#include "formats.h"
#include "utils.h"