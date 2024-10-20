#pragma once

// info from https://flatcap.github.io/linux-ntfs/ntfs/index.html
// and from https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20(NTFS).asciidoc

/*
* NFST-VOL:
* ##################################################################################
* #            #       #                    #                  #                   #
* #    BOOT    #  MFT  #     Free Space     #       Data       #     FreeSpace     #
* #            #       #                    #                  #                   #
* ##################################################################################
*/

/*
* diskpart example starts at 0x10000
*/

// if nothing specified use win2k and not winnt for ntfs
#ifndef OLD_NTFS
#define NTFS3
#endif

PACK

#define NTFS_BOOT_OEM_ID    0x202020205346544E
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

#define NTFS_FILE_RECORD_MAGIC 0x454C4946
#define NTFS_FILE_RECORD_FLAG_IN_USE 0x1
#define NTFS_FILE_RECORD_FLAG_IS_DIR 0x2
#define NTFS_FILE_RECORD_FLAG_IN_$EXTEND 0x4
#define NTFS_FILE_RECORD_FLAG_IS_INDEX 0x8

typedef struct _NTFS_FILE_RECORD {
	UINT32 magic;
	UINT16 update_sequence_offset;
	UINT16 update_sequence_size;
	UINT64 log_file_sequence_number;
	UINT16 sequence_number;
	UINT16 hard_link_count;
	UINT16 attribs_offset;
	UINT16 flags;
	UINT32 file_record_size;
	UINT32 file_record_alloc_size;
	UINT64 base_file_ref;
	UINT16 next_attrib_id;
#ifdef NTFS3
	UINT16 align;
	UINT32 mft_record_number;
#endif
} NTFS_FILE_RECORD, * PNTFS_FILE_RECORD;

#define NTFS_STANDARD_INFORMATION_ID 0x10
#define NTFS_ATTRIBUTE_LIST_ID 0x20
#define NTFS_FILE_NAME_ID 0x30
#ifdef NTFS3
#define NTFS_OBJECT_ID_ID 0x40
#else
#define NTFS_VOLUME_VERSION 0x40
#endif
#define NTFS_SECURITY_DESCRIPTOR_ID 0x50
#define NTFS_VOLUME_NAME_ID 0x60
#define NTFS_VOLUME_INFORMATION_ID 0x70
#define NTFS_DATA_ID 0x80
#define NTFS_INDEX_ROOT_ID 0x90
#define NTFS_INDEX_ALLOCATION_ID 0xA0
#define NTFS_BITMAP_ID 0xB0
#ifdef NTFS3
#define NTFS_REPARSE_POINT_ID 0xC0
#else
#define NTFS_SYMBOLIC_LINK_ID 0xC0
#endif
#define NTFS_EA_INFORMATION_ID 0xD0
#define NTFS_EA_ID 0xE0
#ifndef NTFS3
#define NTFS_PROPERTY_SET_ID 0xF0
#endif
#ifdef NTFS3
#define NTFS_LOGGED_UTILITY_STREAM_ID 0x100
#endif
#define NTFS_END_MARKER 0xFFFFFFFF

#define NTFS_STD_ATTRIB_FLAG_COMPRESSED 0x0001
#define NTFS_STD_ATTRIB_MASK_COMPRESSION 0x00ff
#define NTFS_STD_ATTRIB_FLAG_ENCRYPTED 0x4000
#define NTFS_STD_ATTRIB_FLAG_SPARSE 0x8000

typedef struct _NTFS_STD_ATTRIB_RESIDENT_FOOTER {
	UINT32 attrib_length;
	UINT16 attrib_offset;
	UINT8 index_flag;
	UINT8 pad;
} NTFS_STD_ATTRIB_RESIDENT_FOOTER, * PNTFS_STD_ATTRIB_RESIDENT_FOOTER;

typedef struct _NTFS_STD_ATTRIB_NON_RESIDENT_FOOTER {
	UINT64 start_vcn;
	UINT64 last_vcn;
	UINT16 data_runs_offset;
	UINT16 compressed_unit_size;
	UINT32 pad;
	UINT64 alloc_size;
	UINT64 real_size;
	UINT64 initialized_data_size;
} NTFS_STD_ATTRIB_NON_RESIDENT_FOOTER, * PNTFS_STD_NON_ATTRIB_RESIDENT_FOOTER;

typedef struct _NTFS_STD_ATTRIB_HEADER {
	UINT32 id;
	UINT32 length;
	UINT8 non_resident_flag; // 0x00
	UINT8 name_length;
	UINT16 name_offset;
	UINT16 flags;
	UINT16 attribute_id;
	union {
		NTFS_STD_ATTRIB_RESIDENT_FOOTER resident;
		NTFS_STD_ATTRIB_NON_RESIDENT_FOOTER non_resident;
	};
} NTFS_STD_ATTRIB_HEADER, * PNTFS_STD_ATTRIB_HEADER;

typedef struct _NTFS_STD_RESIDENT_ATTRIB_HEADER {
	UINT32 id;
	UINT32 length;
	UINT8 non_resident_flag; // 0x00
	UINT8 name_length;
	UINT16 name_offset;
	UINT16 flags;
	UINT16 attribute_id;
	UINT32 attrib_length;
	UINT16 attrib_offset;
	UINT8 index_flag;
	UINT8 pad;
} NTFS_STD_RESIDENT_ATTRIB_HEADER, * PNTFS_STD_RESIDENT_ATTRIB_HEADER;

typedef struct _NTFS_STD_NON_RESIDENT_ATTRIB_HEADER {
	UINT32 id;
	UINT32 length;
	UINT8 non_resident_flag; // 0x01
	UINT8 name_length;
	UINT16 name_offset;
	UINT16 flags;
	UINT16 attribute_id;
	UINT64 start_vcn;
	UINT64 last_vcn;
	UINT16 data_runs_offset;
	UINT16 compressed_unit_size;
	UINT32 pad;
	UINT64 alloc_size;
	UINT64 real_size;
	UINT64 initialized_data_size;
} NTFS_STD_NON_RESIDENT_ATTRIB_HEADER, * PNTFS_STD_NON_RESIDENT_ATTRIB_HEADER;

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
	UINT64 creation_time;
	UINT64 altered_time;
	UINT64 mft_changed_time;
	UINT64 read_time;
	UINT32 dos_perms;
	UINT32 max_n_versions;
	UINT32 version_n;
	UINT32 class_id;
#ifdef NTFS3
	UINT32 owner_id;
	UINT32 security_id;
	UINT64 quota_charged;
	UINT64 update_sequence_number;
#endif
} NTFS_STD_ATTRIBUTE, * PNTFS_STD_ATTRIBUTE;

typedef struct _NTFS_ATTRIBUTE_LIST {
	UINT32 type;
	UINT16 record_length;
	UINT8 name_length;
	UINT8 name_offset;
	UINT64 starting_vcn;
	UINT64 base_file_ref;
	UINT16 attrib_id;
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

#define NTFS_NAMESPACE_POSIX 0
#define NTFS_NAMESPACE_WINDOWS 1
#define NTFS_NAMESPACE_DOS 2
#define NTFS_NAMESPACE_DOS_WINDOWS 3

typedef struct _NTFS_FILE_NAME {
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

typedef struct _NTFS_OBJECT_ID {
	GUID object_id;
	GUID birth_vol_id;
	GUID birth_obj_id;
	GUID domain_id;
} NTFS_OBJECT_ID, * PNTFS_OBJECT_ID;


// slightly altered from windows headers

typedef struct _NTFS_SECURITY_DESCRIPTOR_HEADER {
	UINT8 revision;
	UINT8 padding;
	UINT16 control_flags;
	UINT32 user_sid_offset;
	UINT32 group_sid_offset;
	UINT32 sacl_sid_offset;
	UINT32 dacl_sid_offset;
} NTFS_SECURITY_DESCRIPTOR_HEADER, * PNTFS_SECURITY_DESCRIPTOR_HEADER;

typedef struct _NTFS_ACL {
	UINT8 AclRevision;
	UINT8 padding;
	UINT16 AclSize;
	UINT16 AceCount;
	UINT16 padding1;
} NTFS_ACL, * PNTFS_ACL;

//TODO: get all valid ACEs

typedef struct _NTFS_VOLUME_NAME {
	WCHAR str[ANYSIZE_ARRAY];
} NTFS_VOLUME_NAME, * PNTFS_VOLUME_NAME;

#define NTFS_VOLUME_INFO_FLAG_DIRTY               0x0001
#define NTFS_VOLUME_INFO_FLAG_RESIZE_LOG_FILE     0x0002
#define NTFS_VOLUME_INFO_FLAG_UPGRADE_ON_MOUNT    0x0004
#define NTFS_VOLUME_INFO_FLAG_MOUNTED_ON_NT4      0x0008
#define NTFS_VOLUME_INFO_FLAG_DELETE_USN_UNDERWAY 0x0010
#define NTFS_VOLUME_INFO_FLAG_REPAIR_OBJECT_IDS   0x0020

typedef struct _NTFS_VOLUME_INFORMATION {
	NTFS_STD_ATTRIB_HEADER hdr;
	UINT64 reserved;
	UINT8 major_ersion;
	UINT8 minor_version;
	UINT16 flags;
	UINT32 reserved1;
} NTFS_VOLUME_INFORMATION, * PNTFS_VOLUME_INFORMATION;

typedef struct _NTFS_INDEX_ROOT {
	UINT32 type;
	UINT32 collation_rule;
	UINT32 bytes_per_index_record;
	//?
	//TODO
} NTFS_INDEX_ROOT, * PNTFS_INDEX_ROOT;

#define NTFS_REPARSE_TYPE_IS_ALIAS 0x20000000
#define NTFS_REPARSE_TYPE_IS_HIGH_LATENCY 0x40000000
#define NTFS_REPARSE_TYPE_IS_MICROSOFT 0x80000000
#define NTFS_REPARSE_TYPE_NSS 0x68000005
#define NTFS_REPARSE_TYPE_NSS_RECOVER 0x68000006
#define NTFS_REPARSE_TYPE_SIS 0x68000007
#define NTFS_REPARSE_TYPE_DFS 0x68000008
#define NTFS_REPARSE_TYPE_MOUNT_POINT 0x88000003
#define NTFS_REPARSE_TYPE_HSM 0xA8000004
#define NTFS_REPARSE_TYPE_SYMBOLIC_LINK 0xE8000000

typedef struct _NTFS_REPARSE_POINT {
	UINT32 reparse_type;
	UINT16 reparse_data_length;
	UINT16 padding;
} NTFS_REPARSE_POINT, * PNTFS_REPARSE_POINT;

typedef struct _NTFS_REPARSE_POINT_DATA {
	UINT16 subst_name_offset;
	UINT16 subst_name_length;
	UINT16 print_name_offset;
	UINT16 print_name_length;
} NTFS_REPARSE_POINT_DATA, * PNTFS_REPARSE_POINT_DATA;

typedef struct _NTFS_EA_INFORMATION {
	UINT16 attrib_packed_size;
	UINT16 n_needed_ea;
	UINT32 attrib_unpacked_size;
} NTFS_EA_INFORMATION, * PNTFS_EA_INFORMATION;

#define NTFS_EA_NEED_EA 0x80

typedef struct _NTFS_EA {
	UINT32 next_offset;
	UINT8 flags;
	UINT8 name_length;
	UINT16 value_length;
} NTFS_EA, * PNTFS_EA;

ENDPACK