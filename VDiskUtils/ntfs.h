#pragma once

/*
* NFST-VOL:
* ##################################################################################
* #            #       #                    #                  #                   #
* #    BOOT    #  MFT  #     Free Space     #       Data       #     FreeSpace     #
* #            #       #                    #                  #                   #
* ##################################################################################
*/

/*
* bootsector looks like fat but not quite ???
* diskpart example starts at 0x10000000
*/

PACK

#define NTFS_BOOT_OEM_ID    0x2020205346544E
#define NTFS_BOOT_SIGNATURE 0x00800080

typedef struct _NTFS_BOOT {
	UINT8 jump[3];
	union {
		char oem_id[8];
		UINT64 oem_int;
	};
	UINT16 bytes_per_setor;
	UINT8 sectors_per_cluster;
	UINT8 unused1[7];
	UINT8 media_desc;
	UINT8 unused2[2];
	UINT16 sectors_per_track;
	UINT16 number_of_heads;
	UINT8 unused3[8];
	UINT32 signature;
	UINT64 number_of_sectors;
	UINT64 lcn_of_mft;
	UINT64 lcn_of_mftmirr;
	UINT32 cluster_per_mft;
	UINT32 cluster_per_ind;
	UINT64 vol_serial_num;
	UINT8 code[430];
	UINT16 boot_signature;
} NTFS_BOOT, * PNTFS_BOOT;

#define NTFS_STANDARD_INFORMATION_ID 0x10
#define NTFS_ATTRIBUTE_LIST_ID 0x20
#define NTFS_FILE_NAME_ID 0x30

typedef struct _NTFS_STD_ATTRIB_HEADER {
	UINT32 id;
	UINT32 length;
	UINT8 non_resident_flag;
	UINT8 name_length;
	UINT16 name_offset;
	UINT16 flags;
	UINT16 attribute_id;
	UINT32 attrib_length;
	UINT16 attrib_offset;
	UINT8 index_flag;
	UINT8 pad;
} NTFS_STD_ATTRIB_HEADER, * PNTFS_STD_ATTRIB_HEADER;


#define NTFS_DOS_PERM_READ_ONLY     0x0001
#define NTFS_DOS_PERM_HIDDEN        0x0002
#define NTFS_DOS_PERM_SYSTEM        0x0004
#define NTFS_DOS_PERM_ARCHIVE       0x0020
#define NTFS_DOS_PERM_DEVICE        0x0040
#define NTFS_DOS_PERM_NORMAL        0x0080
#define NTFS_DOS_PERM_TEMP          0x0100
#define NTFS_DOS_PERM_SPARSE        0x0200
#define NTFS_DOS_PERM_REPARSE_POINT 0x0400
#define NTFS_DOS_PERM_COMPRESSED    0x0800
#define NTFS_DOS_PERM_OFFILNE       0x1000
#define NTFS_DOS_PERM_NOT_INDEXED   0x2000
#define NTFS_DOS_PERM_ENCRYPTED     0x4000

typedef struct _NTFS_STD_ATTRIBUTE {
	NTFS_STD_ATTRIB_HEADER header;
	UINT64 creation_time;
	UINT64 altered_time;
	UINT64 mft_changed_time;
	UINT64 read_time;
	UINT32 dos_perms;
	UINT32 max_n_versions;
	UINT32 version_n;
	UINT32 class_id;
#ifdef WIN2K
	UINT32 owner_id;
	UINT32 security_id;
	UINT64 quota_charged;
	UINT64 update_sequence_number;
#endif
} NTFS_STD_ATTRIBUTE, * PNTFS_STD_ATTRIBUTE;

typedef struct _NTFS_ATTRIBUTE_LIST {
	NTFS_STD_ATTRIB_HEADER header;
	UINT32 type;
	UINT16 record_length;
	UINT8 name_length;
	UINT8 name_offset;
	UINT64 starting_vcn;
	UINT64 base_file_ref;
} NTFS_ATTRIBUTE_LIST, * PNTFS_ATTRIBUTE_LIST;

#define NTFS_FILE_FLAG_READ_ONLY     0x0001
#define NTFS_FILE_FLAG_HIDDEN        0x0002
#define NTFS_FILE_FLAG_SYSTEM        0x0004
#define NTFS_FILE_FLAG_ARCHIVE       0x0020
#define NTFS_FILE_FLAG_DEVICE        0x0040
#define NTFS_FILE_FLAG_NORMAL        0x0080
#define NTFS_FILE_FLAG_TEMP          0x0100
#define NTFS_FILE_FLAG_SPARSE        0x0200
#define NTFS_FILE_FLAG_REPARSE_POINT 0x0400
#define NTFS_FILE_FLAG_COMPRESSED    0x0800
#define NTFS_FILE_FLAG_OFFILNE       0x1000
#define NTFS_FILE_FLAG_NOT_INDEXED   0x2000
#define NTFS_FILE_FLAG_ENCRYPTED     0x4000
#define NTFS_FILE_FLAG_DIRECTORY     0x10000000
#define NTFS_FILE_FLAG_INDEX_VIEW    0x20000000

typedef struct _NTFS_FILE_NAME {
	NTFS_STD_ATTRIB_HEADER header;
	UINT64 parent_dir_file_ref;
	UINT64 creation_time;
	UINT64 altered_time;
	UINT64 mft_changed_time;
	UINT64 read_time;
	UINT64 file_alloc_size;
	UINT64 file_size;
	UINT32 flags;
	UINT32 used;
	UINT8 n_chars;
	UINT8 namespace;
} NTFS_FILE_NAME, * PNTFS_FILE_NAME;

ENDPACK