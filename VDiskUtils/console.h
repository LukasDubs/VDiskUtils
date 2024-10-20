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
BOOL rdCon(_Out_ PVOID buffer, _In_ DWORD n_chars_to_read, _Out_ LPDWORD n_chars_read);
void cls(int id);

#define CMD_STATUS_EXIT 0
#define CMD_STATUS_SUCCESS 1
#define CMD_STATUS_FS_CONTEXT 2
#define CMD_STATUS_CMD_CONTEXT -1

int execCmd(LPWSTR* args, size_t argc);
int execFsCmd(LPWSTR* args, size_t argc);