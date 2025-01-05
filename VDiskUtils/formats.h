#pragma once

// quality of life macros
#define PACK _Pragma("pack(push, 1)")
#define ENDPACK _Pragma("pack(pop)")

// guid is required for gpt partitioning
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;
#endif

// vdisk formats
#include "vhd.h"
#include "vhdx.h"

// partitioning formats
#include "mbr.h"
#include "gpt.h"

// filesystem formats
#include "fat.h"
#include "ntfs.h"