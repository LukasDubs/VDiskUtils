#pragma once

#define EFI_SIGNATURE 0x5452415020494645
#define EFI_SYSTEM_PARTITION "{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}"

#define EFI_GPT_REVISION 0x00010000

#define GPT_ENTRY_ATTRIB_PLATFORM_REQUIRED	0x0000000000000001
#define GPT_ENTRY_ATTRIB_EFI_IGNORE			0x0000000000000002
#define GPT_ENTRY_ATTRIB_LEGACY_BIOS		0x0000000000000004
#define GPT_ENTRY_ATTRIB_MASK_RESERVED		0x0000FFFFFFFFFFF8
#define GPT_ENTRY_ATTRIB_MASK_TYPE_SPECIFIC	0xFFFF000000000000

PACK

typedef struct _GPT_PART_TABLE {
	UINT64 signature;             // contains EFI_SIGNATURE
	UINT32 revision;              // revision number
	UINT32 header_size;           // size of this header
	UINT32 hdr_checksum;          // CRC32 of this header
	UINT32 reserved;              // zero
	UINT64 current_lba;           // lba of this partition table
	UINT64 backup_lba;            // lba of backup gpt part table
	UINT64 first_lba;             // first lba usable for partitions
	UINT64 last_lba;              // last lba usable for partitions
	GUID disk_guid;               // disk guid
	UINT64 parts_start_lba;       // starting lba of partition entry array
	UINT32 n_parts;               // number of partition entries
	UINT32 part_ent_size;         // size in bytes of partition entry (must be 2^(7+n) with n >= 0)
	UINT32 part_entries_checksum; // CRC32 of partition entry array
} GPT_PART_TABLE, * PGPT_PART_TABLE;

typedef struct _GPT_PART_ENTRY {
	GUID part_type_guid; // partition type guid
	GUID part_guid;      // partition guid
	UINT64 first_lba;    // first usable lba of the partition
	UINT64 last_lba;     // last lba usable of the partition
	UINT64 attribs;      // attributes of the partition
	wchar_t name[36];    // unicode name of the partition
} GPT_PART_ENTRY, * PGPT_PART_ENTRY;

ENDPACK

// parsing disk based on the EFI specs (makes it easier but with some limitations)
//		diskpart and other disk format utility will use the EFI specs as default so no problems should occur
BOOL isGPTPresent(_In_ PVDISK vdisk);
BSTATUS getGPTPartitions(_In_ PVDISK vdisk, _Inout_ PPARTITION* parts, _Inout_ uint32_t* n_parts);