#include "vdisk.h"
#include "console.h"

BOOL isValidEFI_GPTPartitionTable1(_In_ PGPT_PART_TABLE gpt) {
	if (gpt->signature != EFI_SIGNATURE) {
		dbgPrintf("Invalid EFI Signature\n\r");
		return FALSE;
	}
	if (gpt->revision != EFI_GPT_REVISION) {
		dbgPrintf("Invalid revision:0x%x (expected 0x10000)\n\r", gpt->revision);
		return FALSE;
	}
	if (gpt->header_size != sizeof(GPT_PART_TABLE)) {
		dbgPrintf("Invalid header size:0x%x (expected 0x5C)\n\r", gpt->header_size);
		return FALSE;
	}
	if (gpt->reserved != 0) {
		dbgPrintf("Invalid reserved value:0x%x (must be zero)\n\r", gpt->reserved);
		return FALSE;
	}
	if (gpt->current_lba != 1) {
		dbgPrintf("Invalid current header position:0x%llx (expected 1)\n\r", gpt->current_lba);
		return FALSE;
	}
	if (gpt->parts_start_lba != 2) {
		dbgPrintf("Invalid partition array starting lba:0x%llx (expected 2)\n\r", gpt->parts_start_lba);
		return FALSE;
	}
	if (gpt->n_parts != 0x80) {
		dbgPrintf("Invalid number of partitions:0x%x (expected 0x80)\n\r", gpt->n_parts);
		return FALSE;
	}
	if (gpt->part_ent_size != sizeof(GPT_PART_ENTRY)) {
		dbgPrintf("Invalid partition entry size:0x%x (expected 0x80)\n\r", gpt->part_ent_size);
		return FALSE;
	}
	return TRUE;
}

BOOL isValidEFI_GPTPartitionTable2(_In_ PGPT_PART_TABLE gpt, _In_ PGPT_PART_TABLE orig_gpt) {
	if (gpt->signature != EFI_SIGNATURE) {
		dbgPrintf("Invalid EFI Signature\n\r");
		return FALSE;
	}
	if (gpt->revision != EFI_GPT_REVISION) {
		dbgPrintf("Invalid revision:0x%x (expected 0x10000)\n\r", gpt->revision);
		return FALSE;
	}
	if (gpt->header_size != sizeof(GPT_PART_TABLE)) {
		dbgPrintf("Invalid header size:0x%x (expected 0x5C)\n\r", gpt->header_size);
		return FALSE;
	}
	if (gpt->reserved != 0) {
		dbgPrintf("Invalid reserved value:0x%x (must be zero)\n\r", gpt->reserved);
		return FALSE;
	}
	if (gpt->current_lba != orig_gpt->backup_lba) {
		dbgPrintf("Invalid current header position:0x%llx (expected: %llx)\n\r", gpt->current_lba, orig_gpt->backup_lba);
		return FALSE;
	}
	if (gpt->backup_lba != 0x1) {
		dbgPrintf("Invalid backup lba in backup header:%llx (expected 1)\n\r", gpt->backup_lba);
		return FALSE;
	}
	if (gpt->first_lba != orig_gpt->first_lba) {
		dbgPrintf("Missmatch in partition starting lba\n\r");
		return FALSE;
	}
	if (gpt->last_lba != orig_gpt->last_lba) {
		dbgPrintf("Missmatch in partition ending lba\n\r");
		return FALSE;
	}
	if (memcmp(&(gpt->disk_guid), &(orig_gpt->disk_guid), 0x10) != 0) {
		dbgPrintf("Disk GUID missmatch\n\r");
		return FALSE;
	}
	if (gpt->parts_start_lba != orig_gpt->backup_lba - 0x20) {
		dbgPrintf("Invalid partition array starting lba:0x%llx (expected 2)\n\r", gpt->parts_start_lba);
		return FALSE;
	}
	if (gpt->n_parts != 0x80) {
		dbgPrintf("Invalid number of partitions:0x%x (expected 0x80)\n\r", gpt->n_parts);
		return FALSE;
	}
	if (gpt->part_ent_size != sizeof(GPT_PART_ENTRY)) {
		dbgPrintf("Invalid partition entry size:0x%x (expected 0x80)\n\r", gpt->part_ent_size);
		return FALSE;
	}
	return TRUE;
}

BSTATUS calcPartitionTableChecksums(_In_ PGPT_PART_TABLE gpt, _In_ PVDISK vdisk, _Inout_ uint32_t* header_checksum, _Inout_ uint32_t* partition_table) {
	uint64_t length = (uint64_t)(gpt->part_ent_size) * (uint64_t)(gpt->n_parts);
	uint64_t offset = gpt->parts_start_lba << 9;
	*partition_table = 0;
	*header_checksum = 0;
	void* table = VirtualAlloc(0, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (table == 0) {
		errPrintf("Error: VirtualAlloc failed:0x%x\n\r", GetLastError());
		return FALSE;
	}
	if (!(vdisk->driver->read(vdisk, offset, length, table))) {
		return FALSE;
	}
	*partition_table = CRC32(table, length);
	VirtualFree(table, 0, MEM_RELEASE);
	uint32_t temp1 = gpt->hdr_checksum;
	uint32_t temp2 = gpt->part_entries_checksum;
	gpt->hdr_checksum = 0;
	gpt->part_entries_checksum = *partition_table;
	*header_checksum = CRC32(gpt, 0x5C);
	gpt->hdr_checksum = temp1;
	gpt->part_entries_checksum = temp2;
	return TRUE;
}

BOOL isGPTPresent(_In_ PVDISK vdisk) {
	if (vdisk == 0 || ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0)) {
		errPrintf("Invalid VDISK!\n\r");
		return FALSE;
	}
	if (mapVDISKFile(vdisk)) {
		if (vdisk->length >= 0x8600) {
			PMBR_HEAD mbr = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (!mbr) {
				errPrintf("Virtual Alloc failed!\n\r");
				return FALSE;
			}
			if (!(vdisk->driver->read(vdisk, 0, 0x400, mbr))) {
				errPrintf("Error reading VDISK!\n\r");
				goto _gpt_exit;
			}

			// check if mbr is valid for gpt
			int index = 4;
			if (mbr->e[0].part_type == 0xee) {
				index = 0;
			}
			else if ((((uint64_t*)(&(mbr->e[0])))[0] != 0) || (((uint64_t*)(&(mbr->e[0])))[1] != 0)) {
				dbgPrintf("Invalid mbr entry 1\n\r");
				goto _gpt_exit;
			}
			if (mbr->e[1].part_type == 0xee) {
				if (index == 4) {
					dbgPrintf("Too many GPT entries in MBR\n\r");
					goto _gpt_exit;
				}
				index = 1;
			}
			else if ((((uint64_t*)(&(mbr->e[0])))[2] != 0) || (((uint64_t*)(&(mbr->e[0])))[3] != 0)) {
				dbgPrintf("Invalid mbr entry 2\n\r");
				goto _gpt_exit;
			}
			if (mbr->e[2].part_type == 0xee) {
				if (index == 4) {
					dbgPrintf("Too many GPT entries in MBR\n\r");
					goto _gpt_exit;
				}
				index = 2;
			}
			else if ((((uint64_t*)(&(mbr->e[0])))[4] != 0) || (((uint64_t*)(&(mbr->e[0])))[5] != 0)) {
				dbgPrintf("Invalid mbr entry 3\n\r");
				goto _gpt_exit;
			}
			if (mbr->e[3].part_type == 0xee) {
				if (index == 4) {
					dbgPrintf("Too many GPT entries in MBR\n\r");
					goto _gpt_exit;
				}
				index = 3;
			}
			else if ((((uint64_t*)(&(mbr->e[0])))[6] != 0) || (((uint64_t*)(&(mbr->e[0])))[7] != 0)) {
				dbgPrintf("Invalid mbr entry 4\n\r");
				goto _gpt_exit;
			}
			if (index == 4) {
				dbgPrintf("No GPT entry found in MBR\n\r");
				goto _gpt_exit;
			}
			if (mbr->e[index].attrib != 0) {
				dbgPrintf("Invalid Attribute for GPT entry!\n\r");
			}
			if (mbr->e[index].chs_start[0] != 0 || mbr->e[index].chs_start[1] != 2 || mbr->e[index].chs_start[2] != 0 || mbr->e[index].lba_start != 1) {
				dbgPrintf("Invalid start address!\n\r");
				goto _gpt_exit;
			}

			// check first header
			PGPT_PART_TABLE gpt = (PGPT_PART_TABLE)(mbr + 1);
			if (!isValidEFI_GPTPartitionTable1(gpt)) {
				goto _gpt_exit;
			}
			uint32_t checksum1 = 0;
			uint32_t checksum2 = 0;
			if (!calcPartitionTableChecksums(gpt, vdisk, &checksum1, &checksum2)) {
				goto _gpt_exit;
			}
			if (checksum1 != gpt->hdr_checksum) {
				dbgPrintf("Invalid header checksum\n\r");
				goto _gpt_exit;
			}
			if (checksum2 != gpt->part_entries_checksum) {
				dbgPrintf("Invalid table checksum\n\r");
				goto _gpt_exit;
			}
			
			// check second header
			uint64_t offset = gpt->backup_lba << 9;
			PGPT_PART_TABLE gpt1 = (PGPT_PART_TABLE)mbr;
			if (!(vdisk->driver->read(vdisk, offset, 0x200, gpt1))) {
				goto _gpt_exit;
			}
			if (!isValidEFI_GPTPartitionTable2(gpt1, gpt)) {
				goto _gpt_exit;
			}
			if (!calcPartitionTableChecksums(gpt1, vdisk, &checksum1, &checksum2)) {
				goto _gpt_exit;
			}
			if (checksum1 != gpt1->hdr_checksum) {
				dbgPrintf("Invalid header checksum\n\r");
				goto _gpt_exit;
			}
			if (checksum2 != gpt1->part_entries_checksum) {
				dbgPrintf("Invalid table checksum\n\r");
				goto _gpt_exit;
			}

			VirtualFree(mbr, 0, MEM_RELEASE);
			return TRUE;

		_gpt_exit:
			VirtualFree(mbr, 0, MEM_RELEASE);
			return FALSE;

		}
		else {
			dbgPrintf("Disk too small for GPT!\n\r");
			return FALSE;
		}
	}
	else {
		errPrintf("Detection not Successful!\n\r");
		return FALSE;
	}
}

BSTATUS getGPTPartitions(_In_ PVDISK vdisk, _Inout_ PPARTITION* parts, _Inout_ uint32_t* n_parts) {
	if (vdisk == 0 || ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0)) {
		errPrintf("Invalid VDISK!\n\r");
		return FALSE;
	}
	if ((vdisk->attributes & (VDISK_ATTRIBUTE_FLAG_GPT | VDISK_ATTRIBUTE_FLAG_MBR)) != VDISK_ATTRIBUTE_FLAG_GPT) {
		errPrintf("No GPT VDISK specified!\n\r");
		return FALSE;
	}
	// assume EFI formatted gpt
	PGPT_PART_ENTRY entries = VirtualAlloc(0, 0x4000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!entries) {
		errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
		return FALSE;
	}
	if (!vdisk->driver->read(vdisk, 0x400, 0x4000, entries)) {
		VirtualFree(entries, 0, MEM_RELEASE);
		return FALSE;
	}
	// max size is one page and at least a page gets allocated every time so no difference
	*parts = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!(*parts)) {
		errPrintf("VirtualAlloc failed:%x\n\r", GetLastError());
		VirtualFree(entries, 0, MEM_RELEASE);
		return FALSE;
	}
	*n_parts = 0;
	for (int i = 0; i < 0x80; i++) {
		if (!isZeroGUID(&(entries[i].part_type_guid))) {
			(*parts)[*n_parts].attributes = &(entries[i]);
			(*parts)[*n_parts].parent = vdisk;
			(*parts)[*n_parts].start = (entries[i].first_lba) << 9;
			(*parts)[*n_parts].length = (entries[i].last_lba - entries[i].first_lba + 1) << 9;
			(*n_parts)++;
		}
	}
	return TRUE;
}