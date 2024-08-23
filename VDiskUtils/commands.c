#include "console.h"
#include "vdisk.h"

BOOL parseHex(LPWSTR str, size_t* num) {
	*num = 0;
	while (*str != 0) {
		wchar_t c = *(str++);
		if (c >= L'0' && c <= L'9') {
			c -= L'0';
			*num <<= 4;
			*num |= (size_t)c;
		}
		else if (c >= L'A' && c <= L'F') {
			c -= L'7';
			*num <<= 4;
			*num |= (size_t)c;
		}
		else if (c >= L'a' && c <= L'f') {
			c -= L'W';
			*num <<= 4;
			*num |= (size_t)c;
		}
		else {
			errPrintf("Invalid character:%wc\n\r", c);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL parseBin(LPWSTR str, size_t* num) {
	*num = 0;
	while (*str != 0) {
		wchar_t c = *(str++);
		*num <<= 1;
		if (c == L'0') {
			continue;
		}
		else if (c == L'1') {
			*num |= 1;
		}
		else {
			errPrintf("Invalid character:%wc\n\r", c);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL parseDec(LPWSTR str, size_t* num) {
	*num = 0;
	while (*str != 0) {
		wchar_t c = *(str++);
		if (c >= L'0' && c <= L'9') {
			c -= L'0';
			*num *= 10;
			*num += (size_t)c;
		}
		else {
			errPrintf("Invalid character:%wc\n\r", c);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL parseI(LPWSTR str, size_t* num) {
	if (*str == L'0') {
		++str;
		if (*str == L'x' || *str == L'X') {
			return parseHex(++str, num);
		}
		else if (*str == L'b' || *str == L'B') {
			return parseBin(++str, num);
		}
	}
	return parseDec(str, num);
}

BOOL execCmd(LPWSTR* args, size_t argc) {
	if (argc == 0)return TRUE;
	LPWSTR arg0 = args[0];
	size_t arg0l = 0;
	while (*arg0 != 0) {
		if (*arg0 >= L'A' && *arg0 <= L'Z') {
			*arg0 += ' ';
		}
		++arg0;
		++arg0l;
	}
	arg0 = args[0];
	if (arg0l == 3) {
		if (wcsncmp(arg0, L"cls", 3) == 0) {
			if (argc > 2) {
				errPrintf("Invalid number of cls args\n\r");
			}
			else if (argc == 2) {
				if (wcscmp(args[1], L"-s") == 0) {
					cls(1);
				}
				else if (wcscmp(args[1], L"-c") == 0) {
					cls(0);
				}
				else if (wcscmp(args[1], L"-f") == 0) {
					cls(2);
				}
				else if (wcscmp(args[1], L"-h") == 0) {
					exePrintf("cls: [-c|-s|-f|-h]\n\r\t-c\t\tcmd-like cls\n\r\t-s\t\tuses special string to cls\n\r\t-f\t\tforcefully overwrites screenbuffer\n\r\t-h\t\tprints this info\n\rdefaults when no arg specified to -c\n\r");
				}
				else {
					errPrintf("Invalid arg! (-h for help)\n\r");
				}
			}
			else {
				cls(0);
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 4) {
		if (wcsncmp(arg0, L"exit", 4) == 0) {
			return FALSE;
		}
		else if (wcsncmp(arg0, L"list", 4) == 0) {
			if (argc > 1) {
				if (wcscmp(args[1], L"vdisk") == 0) {
					if (argc > 2) {
						errPrintf("No Arguments reqired!\n\r");
					}
					listVDISKs();
				}
				else if (wcscmp(args[1], L"partition") == 0) {
					if (argc > 2) {
						errPrintf("No Arguments reqired!\n\r");
					}
					listPartitions();
				}
				else {
					errPrintf("Invalid Command!\n\r");
				}
			}
			else {
				exePrintf("list [vdisk | partition]\n\r");
			}
		}
		else if (wcsncmp(arg0, L"help", 4) == 0) {
			//TODO
		}
		else if (wcsncmp(arg0, L"open", 4) == 0) {
			if (argc > 1) {
				if (argc == 3) {
					if (wcscmp(args[1], L"-i") == 0) {
						size_t index = 0;
						if (parseI(args[2], &index)) {
							if (openVdiskI(index)) {
								exePrintf("Opened Vdisk Successful\n\r");
							}
							else {
								errPrintf("Error on opening vdisk!\n\r");
							}
						}
					}
					else if (wcscmp(args[1], L"-f") == 0) {
						if (openVdiskP(args[2])) {
							exePrintf("Opened Vdisk Successful\n\r");
						}
						else {
							errPrintf("Error on opening vdisk!\n\r");
						}
					}
				}
				else if(wcscmp(args[1], L"-h") == 0) {
					exePrintf("open [-i index | -f \"path\"]\n\r\t-i\t\tindex of vdisk to be opened\n\r\t-f\t\tpath to file to be opened\n\r\t-h\t\tshows this info\n\r");
				}
				else {
					exePrintf("open [-i index | -f \"path\"]\n\r");
				}
			}
			else {
				if (!openVdiskI(-1)) {
					errPrintf("Error opening selected Vdisk!\n\r");
				}
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 5) {
		if (wcsncmp(arg0, L"close", 5) == 0) {
			if (argc == 2) {
				if (wcscmp(args[1], L"- h") == 0) {
					exePrintf("close [\"index of vdisk to close\"]\n\r");
				}
				else {
					size_t index = 0;
					if (parseI(args[1], &index)) {
						closeVDISK(index);
					}
				}
			}
			else if (argc > 2) {
				errPrintf("Invalid number of arguments!\n\r");
			}
			else {
				closeVDISK(-1);
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 6) {
		if (wcsncmp(arg0, L"select", 6) == 0) {
			if (argc > 1) {
				if (wcscmp(args[1], L"vdisk") == 0) {
					if (argc == 4) {
						if (wcscmp(args[2], L"-i") == 0) {
							size_t index = 0;
							if (parseI(args[3], &index)) {
								if (selectVdiskI(index)) {
									exePrintf("Selected Vdisk:%llu\n\r", index);
								}
							}
						}
						else if (wcscmp(args[2], L"-f") == 0) {
							if (selectVdiskP(args[3])) {
								exePrintf("Selected Vdisk\n\r");
							}
						}
						else {
							errPrintf("Invalid Argument!\n\r");
						}
					}
					else if (wcscmp(args[2], L"-h") == 0) {
						exePrintf("select vdisk [-i index | -f \"path\"]\n\r\t-i\t\tindex of vdisk to be selected\n\r\t-f\t\tpath to file to be selected\n\r\t-h\t\tshows this info\n\r");
					}
					else {
						exePrintf("select vdisk [-i index | -f \"path\"]\n\r");
					}
				}
				else if (wcscmp(args[1], L"partition") == 0) {
					if (argc == 3) {
						size_t index = 0;
						if (parseI(args[2], &index)) {
							if (selectPartition(index)) {
								exePrintf("Selected partition:%llu\n\r", index);
							}
						}
					}
					else {
						errPrintf("Invalid number of arguments!\n\r");
					}
				}
				else if (wcscmp(args[1], L"-h") == 0) {
					exePrintf("select [vdisk | partition]\n\r");
				}
				else {
					errPrintf("Invalid Command!\n\r");
				}
			}
			else {
				exePrintf("select [vdisk | partition]\n\r");
			}
		}
		else if (wcsncmp(arg0, L"create", 6) == 0) {
			//TODO
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if(arg0l == 9) {
		if (wcscmp(arg0, L"attribute") == 0) {
			if (wcscmp(args[1], L"query") == 0) {
				//TODO
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else {
		errPrintf("Invalid Command!\n\r");
	}
	return TRUE;
}