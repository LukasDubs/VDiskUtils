#pragma once

#define PACK _Pragma("pack(push, 1)")
#define ENDPACK _Pragma("pack(pop)")

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;
#endif

#include "vhd.h"
#include "vhdx.h"
#include "mbr.h"
#include "gpt.h"
#include "fat.h"
#include "ntfs.h"