#pragma once

/*
* Based on Microsoft VHDX specs
* download:
* https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-vhdx/83e061f8-f6e2-4de1-91bd-5d518a43d477
*/

#define VHDX_ID_SIGNATURE       0x7668647866696C65
#define VHDX_HDR_SIGNATURE      0x68656164
#define VHDX_REGION_SIGNATURE   0x72656769
#define VHDX_LOG_SIGNATURE      0x65676F6C
#define VHDX_ZERO_SIGNATURE     0x6F72657A
#define VHDX_DESC_SIGNATURE     0x63736564
#define VHDX_DATA_SIGNATURE     0x61746164
#define VHDX_METADATA_SIGNATURE 0x617461646174656D

#define VHDX_BAT_MASK_STATE  0x0000000000000007
#define VHDX_BAT_MASK_OFFSET 0xfffffffffff00000

#define VHDX_BAT_STATE_NOT_PRESENT             0x0
#define VHDX_BAT_STATE_BLOCK_UNDEFINED         0x1
#define VHDX_BAT_STATE_BLOCK_ZERO              0x2
#define VHDX_BAT_STATE_BLOCK_UNMAPPED          0x3
#define VHDX_BAT_STATE_BLOCK_FULLY_PRESENT     0x6
#define VHDX_BAT_STATE_BLOCK_PARTIALLY_PRESENT 0x7

#define VHDX_SB_BLOCK_NOT_PRESENT 0x0
#define VHDX_SB_BLOCK_PRESENT     0x6

#define VHDX_BAT_GUID      "{2DC27766-F623-4200-9D64-115E9BFD4A08}"
#define VHDX_METADATA_GUID "{8B7CA206-4790-4B9A-B8FE-575F050F886E}"

#define VHDX_METADATA_FILE_PARAMS_GUID          "{CAA16737-FA36-4D43-B3B6-33F0AA44E76B}"
#define VHDX_METADATA_VIRTUAL_DISK_SIZE_GUID    "{2FA54224-CD1B-4876-B211-5DBED83BF4B8}"
#define VHDX_METADATA_VIRTUAL_DISK_ID_GUID      "{BECA12AB-B2E6-4523-93EF-C309E000C746}"
#define VHDX_METADATA_LOGICAL_SECTOR_SIZE_GUID  "{8141BF1D-A96F-4709-BA47-F233A8FAAB5F}"
#define VHDX_METADATA_PHYSICAL_SECTOR_SIZE_GUID "{CDA348C7-445D-4471-9CC9-E9885251C556}" 
#define VHDX_METADATA_PARENT_LOCATOR_GUID       "{A8D35F2D-B30B-454D-ABF7-D3D84834AB0C}"

#define VHDX_PARENT_LOCATOR_GUID "{B04AEFB7-D19E-4A81-B789-25B8E9445913}"

PACK

// at the beginning of the file
typedef struct _VHDX_ID_HEADER {
	UINT64 signature;     // contains VHDX_ID_SIGNATURE
	wchar_t creator[256]; // contains creator signatures for diagnostic purpose
} VHDX_ID_HEADER, * PVHDX_ID_HEADER;


// at 64KB and 128KB
typedef struct _VHDX_HEADER {
	UINT32 signature;       // contains VHDX_HDR_SIGNATURE
	UINT32 checksum;        // CRC32
	UINT64 sequence_number; // identifies the most recent header
	GUID file_write_guid;   // must be changed before writing to file
	GUID data_write_guid;   // must be changed before writing to data
	GUID log_guid;          // zero when no logs else must be changed before writing to logs
	UINT16 log_version;     // must be 0
	UINT16 version;         // must be 1
	UINT32 log_length;      // length of logs
	UINT64 log_offset;      // offset to log header
	UINT8 reserved[4016];   // zero
} VHDX_HEADER, * PVHDX_HEADER;

// at 192KB and 256KB
typedef struct _VHDX_REGION_TABLE_HEADER {
	UINT32 signature;   // contains VHDX_REGION_SIGNATURE
	UINT32 checksum;    // CRC32 over the header and the entire region table
	UINT32 entry_count; // must be <= 2047
	UINT32 reserved;    // zero
} VHDX_REGION_TABLE_HEADER, * PVHDX_REGION_TABLE_HEADER;

typedef struct _VHDX_REGION_TABLE_ENTRY {
	GUID guid;         // guid to identify data
	UINT64 offset;     // offset to data
	UINT32 length;     // length (must be multiple of 1MB)
	UINT32 recognized; // if equals to 1 then do not load vhdx file
} VHDX_REGION_TABLE_ENTRY, * PVHDX_REGION_TABLE_ENTRY;

typedef struct _VHDX_VHDX_LOG_ENTRY_HEADER {
	UINT32 signature;           // contains VHDX_LOG_SIGNATURE
	UINT32 checksum;            // CRC32 over all header and entry bytes
	UINT32 entry_length;        // length of this header and entries
	UINT32 tail;                // offset to log entries
	UINT64 sequence_number;     // most recent sequence number
	UINT32 descriptor_count;    //number of descriptors present
	UINT32 reserved;            // zero
	GUID log_guid;              // must be log guid in file header
	UINT64 flushed_file_offset; // stores vhdx file size ??
	UINT64 last_file_offset;    // stores vhdx file size ??
} VHDX_LOG_ENTRY_HEADER, * PVHDX_LOG_ENTRY_HEADER;

typedef struct _VHDX_ZERO_DESCRIPTOR {
	UINT32 zero_signature; // contains VHDX_ZERO_SIGNATURE
	UINT32 reserved;       // zero
	UINT64 zero_length;    // length of zeros (multiple of 4KB)
	UINT64 file_offset;    // offset to zeros (4KB aligned)
	UINT64 sequence_nuber; // sequence number of log entrys header
} VHDX_ZERO_DESCRIPTOR, * PVHDX_ZERO_DESCRIPTOR;

typedef struct _VHDX_DATA_DESCRIPTOR {
	UINT32 data_signature;  // contains VHDX_DESC_SIGNATURE
	UINT32 trailing_bytes;  // trailing bytes from data sector
	UINT64 leading_bytes;   // leading bytes from data sector
	UINT64 file_offset;     // offset to data (4KB aligned)
	UINT64 sequence_number; // sequence number of the log entrys header
} VHDX_DATA_DESCRIPTOR, * PVHDX_DATA_DESCRIPTOR;

typedef struct _VHDX_DATA_SECTOR {
	UINT32 signature;     // contains VHDX_DATA_SIGNATURE
	UINT32 sequence_high; // high 4 bytes from sequence number
	UINT8 data[4084];     // actual data (leading 8 and trailing 4 bytes are stored in header)
	UINT32 sequence_low;  // low 4 bytes from sequence number
} VHDX_DATA_SECTOR, * PVHDX_DATA_SECTOR;

// bits 0-2: state, bits 3-19: reserved (zero), bits 20-63: offset in MB
typedef UINT64 VHDX_BAT_ENTRY;

typedef struct _VHDX_METADATA_TABLE_HEADER {
	UINT64 signature;    // contains VHDX_METADATA_SIGNATURE
	UINT16 reserved;     // zero
	UINT16 entry_count;  // number of entries in table
	UINT8 reserved2[20]; // zero
} VHDX_METADATA_TABLE_HEADER, * PVHDX_METADATA_TABLE_HEADER;

typedef struct _VHDX_METADATA_TABLE_ENTRY {
	GUID item_id;    // id of item
	UINT32 offset;   // offset from metadata region to data
	UINT32 lenth;    // length of metadata
	UINT32 attrib;   // attributes bit 0: isUser, bit 1: isVirtualDisk, bit 2: isRequired, its 3-31: reserved (zero)
	UINT32 reserved; // zero
} VHDX_METADATA_TABLE_ENTRY, * PVHDX_METADATA_TABLE_ENTRY;

typedef struct _VHDX_PARENT_LOCATOR_HEADER {
	GUID parent_locator_type; // type of locator
	UINT16 reserved;          // zero
	UINT16 key_value_count;   // number of key-value pairs in locator
} VHDX_PARENT_LOCATOR_HEADER, * PVHDX_PARENT_LOCATOR_HEADER;

typedef struct _VHDX_PARENT_LOCATOR_ENTRY {
	UINT32 key_offset;   // offset within metadata item
	UINT32 value_offset; // offset within metadata item
	UINT16 key_length;   // length of key in bytes > 0
	UINT16 value_length; // length of value in bytes > 0
} VHDX_PARENT_LOCATOR_ENTRY, * PVHDX_PARENT_LOCATOR_ENTRY;

ENDPACK