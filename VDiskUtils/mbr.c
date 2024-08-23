#include "vdisk.h"
#include "console.h"

void* temp = 0;

BOOL isMBRPresent(_In_ PVDISK vdisk) {
	if (vdisk == 0 || ((vdisk->attributes & VDISK_ATTRIBUTE_FLAG_OPEN) == 0)) {
		errPrintf("Invalid VDISK!\n\r");
		return FALSE;
	}
	if (!temp) {
		temp = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!temp) {
			errPrintf("VirtualAlloc failed:%x", GetLastError());
			return FALSE;
		}
	}
	if (mapVDISKFile(vdisk)) {
		if (vdisk->length >= 0x200) {
			PMBR_HEAD mbr = temp;
			if (!vdisk->driver->read(vdisk, 0, 0x200, mbr)) {
				errPrintf("Read failed!\n\r");
				return FALSE;
			}
			if (mbr->boot_signature != BOOT_SIGNATURE) {
				dbgPrintf("Invalid Boot Signature!\n\r");
				return FALSE;
			}
			// MBR gives no way to identify it for sure so we return true
			return TRUE;
		}
		else {
			dbgPrintf("Disk too small for MBR!\n\r");
			return FALSE;
		}
	}
	else {
		errPrintf("Detection not Successful!\n\r");
		return FALSE;
	}
}