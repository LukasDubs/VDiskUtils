#pragma once

/*
* Based on Microsofts VHD specs
* download:
* https://www.microsoft.com/en-au/download/details.aspx?id=23850
*/

#define VHD_COOKIE						0x78697463656e6f63
#define VHD_FEATURES_NO					0x00000000
#define VHD_FEATURES_TEMPORARY			0x01000000
#define VHD_FEATURES_RESERVED			0x02000000
#define VHD_VERSION						0x00000100
#define VHD_CREATOR_APP_DISKPART		0x206e6977
#define VHD_CREATOR_VERSION_DISKPART	0xa00

#define VHD_CREATOR_OS_WINDOWS			0x6b326957
#define VHD_CREATOR_OS_MAC				0x2063614D

#define VHD_DISK_TYPE_NONE		0x00000000
#define VHD_DISK_TYPE_RESERVED	0x01000000
#define VHD_DISK_TYPE_FIXED		0x02000000
#define VHD_DISK_TYPE_DYNAMIC	0x03000000
#define VHD_DISK_TYPE_DIFF		0x04000000

#define VHD_SAVED				0x1

#define VHD_DYNAMIC_COOKIE		0x6573726170737863
#define VHD_DYNAMIC_VERSION		0x00000100

#define VHD_PLATFORM_Wi2r		0x72326957 // relative ansi path
#define VHD_PLATFORM_Wi2k		0x6B326957 // absolute ansi path
#define VHD_PLATFORM_W2ru		0x75723257 // relative unicode path
#define VHD_PLATFORM_W2ku		0x756B3257 // absolute unicode path
#define VHD_PLATFORM_MAC		0x2063614D
#define VHD_PLATFORM_MACX		0x5863614D // file url

#define VHD_BAT_UNUSED			0xffffffff

PACK

/********************************************************************************\
*																				 *
*							  BIG ENDIAN WARNING!!!!!!!							 *
*																				 *
*		The following structs require numbers to be stored as Big Endian numbers *
*			 NOTE: Macros for these structs are already converted to big endian  *
*																				 *
\********************************************************************************/

typedef struct ___CHS {
	UINT16 C; // cylinders
	UINT8 H;  // heads
	UINT8 S;  // sectors
} __CHS;

typedef union _CHS {
	__CHS chs;
	UINT32 lchs;
} CHS;

typedef struct _VHD_FOOTER {
	UINT64 cookie;              // cookie to identify creator (VHD_COOKIE)
	UINT32 features;            // feature support (VHD_FEATURES_RESERVED must be always present)
	UINT32 file_format_version; // format version (always VHD_VERSION)
	UINT64 offset;              // absolute offset to dynaic header (0xffffffff for fixed disks)
	UINT32 timestamp;           // creation time stamp in seconds since January 1, 2000
	UINT32 creator_app;         // creator app identification
	UINT32 creator_version;     // creator version
	UINT32 creator_os;          // creator host os
	UINT64 original_size;       // original size at creation time for vm information
	UINT64 current_size;        // current size for vm information
	CHS disk_geometry;          // disk geometry in chs format (always rounded down)
	UINT32 disk_type;           // disk type (VHD_DISK_TYPE_...)
	UINT32 checksum;            // ones complement of all bytes in the header added
	UINT8 uid[16];              // generated uid
	UINT8 saved_state;          // Saved state
	UINT8 reserved[427];        // reserved (zero)
} VHD_FOOTER, * PVHD_FOOTER;

typedef struct _VHD_PARENT_LOCATOR {
	UINT32 platform_code;        // platform for format identification
	UINT32 data_space;           // number of 512 byte sectors used for parent locator
	UINT32 data_length;          // actual length of parent hard disk locator
	UINT32 reserved;             // reserved (zero)
	UINT64 platform_data_offset; // absolute offset to platform specific file locator
} VHD_PARENT_LOCATOR, * PVHD_PARENT_LOCATOR;

typedef struct _VHD_DYNAMIC_HEADER {
	UINT64 cookie;                 // cookie to identify header (VHD_DYNAMIC_COOKIE)
	UINT64 data_offset;            // currently unused (0xFFFFFFFFFFFFFFFF)
	UINT64 table_offset;           // offset to BAT (Block Allocation Table)
	UINT32 header_version;         // version on dynamic header (VHD_DYNAMIC_VERSION)
	UINT32 max_table_entries;      // maximum entries present in the BAT
	UINT32 block_size;             // size of data section of block
	UINT32 checkusm;               // ones complement of all bytes in the header added
	UINT8 uid[16];                 // parent hard disk uid
	UINT32 parent_time_stamp;      // modification time stamp in seconds since January 1, 2000
	UINT32 reserved;               // reserved (zero)
	wchar_t name[256];             // String containing parent hard disk filename
	VHD_PARENT_LOCATOR entries[8]; // parent locator entries (only used by differencing hard disks)
	UINT8 reserved1[256];          // reserved (zero)
} VHD_DYNAMIC_HEADER, * PVHD_DYNAMIC_HEADER;

ENDPACK

BOOL isVHDPresent(_In_ PVDISK vdisk);
BSTATUS setVHDDriver(_Inout_ PVDISK vdisk);