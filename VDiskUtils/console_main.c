#include "console.h"

HANDLE con_in = INVALID_HANDLE_VALUE;
HANDLE con_out = INVALID_HANDLE_VALUE;
HANDLE proc_heap = 0;

void* outbuf = 0;

#define CLS L"\x1b[2J\x1b[H"

// forced cls
BOOL clsf() {
	CONSOLE_SCREEN_BUFFER_INFO buf;
	if (!GetConsoleScreenBufferInfo(con_out, &buf)) {
		return FALSE;
	}
	DWORD consz = buf.dwSize.X * buf.dwSize.Y;
	COORD origin = { 0, 0 };
	DWORD wrt = 0;
	if (!FillConsoleOutputCharacterW(con_out, (TCHAR)' ', consz, origin, &wrt)) {
		return FALSE;
	}
	if (!GetConsoleScreenBufferInfo(con_out, &buf)) {
		return FALSE;
	}
	if (!FillConsoleOutputAttribute(con_out, buf.wAttributes, consz, origin, &wrt)) {
		return FALSE;
	}
	return SetConsoleCursorPosition(con_out, origin);
}

// cmd like cls
BOOL clscmd() {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(con_out, &csbi)) {
		return FALSE;
	}

	SMALL_RECT rect = { 0 };
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = csbi.dwSize.X;
	rect.Bottom = csbi.dwSize.Y;

	COORD dest = { 0 };
	dest.X = 0;
	dest.Y = -csbi.dwSize.Y;

	CHAR_INFO fill = { 0 };
	fill.Char.UnicodeChar = L' ';
	fill.Attributes = csbi.wAttributes;
	ScrollConsoleScreenBufferW(con_out, &rect, NULL, dest, &fill);

	csbi.dwCursorPosition.X = 0;
	csbi.dwCursorPosition.Y = 0;
	return SetConsoleCursorPosition(con_out, csbi.dwCursorPosition);
}

// escape character based cls
BOOL clss() {
	DWORD wrt = 0;
	return WriteConsoleW(con_out, CLS, sizeof(CLS) >> 1, &wrt, 0);
}

void cls(int id) {
	if (id == 0) {
		clscmd();
	}
	else if (id == 1) {
		clss();
	}
	else if (id == 2) {
		clsf();
	}
	else {
		errPrintf("Invalid cls args!\n\r");
	}
}

// printf doesnt recognize my console so we flush manually
void flushConsole(int length) {
	DWORD wrt = 0;
	WriteConsoleA(con_out, outbuf, (DWORD)length, &wrt, 0);
}

void pmtPrint(LPCSTR str) {
	SetConsoleTextAttribute(con_out, CON_PMT);
	DWORD wrt = 0;
	size_t len = strlen(str);
	if (len > 0xffffffff) {
		// wtf you doin
		return;
	}
	WriteConsoleA(con_out, str, (DWORD)len, &wrt, 0);
}

//printf wrappers for correct color and console flush

void exePrintf(_In_z_ _Printf_format_string_ const char* _Format, ...) {
	int _Length;
	va_list _ArgList;
	__crt_va_start(_ArgList, _Format);
	_Length = _vsprintf_s_l(outbuf, 0x1000, _Format, NULL, _ArgList);
	SetConsoleTextAttribute(con_out, CON_EXE);
	flushConsole(_Length);
	__crt_va_end(_ArgList);
}

void dbgPrintf(_In_z_ _Printf_format_string_ const char* _Format, ...) {
	int _Length;
	va_list _ArgList;
	__crt_va_start(_ArgList, _Format);
	_Length = _vsprintf_s_l(outbuf, 0x1000, _Format, NULL, _ArgList);
	SetConsoleTextAttribute(con_out, CON_DBG);
	flushConsole(_Length);
	__crt_va_end(_ArgList);
}

void errPrintf(_In_z_ _Printf_format_string_ const char* _Format, ...) {
	int _Length;
	va_list _ArgList;
	__crt_va_start(_ArgList, _Format);
	_Length = _vsprintf_s_l(outbuf, 0x1000, _Format, NULL, _ArgList);
	SetConsoleTextAttribute(con_out, CON_ERR);
	flushConsole(_Length);
	__crt_va_end(_ArgList);
}

int WINAPI CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	LPWSTR cmdl = GetCommandLineW();
	//TODO parse args

	AllocConsole();

	con_out = GetStdHandle(STD_OUTPUT_HANDLE);
	con_in = GetStdHandle(STD_INPUT_HANDLE);
	outbuf = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	DWORD mode = 0;
	if (!GetConsoleMode(con_out, &mode)) {
		errPrintf("Err:%x\n\r", GetLastError());
	}
	else {
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(con_out, mode)) {
			errPrintf("Err:%x\n\r", GetLastError());
		}
	}

	proc_heap = GetProcessHeap();
	calcTable();
	if (!initVDISKs())return GetLastError();

	const LPWSTR o_cmdbuf = VirtualAlloc(0, MAX_INPUT_CHARS << 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!o_cmdbuf) {
		return GetLastError();
	}
	DWORD read;
	LPWSTR* args = VirtualAlloc(0, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!args) {
		return GetLastError();
	}
	while (1) {
		pmtPrint(">>");
		SetConsoleTextAttribute(con_out, CON_USR);
		read = 0;
		LPWSTR cmdbuf = o_cmdbuf;
		if (ReadConsoleW(con_in, cmdbuf, MAX_INPUT_CHARS, &read, 0)) {
			if (read == 0)break;
			if (read > 1) {
				if (cmdbuf[read - 1] == '\n') cmdbuf[read - 1] = 0;
				if (cmdbuf[read - 2] == '\r') cmdbuf[read - 2] = 0;
			}
			while (*cmdbuf == L' ') {
				cmdbuf++;
			}
			if (*cmdbuf == 0)continue;
			BOOL escape = FALSE;
			BOOL str = FALSE;
			BOOL stre = FALSE;
			BOOL strs = (*cmdbuf == L'\"');
			size_t argc = 0;
			args[argc++] = cmdbuf;
			while (*cmdbuf != 0) {
				if (*cmdbuf == L' ') {
					if (!str) {
						if (argc == 512) {
							errPrintf("the maximum of 512 arguments reached!\n\r");
							break;
						}
						if (strs && stre) {
							*(cmdbuf - 1) = 0;
							args[argc - 1]++;
						}
						else {
							*cmdbuf = 0;
						}
						while (*(++cmdbuf) == L' ');
						args[argc++] = cmdbuf;
						strs = (*cmdbuf == L'\"');
					}
					else {
						++cmdbuf;
					}
					stre = FALSE;
					escape = FALSE;
				}
				else if (*cmdbuf == L'\\') {
					escape = !escape;
					stre = FALSE;
					++cmdbuf;
				}
				else if (*cmdbuf == L'\"') {
					if (!escape) {
						str = !str;
						stre = TRUE;
					}
					escape = FALSE;
					++cmdbuf;
				}
				else {
					escape = FALSE;
					stre = FALSE;
					++cmdbuf;
				}
			}
			if (strs && stre) {
				*(cmdbuf - 1) = 0;
				args[argc - 1]++;
			}
			
			if (!execCmd(args, argc)) {
				break;
			}

			memset(o_cmdbuf, 0, MAX_INPUT_CHARS << 1);
			memset(args, 0, 0x1000);
		}
		else {
			break;
		}
	}
	DWORD err = GetLastError();
	printf("exiting with code:%x\n\r", err);
	return err;
}