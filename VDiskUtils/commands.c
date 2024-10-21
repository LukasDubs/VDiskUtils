#include "vdisk.h"
#include "console.h"

PFS_DRIVER driver = 0;
HANDLE current_dir = INVALID_HANDLE_VALUE;
HANDLE current_file = INVALID_HANDLE_VALUE;

BOOL static parseHex(LPWSTR str, size_t* num) {
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

BOOL static parseBin(LPWSTR str, size_t* num) {
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

BOOL static parseDec(LPWSTR str, size_t* num) {
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

BOOL static parseI(LPWSTR str, size_t* num) {
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

size_t static prepStr(LPWSTR arg0) {
	size_t arg0l = 0;
	while (*arg0 != 0) {
		if (*arg0 >= L'A' && *arg0 <= L'Z') {
			*arg0 += ' ';
		}
		++arg0;
		++arg0l;
	}
	return arg0l;
}

#define CLS_HELP "cls: [-c|-s|-f|-h]\n\r\t-c\t\tcmd-like cls\n\r\t-s\t\tuses special string to cls\n\r\t-f\t\tforcefully overwrites screenbuffer\n\r\t-h\t\tprints this info\n\rdefaults when no arg specified to -c\n\r"
#define RAW_HELP "raw: read|write\n\r\tread\t\tperform a raw read on the disk\n\r\twrite\t\tperforms a raw write to the disk\n\r"
#define LIST_HELP "list [vdisk | partition]\n\r"
#define OPEN_HELP "open [-i index | -f \"path\"]\n\r\t-i\t\tindex of vdisk to be opened\n\r\t-f\t\tpath to file to be opened\n\r\t-h\t\tshows this info\n\r"
#define CLOSE_HELP "close [\"index of vdisk to close\"]\n\r"

__inline static void clscmd(LPWSTR* args, size_t argc) {
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
			exePrintf(CLS_HELP);
		}
		else {
			errPrintf("Invalid arg! (-h for help)\n\r");
		}
	}
	else {
		cls(0);
	}
}

int execCmd(LPWSTR* args, size_t argc) {
	if (argc == 0)return CMD_STATUS_SUCCESS;
	LPWSTR arg0 = args[0];
	size_t arg0l = prepStr(arg0);
	if (arg0l == 3) {
		if (wcsncmp(arg0, L"cls", 3) == 0) {
			clscmd(args, argc);
		}
		else if (wcsncmp(arg0, L"raw", 3) == 0) {
			if (argc == 1) {
				exePrintf(RAW_HELP);
			}
			else if(wcscmp(args[1], L"read") == 0) {
				//TODO
				errPrintf("Not Implemented!\n\r");
			}
			else if (wcscmp(args[1], L"write") == 0) {
				//TODO
				errPrintf("Not Implemented!\n\r");
			}
			else if (wcscmp(args[1], L"-h") == 0) {
				exePrintf(RAW_HELP);
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 4) {
		if (wcsncmp(arg0, L"exit", 4) == 0) {
			SetLastError(0);
			return CMD_STATUS_EXIT;
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
				else if (wcscmp(args[1], L"-h") == 0) {
					exePrintf(LIST_HELP);
				}
				else {
					errPrintf("Invalid Command!\n\r");
				}
			}
			else {
				exePrintf(LIST_HELP);
			}
		}
		else if (wcsncmp(arg0, L"help", 4) == 0) {
			exePrintf("Implemented Commands:\n\r\tcls\t\tclears the screen\n\r\texit\t\texits the tool\n\r\tlist\t\tlist vdisks\n\r\thelp\t\tshows this info\n\r\topen\t\topen a vdisk\n\r\tclose\t\tcloses the selected vdisk\n\r\tenter\t\tenters Filesystem-Management mode\n\r\tselect\t\tselects a vdisk\n\r");
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
					exePrintf(OPEN_HELP);
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
				if (wcscmp(args[1], L"-h") == 0) {
					exePrintf(CLOSE_HELP);
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
		else if (wcsncmp(arg0, L"enter", 5) == 0) {
			driver = getSelDrv();
			if (driver != 0) {
				NTSTATUS status = driver->open(driver, L"/", 0, &current_dir);
				if (status != 0) {
					errPrintf("opening root dir failed:%x\n\r", status);
					driver->exit(driver);
					driver = 0;
				}
				else {
					return CMD_STATUS_FS_CONTEXT;
				}
			}
			else {
				errPrintf("Entering FS manager failed!\n\r");
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
					else if ((argc == 3) && (wcscmp(args[2], L"-h") == 0)) {
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
			errPrintf("Not Implemented!\n\r");
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if(arg0l == 9) {
		if (wcscmp(arg0, L"attribute") == 0) {
			if (wcscmp(args[1], L"get") == 0) {
				//TODO
				errPrintf("Not Implemented!\n\r");
			}
			else if (wcscmp(args[1], L"set") == 0) {
				//TODO
				errPrintf("Not Implemented!\n\r");
			}
			else {
				//TODO
				errPrintf("Not Implemented!\n\r");
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else {
		errPrintf("Invalid Command!\n\r");
	}
	return CMD_STATUS_SUCCESS;
}

int execFsCmd(LPWSTR* args, size_t argc) {
	if (argc == 0)return CMD_STATUS_SUCCESS;
	if (driver == 0) {
		return CMD_STATUS_CMD_CONTEXT;
	}
	LPWSTR arg0 = args[0];
	size_t arg0l = prepStr(arg0);
	if (arg0l == 2) {
		if (wcsncmp(arg0, L"ls", 2) == 0) {
			if (current_dir == INVALID_HANDLE_VALUE) {
				errPrintf("Invalid directory");
			}
			else {
				errPrintf("Not Implemented!\n\r");
			}
		}
		else if (wcsncmp(arg0, L"cd", 2) == 0) {
			if (argc != 2) {
				errPrintf("Syntax: cd \"path\"\n\r");
			}
			else {
				HANDLE hd;
				NTSTATUS status;
				if ((status = driver->open(driver, args[1], current_dir, &hd)) != 0) {
					errPrintf("Couldn't change dir:%x\n\r", status);
				}
				else {
					DWORD attrib = 0;
					if ((status = driver->get_info(driver, hd, FSAttribute, &attrib, sizeof(attrib))) != 0) {
						errPrintf("Cannot get file attribute:%x\n\r", status);
					}
					else if((attrib & FS_ATTRIBUTE_DIR) != 0) {
						driver->close(driver, current_dir);
						current_dir = hd;
					}
					else {
						driver->close(driver, hd);
						errPrintf("Path destination is not a Directory!\n\r");
					}
				}
			}

		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 3) {
		if (wcsncmp(arg0, L"cls", 3) == 0) {
			clscmd(args, argc);
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 4) {
		if (wcsncmp(arg0, L"open", 4) == 0) {
			if (argc != 2) {
				errPrintf("Invalid number of arguments! Syntax: open \"path\"\n\r");
			}
			else if (current_file != INVALID_HANDLE_VALUE) {
				errPrintf("Close current File before opening a new one!\n\r");
			}
			else {
				NTSTATUS status = driver->open(driver, args[1], current_dir, &current_file);
				if (status != 0) {
					errPrintf("Opening File failed:%x\n\r", status);
				}
				else {
					exePrintf("Opened File successfully\n\r");
				}
			}
		}
		else if (wcsncmp(arg0, L"exit", 4) == 0) {
			driver->exit(driver);
			driver = 0;
			return CMD_STATUS_CMD_CONTEXT;
		}
		else if (wcsncmp(arg0, L"help", 4) == 0) {
			exePrintf("Implemented Commands:\n\r\tcd\t\tchanges the current dir\n\r\tcls\t\tclears the screen\n\r\topen\t\topens a file\n\r\texit\t\texits FS-Manager mode\n\r\tclose\t\tcloses the currently open file\n\r\tattribute\t\tget/set the attribute of a file\n\r");
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 5) {
		if (wcsncmp(arg0, L"close", 5) == 0) {
			if (current_file != INVALID_HANDLE_VALUE) {
				driver->close(driver, current_file);
				current_file = INVALID_HANDLE_VALUE;
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else if (arg0l == 9) {
		if (wcsncmp(arg0, L"attribute", 9) == 0) {
			if (argc == 1) {
				exePrintf("usage: attribute [get | set]\n\r");
			}
			else {
				if (wcscmp(args[1], L"get") == 0) {
					if (argc != 3) {
						exePrintf("usage: attribute get \"attribute\"\n\r\tname     \treturns the filename\n\r\tsize     \treturns the filesizie\n\r\tattribute\treturns the file attribute\n\r\tcreation \treturns the creation date\n\r\tmodified \treturns the last modified date\n\r\taccessed \treturns the last accessed date\n\r\tdates    \treturns the creaton and modified and last accessed date\n\r");
					}
					else {
						if (wcscmp(args[2], L"name") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_NAME_INFO data = { 0 };
								data.name_8_3 = HeapAlloc(proc_heap, 0, 1024);
								if (data.name_8_3 != 0) {
									data.name_8_3_max_length = 256;
									data.long_name = (PWSTR)(((size_t)data.name_8_3) + 512);
									data.long_name_max_length = 256;
									NTSTATUS status = driver->get_info(driver, current_file, FSName, &data, sizeof(data));
									if (status != 0) {
										errPrintf("Getting filename failed:%x\n\r", status);
									}
									else {
										exePrintf("LFN: \"%ws\"\t8.3-Name:\"%s\"\n\r", data.long_name, data.name_8_3);
									}
									HeapFree(proc_heap, 0, data.name_8_3);
								}
								else {
									errPrintf("HeapAlloc failed:%x\n\r", GetLastError());
								}
							}
						}
						else if (wcscmp(args[2], L"size") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_FILESIZE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSSize, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									exePrintf("Filesize:%llu, Size on Disk:%llu\n\r", data.size, data.size_on_disk);
								}
							}
						}
						else if (wcscmp(args[2], L"attribute") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_ATTRIBUTE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSAttribute, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									exePrintf("File Attribute:%x\n\r", data);
								}
							}
						}
						else if (wcscmp(args[2], L"creation") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_DATE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSDateInfo, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									SYSTEMTIME systime = { 0 };
									if (!FileTimeToSystemTime(&(data.creation), &systime)) {
										errPrintf("Couldn't get creation Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Creation (Date: yy-mm-dd): %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
								}
							}
						}
						else if (wcscmp(args[2], L"modified") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_DATE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSDateInfo, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									SYSTEMTIME systime = { 0 };
									if (!FileTimeToSystemTime(&(data.last_modified), &systime)) {
										errPrintf("Couldn't get las modified Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Last-Modified (Date: yy-mm-dd): %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
								}
							}
						}
						else if (wcscmp(args[2], L"accessed") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_DATE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSDateInfo, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									SYSTEMTIME systime = { 0 };
									if (!FileTimeToSystemTime(&(data.last_accessed), &systime)) {
										errPrintf("Couldn't get las accessed Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Last-Accessed (Date: yy-mm-dd): %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
								}
							}
						}
						else if (wcscmp(args[2], L"dates") == 0) {
							if (current_file == INVALID_HANDLE_VALUE) {
								errPrintf("Invalid/No File selected!\n\r");
							}
							else {
								FS_DATE_INFO data = { 0 };
								NTSTATUS status = driver->get_info(driver, current_file, FSDateInfo, &data, sizeof(data));
								if (status != 0) {
									errPrintf("Getting filename failed:%x\n\r", status);
								}
								else {
									SYSTEMTIME systime = { 0 };
									if (!FileTimeToSystemTime(&(data.creation), &systime)) {
										errPrintf("Couldn't get creation Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Creation (Date: yy-mm-dd):      %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
									if (!FileTimeToSystemTime(&(data.last_modified), &systime)) {
										errPrintf("Couldn't get las modified Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Last-Modified (Date: yy-mm-dd): %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
									if (!FileTimeToSystemTime(&(data.last_accessed), &systime)) {
										errPrintf("Couldn't get las accessed Time:%x\n\r", GetLastError());
									}
									else {
										exePrintf("Last-Accessed (Date: yy-mm-dd): %u-%u-%u at %u:%u:%u.%04u\n\r", systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
									}
								}
							}
						}
						else {
							errPrintf("usage: attribute get \"attribute\"\n\r\tname    \treturns the filename\n\r\tsize    \treturns the filesizie\n\r\tcreation\treturns the creation date\n\r\tmodified\treturns the last modified date\n\r\taccessed\treturns the last accessed date\n\r\tdates   \treturns the creaton and modified and last accessed date\n\r");
						}
					}
				}
				else if (wcscmp(args[1], L"set") == 0) {
					errPrintf("Not Implemented!\n\r");
				}
				else {
					exePrintf("usage: attribute get|set \"attribute\" \"value\"\n\r");
				}
			}
		}
		else {
			errPrintf("Invalid Command!\n\r");
		}
	}
	else {
		errPrintf("Invalid Command!\n\r");
	}
	return CMD_STATUS_SUCCESS;
}