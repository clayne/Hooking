#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "Logging\Logging.h"

namespace Hook
{
	FARPROC GetProcAddress(HMODULE, LPCSTR);
	bool CheckExportAddress(HMODULE hModule, void* AddressCheck);
	HMODULE GetModuleHandle(char*);

	// Managed hooks
	void *HookAPI(HMODULE, const char *, void *, const char *, void *);
	void UnhookAPI(HMODULE, const char *, void *, const char *, void *);
	bool UnhookAll();

	// HotPatch hooks
	void *HotPatch(void*, const char*, void*);

	// IATPatch hooks
	void *IATPatch(HMODULE, DWORD, const char*, void*, const char*, void*);
	bool UnhookIATPatch(HMODULE, DWORD, const char *, void *, const char *, void *);
	bool UnIATPatchAll();
}
