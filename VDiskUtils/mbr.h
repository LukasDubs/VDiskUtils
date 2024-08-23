#pragma once

#define BOOT_SIGNATURE 0xAA55

PACK

typedef struct _MBR_PART_ENTRY {
	UINT8 attrib;       // bit 7 is active bit, others are reserved (make MBR invalid on older MBRs, newer MBRs store stuff in those)
	UINT8 chs_start[3]; // chs address of start sector
	UINT8 part_type;    // partition type
	UINT8 chs_end[3];   // chs of last sector
	UINT32 lba_start;   // lba of start sector
	UINT32 part_length; // length of partition in sectors
} MBR_PART_ENTRY, * PMBR_PART_ENTRY;

typedef struct _MBR_HEAD {
	UINT8 boot_sector[440]; // possibly boot code
	UINT32 udid;            // optional uid
	UINT16 reserved;        // reserved but could contain special signatures
	MBR_PART_ENTRY e[4];    // partition entries
	UINT16 boot_signature;  // must be present, all the other fields are optional
} MBR_HEAD, * PMBR_HEAD;

ENDPACK

BOOL isMBRPresent(_In_ PVDISK vdisk);