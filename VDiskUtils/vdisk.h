#pragma once

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
	// driver can and will store implementation specific info here so NEVER trust the size of this struct
} VDISK_DRIVER, * PVDISK_DRIVER;

extern PVDISK_DRIVER raw_driver;

#define VDISK_ATTRIBUTE_FLAG_OPEN				0x80000000
#define VDISK_ATTRIBUTE_FLAG_MAPPED				0x40000000
#define VDISK_ATTRIBUTE_FLAG_GPT				0x00000004
#define VDISK_ATTRIBUTE_FLAG_MBR				0x00000008
#define VDISK_ATTRIBUTE_MASK_VDISK_TYPE			0x00000003

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

#include "formats.h"
#include "utils.h"