#pragma once

PACK

typedef struct _FAT_BPB {
	UINT8 jmp[3];
	char oem_id[8];
	UINT16 bytes_per_sector;     // normally 0x200
	UINT8 sectors_per_cluster;   // normally 1
	UINT16 n_reserved_sectors;   // 
	UINT8 n_fats;                // normally 2
	UINT16 n_root_entries;       // 
	UINT16 n_sectors;            // 
	UINT8 media_descriptor_type; // should be 0xf8 on modern systems
	UINT16 sectors_per_fat;      // 
	UINT16 n_sectors_per_track;  // normally 0x3f
	UINT16 n_heads;              // 0xff ?
	UINT32 n_hidden_sectors;     // 
	UINT32 n_large_sectors;      // 
} FAT_BPB, * PFAT_BPB;

typedef struct _FAT12 {
	FAT_BPB bpb;
	UINT8 drive_number;
	UINT8 flags;
	UINT8 signature;
	UINT32 volume_id;
	char volume_label[11];
	char sysid[8];
	UINT8 boot_code[448];
	UINT16 boot_signature;
} FAT12, * PFAT12;

typedef struct _FAT16 {
	FAT_BPB bpb;
	UINT8 drive_number;
	UINT8 flags;
	UINT8 signature;
	UINT32 volume_id;
	char volume_label[11];
	char sysid[8];
	UINT8 boot_code[448];
	UINT16 boot_signature;
} FAT16, * PFAT16;

typedef struct _FAT32 {
	FAT_BPB bpb;
	UINT32 sectors_per_fat;
	UINT16 flags;
	UINT16 fat_version;
	UINT32 root_sector;
	UINT16 fs_info_sector;
	UINT16 backup_boot_sector;
	UINT8 reserved[12];
	UINT8 drive_number;
	UINT8 flags1;
	UINT8 signature;
	UINT32 volume_id;
	UINT8 volume_label[11];
	char system_id[8];
	UINT8 boot_code[420];
	UINT16 boot_signature;
} FAT32, * PFAT32;

typedef union _FAT_BS {
	FAT_BPB bpb;
	FAT12 fat12;
	FAT16 fat16;
	FAT32 fat32;
} FAT_BS, * PFAT_BS;

#define FAT_FSI_LEAD_SIGNATURE  0x41615252
#define FAT_FSI_SIGNATURE       0x61417272
#define FAT_FSI_TRAIL_SIGNATURE 0xAA550000

typedef struct _FAT_FS_INFO {
	UINT32 lead_signature;   // FAT_FSI_LEAD_SIGNATURE
	UINT8 reserved[480];     // zero
	UINT32 signature;        // FAT_FSI_SIGNATURE
	UINT32 c_free_cluster;   // number of free clusters
	UINT32 c_start_loooking; // first cluster to start looking for free clusters
	UINT8 reserved1[12];     // zero
	UINT32 trail_signature;  // FAT_FSI_TRAIL_SIGNATURE
} FAT_FS_INFO, * PFAT_FS_INFO;

typedef struct _EXFAT {
	UINT8 jmp[3];
	char eom_id[8];
	UINT8 zero[53];
	UINT64 part_offset;
	UINT64 volume_length;
	UINT32 fat_offset;
	UINT32 fat_length;
	UINT32 cluster_heap_offset;
	UINT32 cluster_count;
	UINT32 root_cluster;
	UINT32 serial_number;
	UINT16 revision;
	UINT16 flags;
	UINT8 sector_shift;
	UINT8 cluster_shit;
	UINT8 n_fats;
	UINT8 drive_select;
	UINT8 use_precentage;
	UINT8 reserved[7];
} EXFAT, * PEXFAT;

#define FAT_ATTRIB_READ_ONLY			0x01 
#define FAT_ATTRIB_HIDDEN				0x02
#define FAT_ATTRIB_SYSTEM				0x04
#define FAT_ATTRIB_VOLUME_ID			0x08
#define FAT_ATTRIB_DIRECTORY			0x10
#define FAT_ATTRIB_ARCHIVE				0x20
#define FAT_ATTRIB_LFN					(FAT_ATTRIB_READ_ONLY | FAT_ATTRIB_HIDDEN | FAT_ATTRIB_SYSTEM | FAT_ATTRIB_VOLUME_ID)

typedef struct _FAT_DIR83 {
	char filename[8];          // 8 filename chars
	char extension[3];         // extension
	UINT8 attribute;           // entry attribute
	UINT8 reserved;            // reserved for windows NT
	UINT8 creation_seconds;    // specs say 10th second reality 100th of second (range 0-199)
	UINT16 creation_time;      // time of creation (bits: 0-4: seconds/2 (0-29), 5-10: minutes (0-60), 11-15: hours (0-23))
	UINT16 creation_date;      // date of creation (bits: 0-4: day (0-31), 5-8: month (1-12), 9-15: year (0 = 1980))
	UINT16 last_accessed_date; // date of last access (see creation date)
	UINT16 cluster_high;       // FAT32: high 2 bytes for cluster number, fat12/16: should be zero
	UINT16 last_mod_time;      // time of last modifiaction (see creation time)
	UINT16 last_mod_date;      // date of last modifiaction (see creation date)
	UINT16 cluster_low;        // lower 2 bytes for cluster number
	UINT32 filesize;           // size of the file in bytes
} FAT_DIR83, * PFAT_DIR83;

#define SEQ_NUM_MASK 0x1F
#define SEQ_FLAG_FIRST_LAST 0x40

typedef struct _FAT_DIR_LONG {
	UINT8 seq;             // sequence number (bits: 0-4: number, 5: reserved, 6: last logical / first physical, 7: reserved)
	UINT16 first_chars[5]; // name characters
	UINT8 attrib;          // attribute of entry: always 0x0F for LFN
	UINT8 type;            // long entry type
	UINT8 checksum;        // short filename checksum
	UINT16 next_chars[6];  // name characters
	UINT16 zero;           // pad/reserved
	UINT16 final_chars[2]; // name characters
} FAT_DIR_LONG, * PFAT_DIR_LONG;

#define EXFAT_ENTRY_TYPE_ENTRY			0x85
#define EXFAT_ENTRY_TYPE_STREAM			0xC0
#define EXFAT_ENTRY_TYPE_NAME			0xC1

typedef struct _EXFAT_DIR {
	UINT8 type;
	UINT8 secondary_count;
	UINT16 checksum;
	UINT16 attribs;
	UINT16 reserved;
	UINT32 creation_date_time;
	UINT32 modified_date_time;
	UINT32 access_date_time;
	UINT8 create_millis;
	UINT8 modified_millis;
	UINT8 create_utc;
	UINT8 modified_utc;
	UINT8 access_utc;
	UINT8 reserved1[7];
} EXFAT_DIR, * PEXFAT_DIR;

typedef struct _EXFAT_STREAM_EXTENSION {
	UINT8 type;
	UINT8 flags;
	UINT8 reserved;
	UINT8 name_length;
	UINT16 name_hash;
	UINT16 reserved1;
	UINT64 v_data_length;
	UINT32 reserved2;
	UINT32 first_cluster;
	UINT64 data_length;
} EXFAT_STREAM_EXTENSION, * PEXFAT_STREAM_EXTENSION;

typedef struct _EXFAT_NAME {
	UINT8 type;
	UINT8 flags;
	UINT16 name[15];
} EXFAT_NAME, * PEXFAT_NAME;

typedef struct _EXFAT_LONG_NAME {
	UINT8 seq;
	UINT16 first_chars[5];
	UINT8 attrib;
	UINT8 long_type;
	UINT8 checksum;
	UINT16 next_name[6];
	UINT16 zero;
	UINT16 last_chars[2];
} EXFAT_LONG_NAME, * PEXFAT_LONG_NAME;

ENDPACK

#define INVALID_FAT_CHAR(c) ((((c) <= 0x20) && ((c) != 0x5)) /* Control chars are invalid except 0x05 as replacement for 0xE5 */ || \
							((c) > 0xff) /* UTF-16 chars are invalid */ || \
							((c) == 0x7C) /*pipe char is invalid*/ || \
							((c) == 0x7F) /* DEl char is invalid? (not sure but forbidden for safety) */ || \
							(((c) < 0x60) && (((0x38000000FC00DC04 >> (((unsigned char)(c)) - 0x20)) & 1) != 0)) /* magic for all invalid characters between 0x20 and 0x60: 0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D */ )

BSTATUS createFATDriver(_In_ PVDISK vdisk, _In_opt_ UINT32 partition_index, _Out_ PFS_DRIVER* driver);