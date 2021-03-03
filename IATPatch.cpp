/**
* Copyright (C) 2021 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*
* Created from source code found in DxWnd v2.03.99
* https://sourceforge.net/projects/dxwnd/
*/

// return:
// 0 = patch failed
// 1 = already patched
// addr = address of the original function

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <vector>
#include "Hook.h"

namespace Hook
{
	struct IATPATCH
	{
		HMODULE module;
		DWORD ordinal;
		std::string dll;
		void *apiproc;
		std::string apiname;
		void *hookproc;
	};

	std::vector<IATPATCH> IATPatchProcs;

	void StoreIATRecord(HMODULE module, DWORD ordinal, const char *dll, void *apiproc, const char *apiname, void *hookproc)
	{
		IATPATCH tmpMemory;
		tmpMemory.module = module;
		tmpMemory.ordinal = ordinal;
		tmpMemory.dll = std::string(dll);
		tmpMemory.apiproc = apiproc;
		tmpMemory.hookproc = hookproc;
		tmpMemory.apiname = std::string(apiname);
		IATPatchProcs.push_back(tmpMemory);
	}
}

// Hook API using IAT patch
void *Hook::IATPatch(HMODULE module, DWORD ordinal, const char *dll, void *apiproc, const char *apiname, void *hookproc)
{
	PIMAGE_NT_HEADERS pnth;
	PIMAGE_IMPORT_DESCRIPTOR pidesc;
	DWORD base, rva;
	PSTR impmodule;
	PIMAGE_THUNK_DATA ptaddr;
	PIMAGE_THUNK_DATA ptname;
	PIMAGE_IMPORT_BY_NAME piname;
	DWORD oldprotect;
	void *org;

	// Check if dll name is blank
	if (!dll)
	{
		Logging::LogFormat(__FUNCTION__ " Error: NULL dll name");
		return nullptr;
	}

	// Check if API name is blank
	if (!apiname)
	{
		Logging::LogFormat(__FUNCTION__ " Error: NULL api name");
		return nullptr;
	}

	// Check module addresses
	if (!module)
	{
		Logging::LogFormat(__FUNCTION__ " Error: NULL api module address for '%s'", apiname);
		return nullptr;
	}

	// Check API address
	if (!apiproc)
	{
		Logging::LogFormat(__FUNCTION__ " Error: Failed to find '%s' api", apiname);
		return nullptr;
	}

	// Check hook address
	if (!hookproc)
	{
		Logging::LogFormat(__FUNCTION__ " Error: Invalid hook address for '%s'", apiname);
		return nullptr;
	}

#ifdef _DEBUG
	Logging::LogFormat(__FUNCTION__ ": module=%p ordinal=%x name=%s dll=%s", module, ordinal, apiname, dll);	
#endif

	base = (DWORD)module;
	org = 0; // by default, ret = 0 => API not found

	__try
	{
		pnth = PIMAGE_NT_HEADERS(PBYTE(base) + PIMAGE_DOS_HEADER(base)->e_lfanew);
		if (!pnth)
		{
			Logging::LogFormat(__FUNCTION__ ": ERROR no PNTH at %d", __LINE__);
			return nullptr;
		}
		rva = pnth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		if (!rva)
		{
			Logging::LogFormat(__FUNCTION__ ": ERROR no RVA at %d", __LINE__);
			return nullptr;
		}
		pidesc = (PIMAGE_IMPORT_DESCRIPTOR)(base + rva);

		while (pidesc->FirstThunk)
		{
			impmodule = (PSTR)(base + pidesc->Name);
#ifdef _DEBUG
			//Logging::LogFormat(__FUNCTION__ ": analyze impmodule=%s", impmodule);
#endif
			char *fname = impmodule;
			for (; *fname; fname++); for (; !*fname; fname++);

			if (!lstrcmpiA(dll, impmodule))
			{
#ifdef _DEBUG
				Logging::LogFormat(__FUNCTION__ ": dll=%s found at %p", dll, impmodule);				
#endif

				ptaddr = (PIMAGE_THUNK_DATA)(base + (DWORD)pidesc->FirstThunk);
				ptname = (pidesc->OriginalFirstThunk) ? (PIMAGE_THUNK_DATA)(base + (DWORD)pidesc->OriginalFirstThunk) : nullptr;

				while (ptaddr->u1.Function)
				{
#ifdef _DEBUG
					//Logging::LogFormat(__FUNCTION__ ": address=%x ptname=%x", ptaddr->u1.AddressOfData, ptname);
#endif

					if (ptname)
					{
						// examining by function name
						if (!IMAGE_SNAP_BY_ORDINAL(ptname->u1.Ordinal))
						{
							piname = (PIMAGE_IMPORT_BY_NAME)(base + (DWORD)ptname->u1.AddressOfData);
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": BYNAME ordinal=%x address=%x name=%s hint=%x", ptaddr->u1.Ordinal, ptaddr->u1.AddressOfData, (char *)piname->Name, piname->Hint);							
#endif
							if (!lstrcmpiA(apiname, (char *)piname->Name))
							{
								break;
							}
						}
						else
						{
#ifdef _DEBUG
							//Logging::LogFormat(__FUNCTION__ ": BYORD target=%x ord=%x", ordinal, IMAGE_ORDINAL32(ptname->u1.Ordinal));
#endif
							// skip unknow ordinal 0
							if (ordinal && (IMAGE_ORDINAL32(ptname->u1.Ordinal) == ordinal))
							{
#ifdef _DEBUG
								Logging::LogFormat(__FUNCTION__ ": BYORD ordinal=%x addr=%x", ptname->u1.Ordinal, ptaddr->u1.Function);
								//Logging::LogFormat(__FUNCTION__ ": BYORD GetProcAddress=%x", GetProcAddress(GetModuleHandle(dll), MAKEINTRESOURCE(IMAGE_ORDINAL32(ptname->u1.Ordinal))));									
#endif
								break;
							}
						}

					}
					else
					{
#ifdef _DEBUG
						//Logging::LogFormat(__FUNCTION__ ": fname=%s", fname);
						//LogText(buffer);
#endif
						if (!lstrcmpiA(apiname, fname))
						{
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": BYSCAN ordinal=%x address=%x name=%s", ptaddr->u1.Ordinal, ptaddr->u1.AddressOfData, fname);							
#endif
							break;
						}
						for (; *fname; fname++); for (; !*fname; fname++);
					}

					if (apiproc)
					{
						// examining by function addr
						if (ptaddr->u1.Function == (DWORD)apiproc)
						{
							break;
						}
					}
					ptaddr++;
					if (ptname) ptname++;
				}

				if (ptaddr->u1.Function)
				{
					org = (void *)ptaddr->u1.Function;
					if (org == hookproc) return (void *)1; // already hooked

					if (!VirtualProtect(&ptaddr->u1.Function, 4, PAGE_EXECUTE_READWRITE, &oldprotect))
					{
#ifdef _DEBUG
						Logging::LogFormat(__FUNCTION__ ": VirtualProtect error %d at %d", GetLastError(), __LINE__);						
#endif
						return nullptr;
					}
					ptaddr->u1.Function = (DWORD)hookproc;
					if (!VirtualProtect(&ptaddr->u1.Function, 4, oldprotect, &oldprotect))
					{
#ifdef _DEBUG
						Logging::LogFormat(__FUNCTION__ ": VirtualProtect error %d at %d", GetLastError(), __LINE__);						
#endif
						return nullptr;
					}
					if (!FlushInstructionCache(GetCurrentProcess(), &ptaddr->u1.Function, 4))
					{
#ifdef _DEBUG
						Logging::LogFormat(__FUNCTION__ ": FlushInstructionCache error %d at %d", GetLastError(), __LINE__);						
#endif
						return nullptr;
					}
#ifdef _DEBUG
					Logging::LogFormat(__FUNCTION__ " hook=%s address=%p->%p", apiname, org, hookproc);					
#endif
					// Record hook
					StoreIATRecord(module, ordinal, dll, apiproc, apiname, hookproc);

					// Return old address
					return org;
				}
			}
			pidesc++;
		}
		if (!pidesc->FirstThunk)
		{
#ifdef _DEBUG
			Logging::LogFormat(__FUNCTION__ ": PE unreferenced function %s:%s", dll, apiname);			
#endif
			return nullptr;
		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Logging::LogFormat(__FUNCTION__ "Ex: EXCEPTION hook=%s:%s Hook Failed.", dll, apiname);
	}
	return org;
}

// Restore all addresses hooked
bool Hook::UnIATPatchAll()
{
	bool flag = true;
	while (IATPatchProcs.size() != 0)
	{
		if (!UnhookIATPatch(IATPatchProcs.back().module, IATPatchProcs.back().ordinal, IATPatchProcs.back().dll.c_str(), IATPatchProcs.back().apiproc, IATPatchProcs.back().apiname.c_str(), IATPatchProcs.back().hookproc))
		{
			// Failed to restore address
			flag = false;
			Logging::LogFormat(__FUNCTION__ ": failed to restore address. procaddr: %p", IATPatchProcs.back().apiproc);
		}
		IATPatchProcs.pop_back();
	}
	IATPatchProcs.clear();
	return flag;
}

// Unhook IAT patched API
bool Hook::UnhookIATPatch(HMODULE module, DWORD ordinal, const char *dll, void *apiproc, const char *apiname, void *hookproc)
{
	PIMAGE_NT_HEADERS pnth;
	PIMAGE_IMPORT_DESCRIPTOR pidesc;
	DWORD base, rva;
	PSTR impmodule;
	PIMAGE_THUNK_DATA ptaddr;
	PIMAGE_THUNK_DATA ptname;
	PIMAGE_IMPORT_BY_NAME piname;
	DWORD oldprotect;
	void *org;

#ifdef _DEBUG
	Logging::LogFormat(__FUNCTION__ ": module=%p ordinal=%x name=%s dll=%s", module, ordinal, apiname, dll);	
#endif

	base = (DWORD)module;
	org = 0;

	__try
	{
		pnth = PIMAGE_NT_HEADERS(PBYTE(base) + PIMAGE_DOS_HEADER(base)->e_lfanew);
		if (!pnth)
		{
			Logging::LogFormat(__FUNCTION__ ": ERROR no PNTH at %d", __LINE__);
			return false;
		}
		rva = pnth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		if (!rva)
		{
			Logging::LogFormat(__FUNCTION__ ": ERROR no RVA at %d", __LINE__);
			return false;
		}
		pidesc = (PIMAGE_IMPORT_DESCRIPTOR)(base + rva);

		while (pidesc->FirstThunk)
		{
			impmodule = (PSTR)(base + pidesc->Name);
#ifdef _DEBUG
			//Logging::LogFormat(__FUNCTION__ ": analyze impmodule=%s", impmodule);
#endif
			char *fname = impmodule;
			for (; *fname; fname++); for (; !*fname; fname++);

			if (!lstrcmpiA(dll, impmodule))
			{
#ifdef _DEBUG
				Logging::LogFormat(__FUNCTION__ ": dll=%s found at %p", dll, impmodule);				
#endif

				ptaddr = (PIMAGE_THUNK_DATA)(base + (DWORD)pidesc->FirstThunk);
				ptname = (pidesc->OriginalFirstThunk) ? (PIMAGE_THUNK_DATA)(base + (DWORD)pidesc->OriginalFirstThunk) : nullptr;

				while (ptaddr->u1.Function)
				{
#ifdef _DEBUG
					//Logging::LogFormat(__FUNCTION__ ": address=%x ptname=%x", ptaddr->u1.AddressOfData, ptname);
#endif

					if (ptname)
					{
						// examining by function name
						if (!IMAGE_SNAP_BY_ORDINAL(ptname->u1.Ordinal))
						{
							piname = (PIMAGE_IMPORT_BY_NAME)(base + (DWORD)ptname->u1.AddressOfData);
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": BYNAME ordinal=%x address=%x name=%s hint=%x", ptaddr->u1.Ordinal, ptaddr->u1.AddressOfData, (char *)piname->Name, piname->Hint);							
#endif
							if (!lstrcmpiA(apiname, (char *)piname->Name))
							{
								break;
							}
						}
						else
						{
#ifdef _DEBUG
							//Logging::LogFormat(__FUNCTION__ ": BYORD target=%x ord=%x", ordinal, IMAGE_ORDINAL32(ptname->u1.Ordinal));
#endif
							// skip unknown ordinal 0
							if (ordinal && (IMAGE_ORDINAL32(ptname->u1.Ordinal) == ordinal))
							{
#ifdef _DEBUG
								Logging::LogFormat(__FUNCTION__ ": BYORD ordinal=%x addr=%x", ptname->u1.Ordinal, ptaddr->u1.Function);
								//Logging::LogFormat(__FUNCTION__ ": BYORD GetProcAddress=%x", GetProcAddress(GetModuleHandle(dll), MAKEINTRESOURCE(IMAGE_ORDINAL32(ptname->u1.Ordinal))));									
#endif
								break;
							}
						}

					}
					else
					{
#ifdef _DEBUG
						//Logging::LogFormat(__FUNCTION__ ": fname=%s", fname);
#endif
						if (!lstrcmpiA(apiname, fname))
						{
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": BYSCAN ordinal=%x address=%x name=%s", ptaddr->u1.Ordinal, ptaddr->u1.AddressOfData, fname);							
#endif
							break;
						}
						for (; *fname; fname++); for (; !*fname; fname++);
					}

					if (apiproc)
					{
						// examining by function addr
						if (ptaddr->u1.Function == (DWORD)apiproc)
						{
							break;
						}
					}
					ptaddr++;
					if (ptname) ptname++;
				}

				if (ptaddr->u1.Function)
				{
					org = (void *)ptaddr->u1.Function;

					// Check if API is patched
					if (org == hookproc)
					{

						if (!VirtualProtect(&ptaddr->u1.Function, 4, PAGE_EXECUTE_READWRITE, &oldprotect))
						{
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": VirtualProtect error %d at %d", GetLastError(), __LINE__);							
#endif
							return false;
						}
						ptaddr->u1.Function = (DWORD)apiproc;
						if (!VirtualProtect(&ptaddr->u1.Function, 4, oldprotect, &oldprotect))
						{
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": VirtualProtect error %d at %d", GetLastError(), __LINE__);							
#endif
							return false;
						}
						if (!FlushInstructionCache(GetCurrentProcess(), &ptaddr->u1.Function, 4))
						{
#ifdef _DEBUG
							Logging::LogFormat(__FUNCTION__ ": FlushInstructionCache error %d at %d", GetLastError(), __LINE__);							
#endif
							return false;
						}
#ifdef _DEBUG
						Logging::LogFormat(__FUNCTION__ " hook=%s address=%p->%p", apiname, org, hookproc);						
#endif

						return true;
					}
					return false;
				}
			}
			pidesc++;
		}
		if (!pidesc->FirstThunk)
		{
#ifdef _DEBUG
			Logging::LogFormat(__FUNCTION__ ": PE unreferenced function %s:%s", dll, apiname);			
#endif
			return false;
		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Logging::LogFormat(__FUNCTION__ "Ex: EXCEPTION hook=%s:%s Hook Failed.", dll, apiname);
	}
	return false;
}
