#pragma once

typedef struct _FAT_CONST_PARAMS {
    UINT16 bytes_per_sector;
    UINT8 sectors_per_cluster;
    UINT8 n_fats;
    UINT16 n_res_sectors;
    UINT32 root_cluster;
    UINT32 clusters_per_fat;
    UINT32 n_sectors;
    UINT32 sectors_per_fat;
    size_t bytes_per_fat;
    size_t bytes_per_cluster;
    size_t root_offset;
    size_t root_size;
    size_t data_offset;
    size_t data_size;
} FAT_CONST_PARAMS, * PFAT_CONST_PARAMS;

#define HANDLE_FLAG_OPEN   0x80000000
#define HANDLE_FLAG_DIR    0x00000001
#define HANDLE_FLAG_QUERY  0x40000000
#define HANDLE_FLAG_QUERY_RECURSIVE 0x08000000

typedef struct _FAT_QUERY_DATA {
    PFAT_DIR83 dir_cache;
    UINT32 dir_len;
    UINT32 recursion_limit;
    UINT32 indices[ANYSIZE_ARRAY];
} FAT_QUERY_DATA, * PFAT_QUERY_DATA;

typedef struct _FAT_HANDLE {
    UINT32 flags;
    UINT32 cluster_index;
    UINT32 parent_dir_start_cluster;
    UINT32 dir_ent_index;
    UINT32 size;
    UINT32 depth;
    union {
        PFAT_QUERY_DATA query;
        PWSTR path;
    };
} FAT_HANDLE, * PFAT_HANDLE;

#define FAT_MAX_HANDLES 0x100

// this must be a power of two
#define FAT_CACHE_SIZE  0x100

#define FAT_MASK_FAT   0xFF000000
#define FAT_FLAG_FAT12 0x12000000
#define FAT_FLAG_FAT16 0x16000000
#define FAT_FLAG_FAT32 0x32000000

// all other values than these are a sign of corruption
#define FAT_FLAG_VALID   0x00000077
#define FAT_FLAG_INVALID 0x000000CC

typedef struct _FAT_DRIVER_IMPL {
    FS_DRIVER interf;
    UINT32 pad;
    UINT32 flags;
    FAT_CONST_PARAMS params;
    UINT32 start_looking;
    UINT32 n_free_clus;
    PFAT_HANDLE handle_table;
} FAT_DRIVER_IMPL, * PFAT_DRIVER_IMPL;

#define isValidJMP(bpb) ((bpb->jmp[0] == 0xE9) || (bpb->jmp[0] == 0xEB && bpb->jmp[2] == 0x90))
#define isValidBPS(bpb) ((bpb->bytes_per_sector == 0x200) || (bpb->bytes_per_sector == 0x4009) || (bpb->bytes_per_sector == 0x800) || (bpb->bytes_per_sector == 0x1000))
#define isValidSPC(bpb) (((bpb->sectors_per_cluster & (bpb->sectors_per_cluster - 1)) == 0) && (bpb->sectors_per_cluster != 0))
#define isValidNRES(bpb) (bpb->n_reserved_sectors != 0)
#define isValidTOTSECS(bpb) ((bpb->n_sectors == 0) ^ (bpb->n_large_sectors == 0))

// deprecated: #define isValidMEDIADSC(bpb) (((bpb->media_descriptor_type & 0xF0) == 0xF0) && ((bpb->media_descriptor_type & 0xF) == 0 || (bpb->media_descriptor_type & 0xF) >= 8))

//#define isValidBPB(bpb) (isValidJMP(bpb) && isValidBPS(bpb) && isValidSPC(bpb) && isValidNRES(bpb) && isValidTOTSECS(bpb))

NTSTATUS fat12Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle);
NTSTATUS fat12Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle);
NTSTATUS fat12Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE, _In_opt_ UINT32 flags);
NTSTATUS fat12Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file);
NTSTATUS fat12Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
NTSTATUS fat12Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer);
NTSTATUS fat12GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_opt_(data_size) PVOID query_data, _In_opt_ DWORD data_size);
NTSTATUS fat12SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);

NTSTATUS fat16Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle);
NTSTATUS fat16Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle);
NTSTATUS fat16Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE, _In_opt_ UINT32 flags);
NTSTATUS fat16Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file);
NTSTATUS fat16Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
NTSTATUS fat16Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer);
NTSTATUS fat16GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_opt_(data_size) PVOID query_data, _In_opt_ DWORD data_size);
NTSTATUS fat16SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);

NTSTATUS fat32Open(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE handle);
NTSTATUS fat32Close(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle);
NTSTATUS fat32Create(_In_ PFAT_DRIVER_IMPL drv, _In_ LPCWSTR path, _In_opt_ HANDLE current_path, _Out_ PHANDLE, _In_opt_ UINT32 flags);
NTSTATUS fat32Delete(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file);
NTSTATUS fat32Read(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _In_reads_bytes_(length) void* buffer);
NTSTATUS fat32Write(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE file, _In_opt_ size_t offset, _In_ size_t length, _Out_writes_bytes_(length) void* buffer);
NTSTATUS fat32GetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _Inout_updates_bytes_opt_(data_size) PVOID query_data, _In_opt_ DWORD data_size);
NTSTATUS fat32SetInfo(_In_ PFAT_DRIVER_IMPL drv, _In_ HANDLE handle, _In_ FS_INFO_TYPE info, _In_reads_bytes_(data_size) PVOID query_data, _In_ DWORD data_size);