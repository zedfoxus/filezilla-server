#include "stdafx.h"
#include "dll.h"

DLL::DLL()
{
}

DLL::DLL(CString const& s)
{
	load(s);
}

DLL::~DLL()
{
	clear();
}

bool DLL::load(CString const& s)
{
	clear();
	hModule = LoadLibraryEx(s, 0, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
	return hModule != 0;
}

void DLL::clear()
{
	if (hModule) {
		FreeLibrary(hModule);
		hModule = 0;
	}
}
