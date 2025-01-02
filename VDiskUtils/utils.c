#include "vdisk.h"
#include "console.h"

uint32_t CRCTable[256];

void calcTable() {
	memset(CRCTable, 0, 1024);
	uint32_t crc = 1;
	int i = 128;
	do {
		if (crc & 1) {
			crc = (crc >> 1) ^ 0xEDB88320;
		}
		else {
			crc >>= 1;
		}
		for (int j = 0; j < 256; j += i << 1) {
			CRCTable[i + j] = crc ^ CRCTable[j];
		}
		i >>= 1;
	} while (i > 0);
}

uint32_t CRC32(_In_ const void* data, _In_ size_t data_length) {
	const uint8_t* dta = data;
	uint32_t crc32 = 0xFFFFFFFF;
	for (size_t i = 0; i < data_length; i++) {
		const uint32_t lookupIndex = (crc32 ^ dta[i]) & 0xff;
		crc32 = (crc32 >> 8) ^ CRCTable[lookupIndex];
	}
	crc32 = ~crc32;
	return crc32;
}

CHS toCHS(_In_opt_ size_t lba) {
	CHS out = { 0 };
	if (lba > (size_t)0x0FEFF010) {
		out.lchs = 0xffffffff;
		return out;
	}
	unsigned long cth = 0;
	if (lba >= (size_t)0x3EFFC10) {
		out.lchs = 0xff100000;
		cth = (unsigned long)(lba >> 0x4);
	}
	else {
		out.chs.S = 0x11;
		cth = (unsigned long)(lba / 0x11);
		out.chs.H = (unsigned char)((cth + 1023) >> 10);
		if (out.chs.H < 4) {
			out.chs.H = 4;
		}
		if ((cth >= ((unsigned long)out.chs.H << 10)) || (out.chs.H > (unsigned char)16)) {
			out.lchs = 0x1f100000;
			cth = (unsigned long)(lba / 0x1f);
		}
		if (cth >= ((unsigned long)out.chs.H << 10)) {
			out.lchs = 0x3f100000;
			cth = (unsigned long)(lba / 0x3f);
		}
	}
	out.chs.C = (unsigned short)(cth / (unsigned long)out.chs.H);
	return out;
}

void randGUID(_Inout_ GUID* guid) {
	short* arr = (short*)guid;
	for (int i = 0; i < 8; i++) {
		arr[i] ^= rand() & 0xffff;
	}
}

BSTATUS StrFromGUID(_Inout_ PWSTR buf, _In_ int buffer_length, _In_ const GUID* guid) {
	return StringFromGUID2((const IID*)guid, (LPOLESTR)buf, buffer_length) != 0;
}

BSTATUS GUIDFromStr(_Inout_ GUID* guid, _In_ LPCWSTR str) {
	return CLSIDFromString((LPOLESTR)str, (LPCLSID)guid) == 0;
} 

BOOL isZeroGUID(_In_ const GUID* guid) {
	const uint8_t* data = (const uint8_t*)guid;
	for (int i = 0; i < 16; i++) {
		if (data[i] != 0)return FALSE;
	}
	return TRUE;
}

BSTATUS setEOF(_In_ HANDLE file, _In_ UINT64 length) {
	NTSTATUS status;
	IO_STATUS_BLOCK block = { 0 };
	UINT64 data = length;
	status = NtSetInformationFile(file, &block, &data, 8, 0x14);
	if (status < 0) {
		SetLastError(status);
		return FALSE;
	}
	data = length;
	status = NtSetInformationFile(file, &block, &data, 8, 0x13);
	if (status < 0) {
		SetLastError(status);
		return FALSE;
	}
	return TRUE;
}

LPWSTR __inline storePath(_In_ const LPCWSTR str) {
	size_t len = (wcslen(str) + 1) << 1; // len + 1 since we want to include the terminating null character
	LPWSTR out = HeapAlloc(proc_heap, 0, len);
	if (out == 0) {
		errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
		return 0;
	}
	memcpy(out, str, len);
	return out;
}

void __inline deletePath(_In_ LPWSTR path) {
	size_t len = wcslen(path);
	memset(path, 0, len);
	HeapFree(proc_heap, 0, path);
}

#pragma warning(disable : 6054) // buffer cannot overrun since content of string is constant
#pragma warning(disable : 6386) // 

BSTATUS parsePath(_In_ LPCWSTR usr_path, _Out_ LPWSTR* path, _Out_ LPWSTR** tokens, _Out_ LPDWORD n_tokens) {
	LPWSTR pth = storePath(usr_path);
	if (pth == 0) {
		return FALSE;
	}
	LPWSTR token_buf = storePath(usr_path);
	if (token_buf == 0) {
		deletePath(pth);
		return FALSE;
	}
	size_t buflen = wcslen(token_buf) << 1;
	LPWSTR pt = token_buf;
	DWORD n_tok = 0;
	while (*pt != 0) {
		if (*pt == L'/') {
			++n_tok;
		}
		++pt;
	}
	if (*pt != '/') {
		++n_tok;
	}
	LPWSTR* toks = HeapAlloc(proc_heap, 0, ((size_t)n_tok) << 3);
	if (toks == 0) {
		deletePath(pth);
		deletePath(token_buf);
		return FALSE;
	}
	pt = token_buf;
	LPWSTR lt = token_buf;
	n_tok = 0;
	while (*pt != 0) {
		if (*pt == L'/') {
			*pt = 0;
			toks[n_tok] = lt;
			++pt;
			++n_tok;
			lt = pt;
		}
		else {
			++pt;
		}
	}
	if (*lt != 0) {
		toks[n_tok] = lt;
		++n_tok;
	}
	*path = pth;
	*tokens = toks;
	*n_tokens = n_tok;
	return TRUE;
}