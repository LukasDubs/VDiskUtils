#pragma once

uint32_t CRC32(_In_ const void* data, _In_ size_t length);

CHS toCHS(_In_opt_ size_t lba);

BSTATUS StrFromGUID(_Inout_ PWSTR buf, _In_ int buffer_length, _In_ const GUID* guid);
BSTATUS GUIDFromStr(_Inout_ GUID* guid, _In_ LPCWSTR str);
void randGUID(_Inout_ GUID* guid);
BOOL isZeroGUID(_In_ const GUID* guid);

#pragma comment(lib, "ntdll.lib")
typedef struct _IO_STATUS_BLOCK {
	UINT64 reserved[2];
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;
extern NTSYSCALLAPI NTSTATUS NTAPI NtSetInformationFile(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_ PVOID FileInformation, _In_ ULONG Length, _In_ UINT FileInformationClass);

BSTATUS setEOF(_In_ HANDLE file, _In_ UINT64 length);