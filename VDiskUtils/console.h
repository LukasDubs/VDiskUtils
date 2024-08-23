#pragma once

#include <windows.h>
#include <stdio.h>

#define MAX_INPUT_CHARS 0x10000

#define CON_USR FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define CON_PMT FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define CON_EXE FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define CON_ERR FOREGROUND_RED | FOREGROUND_INTENSITY
#define CON_DBG FOREGROUND_RED | FOREGROUND_GREEN

BOOL initVDISKs();
void calcTable();
extern HANDLE proc_heap;

void exePrintf(_In_z_ _Printf_format_string_ const char* _Format, ...);
void dbgPrintf(_In_z_ _Printf_format_string_ const char* _Format, ...);
void errPrintf(_In_z_ _Printf_format_string_ const char* _Format, ...);
void cls(int id);

BOOL execCmd(LPWSTR* args, size_t argc);