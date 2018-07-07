// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2018 - Tim Kosse <tim.kosse@filezilla-project.org>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "stdafx.h"
#include "misc\md5.h"
#include "Permissions.h"
#include "pugixml/pugixml.hpp"
#include "xml_utils.h"
#include "options.h"
#include "iputils.h"

#include "AsyncSslSocketLayer.h"

#include <libfilezilla/format.hpp>
#include <libfilezilla/string.hpp>

#include <array>
#include <cwctype>

class CPermissionsHelperWindow final
{
public:
	CPermissionsHelperWindow(CPermissions *pPermissions)
	{
		ASSERT(pPermissions);
		m_pPermissions = pPermissions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize = sizeof wndclass;
		wndclass.style = 0;
		wndclass.lpfnWndProc = WindowProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = GetModuleHandle(0);
		wndclass.hIcon = 0;
		wndclass.hCursor = 0;
		wndclass.hbrBackground = 0;
		wndclass.lpszMenuName = 0;
		wndclass.lpszClassName = _T("CPermissions Helper Window");
		wndclass.hIconSm = 0;

		RegisterClassEx(&wndclass);

		m_hWnd = CreateWindow(_T("CPermissions Helper Window"), _T("CPermissions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	~CPermissionsHelperWindow()
	{
		//Destroy window
		if (m_hWnd) {
			DestroyWindow(m_hWnd);
			m_hWnd = 0;
		}
	}

	HWND GetHwnd()
	{
		return m_hWnd;
	}

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_USER) {
			/* If receiving WM_USER, update the permission data of the instance with the permission
			 * data from the global data
			 */

			// Get own instance
			CPermissionsHelperWindow *pWnd=(CPermissionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd) {
				return 0;
			}
			if (!pWnd->m_pPermissions) {
				return 0;
			}

			pWnd->m_pPermissions->UpdatePermissions(true);
		}
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}

protected:
	CPermissions *m_pPermissions;

private:
	HWND m_hWnd;
};

/////////////////////////////////////////////////////////////////////////////
// CPermissions

std::recursive_mutex CPermissions::m_mutex;
CPermissions::t_UsersList CPermissions::m_sUsersList;
CPermissions::t_GroupsList CPermissions::m_sGroupsList;
std::vector<CPermissions *> CPermissions::m_sInstanceList;

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CPermissions::CPermissions(std::function<void()> const& updateCallback)
	: updateCallback_(updateCallback)
{
	Init();
}

CPermissions::~CPermissions()
{
	simple_lock lock(m_mutex);
	auto it = std::find(m_sInstanceList.begin(), m_sInstanceList.end(), this);
	if (it != m_sInstanceList.end()) {
		m_sInstanceList.erase(it);
	}
	delete m_pPermissionsHelperWindow;
}

void CPermissions::AddLongListingEntry(std::list<t_dirlisting> &result, bool isDir, std::string const& name, const t_directory& directory, __int64 size, FILETIME* pTime, std::string const&, bool *)
{
	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!pTime && GetStatus64(directory.dir.c_str(), status)) {
		size = GetLength64(status);
		pTime = &status.ftLastWriteTime;
	}

	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (60 + name.size())) {
		result.push_back(t_dirlisting());
	}

	if (isDir) {
		memcpy(result.back().buffer + result.back().len, "drwxr-xr-x", 10);
		result.back().len += 10;
	}
	else {
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = directory.bFileWrite ? 'w' : '-';

		bool isexe = false;
		if (name.size() > 4 && name[name.size() - 4] == '.') {
			std::string ext = name.substr(name.size() - 3);
			ext = fz::str_tolower_ascii(ext);
			if (ext == "exe") {
				isexe = true;
			}
			else if (ext == "bat") {
				isexe = true;
			}
			else if (ext == "com") {
				isexe = true;
			}
		}
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
		result.back().buffer[result.back().len++] = directory.bFileRead ? 'r' : '-';
		result.back().buffer[result.back().len++] = '-';
		result.back().buffer[result.back().len++] = isexe ? 'x' : '-';
	}

	memcpy(result.back().buffer + result.back().len, " 1 ftp ftp ", 11);
	result.back().len += 11;

	result.back().len += sprintf(result.back().buffer + result.back().len, "% 14I64d", size);

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime) {
		mtime = *pTime;
	}
	else {
		mtime = fTime;
	}

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	long long t1 = (static_cast<long long>(mtime.dwHighDateTime) << 32) + mtime.dwLowDateTime;
	t1 -= offset;
	mtime.dwHighDateTime = static_cast<DWORD>(t1 >> 32);
	mtime.dwLowDateTime = static_cast<DWORD>(t1 & 0xFFFFFFFF);

	SYSTEMTIME sFileTime;
	FileTimeToSystemTime(&mtime, &sFileTime);

	long long t2 = (static_cast<long long>(fTime.dwHighDateTime) << 32) + fTime.dwLowDateTime;
	const char months[][4]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	result.back().len += sprintf(result.back().buffer + result.back().len, " %s %02d ", months[sFileTime.wMonth-1], sFileTime.wDay);
	if (t1 > t2 || (t2 - t1) > (1000000ll * 60 * 60 * 24 * 350)) {
		result.back().len += sprintf(result.back().buffer + result.back().len, " %d ", sFileTime.wYear);
	}
	else {
		result.back().len += sprintf(result.back().buffer + result.back().len, "%02d:%02d ", sFileTime.wHour, sFileTime.wMinute);
	}

	memcpy(result.back().buffer + result.back().len, name.c_str(), name.size());
	result.back().len += name.size();
	result.back().buffer[result.back().len++] = '\r';
	result.back().buffer[result.back().len++] = '\n';
}

void CPermissions::AddFactsListingEntry(std::list<t_dirlisting> &result, bool isDir, std::string const& name, const t_directory& directory, __int64 size, FILETIME* pTime, std::string const&, bool *enabledFacts)
{
	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (!pTime && GetStatus64(directory.dir.c_str(), status)) {
		size = GetLength64(status);
		pTime = &status.ftLastWriteTime;
	}

	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (76 + name.size())) {
		result.push_back(t_dirlisting());
	}

	if (!enabledFacts || enabledFacts[0]) {
		if (isDir) {
			memcpy(result.back().buffer + result.back().len, "type=dir;", 9);
			result.back().len += 9;
		}
		else {
			memcpy(result.back().buffer + result.back().len, "type=file;", 10);
			result.back().len += 10;
		}
	}

	// Adjust time zone info and output file date/time
	SYSTEMTIME sLocalTime;
	GetLocalTime(&sLocalTime);
	FILETIME fTime;
	VERIFY(SystemTimeToFileTime(&sLocalTime, &fTime));

	FILETIME mtime;
	if (pTime) {
		mtime = *pTime;
	}
	else {
		mtime = fTime;
	}

	if (!enabledFacts || enabledFacts[2]) {
		if (mtime.dwHighDateTime || mtime.dwLowDateTime) {
			SYSTEMTIME time;
			FileTimeToSystemTime(&mtime, &time);
			std::string str = fz::sprintf("modify=%04d%02d%02d%02d%02d%02d;",
				time.wYear,
				time.wMonth,
				time.wDay,
				time.wHour,
				time.wMinute,
				time.wSecond);

			memcpy(result.back().buffer + result.back().len, str.c_str(), str.size());
			result.back().len += str.size();
		}
	}

	if (!enabledFacts || enabledFacts[1]) {
		if (!isDir) {
			result.back().len += sprintf(result.back().buffer + result.back().len, "size=%I64d;", size);
		}
	}

	if (enabledFacts && enabledFacts[fact_perm]) {
		// TODO: a, d,f,p,r,w
		memcpy(result.back().buffer + result.back().len, "perm=", 5);
		result.back().len += 5;
		if (isDir) {
			if (directory.bFileWrite) {
				result.back().buffer[result.back().len++] = 'c';
			}
			result.back().buffer[result.back().len++] = 'e';
			if (directory.bDirList) {
				result.back().buffer[result.back().len++] = 'l';
			}
			if (directory.bFileDelete || directory.bDirDelete) {
				result.back().buffer[result.back().len++] = 'p';
			}
		}
	}

	result.back().len += sprintf(result.back().buffer + result.back().len, " %s\r\n", name.c_str());
}

void CPermissions::AddShortListingEntry(std::list<t_dirlisting> &result, bool, std::string const& name, const t_directory&, __int64, FILETIME*, std::string const& dirToDisplay, bool *)
{
	// This wastes some memory but keeps the whole thing fast
	if (result.empty() || (8192 - result.back().len) < (10 + name.size() + dirToDisplay.size())) {
		result.push_back(t_dirlisting());
	}

	memcpy(result.back().buffer + result.back().len, dirToDisplay.c_str(), dirToDisplay.size());
	result.back().len += dirToDisplay.size();
	memcpy(result.back().buffer + result.back().len, name.c_str(), name.size());
	result.back().len += name.size();
	result.back().buffer[result.back().len++] = '\r';
	result.back().buffer[result.back().len++] = '\n';
}

int CPermissions::GetDirectoryListing(CUser const& user, std::wstring currentDir, std::wstring dirToDisplay,
									  std::list<t_dirlisting> &result, std::wstring& physicalDir,
									  std::wstring& logicalDir, addFunc_t addFunc,
									  bool *enabledFacts)
{
	std::wstring dir = CanonifyServerDir(currentDir, dirToDisplay);
	if (dir.empty()) {
		return PERMISSION_INVALIDNAME;
	}
	logicalDir = dir;

	// Get directory from directory name
	t_directory directory;
	bool bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	std::wstring sFileSpec = L"*"; // Which files to list in the directory
	if (res == PERMISSION_FILENOTDIR || res == PERMISSION_NOTFOUND) // Try listing using a direct wildcard filespec instead?
	{
		// Check dirToDisplay if we are allowed to go back a directory
		fz::replace_substrings(dirToDisplay, L"\\", L"/");
		while (fz::replace_substrings(dirToDisplay, _T("//"), _T("/")));
		if (!dirToDisplay.empty() && dirToDisplay.back() == '/') {
			return res;
		}
		size_t pos = dirToDisplay.rfind('/');
		if (res != PERMISSION_FILENOTDIR && pos != std::wstring::npos && dirToDisplay.find('*', pos + 1) == std::string::npos) {
			return res;
		}
		if (pos != std::wstring::npos) {
			dirToDisplay = dirToDisplay.substr(0, pos + 1);
		}

		if (dir == _T("/")) {
			return res;
		}

		pos = dir.rfind('/');
		if (pos != std::wstring::npos) {
			sFileSpec = dir.substr(pos + 1);
		}
		if (pos > 0) {
			dir = dir.substr(0, pos);
		}
		else {
			dir = _T("/");
		}

		if (sFileSpec.find('*') == std::wstring::npos && res != PERMISSION_FILENOTDIR) {
			return res;
		}

		res = GetRealDirectory(dir, user, directory, bTruematch);
	}
	if (res) {
		return res;
	}

	// Check permissions
	if (!directory.bDirList) {
		return PERMISSION_DENIED;
	}

	TIME_ZONE_INFORMATION tzInfo;
	int tzRes = GetTimeZoneInformation(&tzInfo);
	_int64 offset = tzInfo.Bias+((tzRes==TIME_ZONE_ID_DAYLIGHT)?tzInfo.DaylightBias:tzInfo.StandardBias);
	offset *= 60 * 10000000;

	if (!dirToDisplay.empty() && dirToDisplay.back() != '/') {
		dirToDisplay.append(L"/");
	}

	auto dirToDisplayUTF8 = fz::to_utf8(dirToDisplay);
	if (dirToDisplayUTF8.empty() && !dirToDisplay.empty()) {
		return PERMISSION_DENIED;
	}

	for (auto const& virtualAliasName : user.virtualAliasNames) {
		if (fz::stricmp(virtualAliasName.first, dir)) {
			continue;
		}

		t_directory directory;
		bool truematch = false;
		if (GetRealDirectory(dir + _T("/") + virtualAliasName.second, user, directory, truematch)) {
			continue;
		}
		if (!directory.bDirList) {
			continue;
		}
		if (!truematch && !directory.bDirSubdirs) {
			continue;
		}

		if (sFileSpec != L"*.*" && sFileSpec != L"*") {
			if (!WildcardMatch(virtualAliasName.second, sFileSpec)) {
				continue;
			}
		}

		auto name = fz::to_utf8(virtualAliasName.second);
		if (!name.empty()) {
			addFunc(result, true, name, directory, 0, 0, dirToDisplayUTF8, enabledFacts);
		}
	}

	physicalDir = directory.dir;
	if (sFileSpec != L"*" && sFileSpec != L"*.*") {
		physicalDir += sFileSpec;
	}

	WIN32_FIND_DATA FindFileData;
	WIN32_FIND_DATA NextFindFileData;
	HANDLE hFind;
	hFind = FindFirstFile((directory.dir + L"\\" + sFileSpec).c_str(), &NextFindFileData);
	while (hFind != INVALID_HANDLE_VALUE) {
		FindFileData = NextFindFileData;
		if (!FindNextFile(hFind, &NextFindFileData)) {
			FindClose(hFind);
			hFind = INVALID_HANDLE_VALUE;
		}

		if (!_tcscmp(FindFileData.cFileName, _T(".")) || !_tcscmp(FindFileData.cFileName, _T(".."))) {
			continue;
		}

		std::wstring const fn = FindFileData.cFileName;

		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Check permissions of subdir. If we don't have LIST permission,
			// don't display the subdir.
			bool truematch{};
			t_directory subDir;
			if (GetRealDirectory(dir + _T("/") + fn, user, subDir, truematch)) {
				continue;
			}

			if (subDir.bDirList) {
				auto utf8 = fz::to_utf8(fn);
				if (utf8.empty() && !fn.empty()) {
					continue;
				}
				addFunc(result, true, utf8, subDir, 0, &FindFileData.ftLastWriteTime, dirToDisplayUTF8, enabledFacts);
			}
		}
		else {
			auto utf8 = fz::to_utf8(fn);
			if (utf8.empty() && !fn.empty()) {
				continue;
			}
			addFunc(result, false, utf8, directory, FindFileData.nFileSizeLow + ((_int64)FindFileData.nFileSizeHigh<<32), &FindFileData.ftLastWriteTime, dirToDisplayUTF8, enabledFacts);
		}
	}

	return 0;
}

int CPermissions::CheckDirectoryPermissions(CUser const& user, std::wstring dirname, std::wstring currentdir, int op, std::wstring& physicalDir, std::wstring& logicalDir)
{
	std::wstring dir = CanonifyServerDir(currentdir, dirname);
	if (dir.empty()) {
		return PERMISSION_INVALIDNAME;
	}
	if (dir == L"/") {
		return PERMISSION_NOTFOUND;
	}

	size_t pos = dir.rfind('/');
	if (pos == std::wstring::npos || pos + 1 == dir.size()) {
		return PERMISSION_NOTFOUND;
	}
	logicalDir = dir;
	dirname = dir.substr(pos + 1);
	if (!pos) {
		dir = L"/";
	}
	else {
		dir = dir.substr(0, pos);
	}

	// dir now is the absolute path (logical server path of course)
	// awhile dirname is the pure dirname without the full path

	std::wstring realDir;
	std::wstring realDirname;

	// Get the physical path, only of dir to get the right permissions
	t_directory directory;
	bool truematch;
	int res;

	std::wstring dir2 = dir;
	std::wstring dirname2 = dirname;
	do
	{
		res = GetRealDirectory(dir2, user, directory, truematch);
		if (res & PERMISSION_NOTFOUND && op == DOP_CREATE)
		{ //that path could not be found. Maybe more than one directory level has to be created, check that
			if (dir2 == L"/") {
				return res;
			}

			size_t pos = dir2.rfind('/');
			if (pos == std::wstring::npos) {
				return res;
			}

			dirname2 = dir2.substr(pos + 1) + _T("/") + dirname2;
			if (pos) {
				dir2 = dir2.substr(0, pos);
			}
			else {
				dir2 = _T("/");
			}

			continue;
		}
		else if (res) {
			return res;
		}

		realDir = directory.dir;
		realDirname = dirname2;
		if (!directory.bDirDelete && op & DOP_DELETE) {
			res |= PERMISSION_DENIED;
		}
		if (!directory.bDirCreate && op & DOP_CREATE) {
			res |= PERMISSION_DENIED;
		}
		break;
	} while (true);

	fz::replace_substrings(realDirname, L"/", L"\\");
	physicalDir = realDir + L"\\" + realDirname;

	//Check if dir + dirname is a valid path
	int res2 = GetRealDirectory(dir + _T("/") + dirname, user, directory, truematch);

	if (!res2) {
		physicalDir = directory.dir;
	}

	if (!res2 && op&DOP_CREATE) {
		res |= PERMISSION_DOESALREADYEXIST;
	}
	else if (!(res2 & PERMISSION_NOTFOUND)) {
		if (op&DOP_DELETE && user.GetAliasTarget(logicalDir + _T("/")) != _T("")) {
			res |= PERMISSION_DENIED;
		}
		return res | res2;
	}

	// check dir attributes
	DWORD nAttributes = GetFileAttributesW(physicalDir.c_str());
	if (nAttributes == 0xFFFFFFFF && !(op&DOP_CREATE)) {
		res |= PERMISSION_NOTFOUND;
	}
	else if (!(nAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
		res |= PERMISSION_FILENOTDIR;
	}

	//Finally, a valid path+dirname!
	return res;
}

int CPermissions::CheckFilePermissions(CUser const& user, std::wstring filename, std::wstring currentdir, int op, std::wstring& physicalFile, std::wstring& logicalFile)
{
	std::wstring dir = CanonifyServerDir(currentdir, filename);
	if (dir.empty()) {
		return PERMISSION_INVALIDNAME;
	}
	if (dir == L"/") {
		return PERMISSION_NOTFOUND;
	}

	size_t pos = dir.rfind('/');
	if (pos == std::wstring::npos) {
		return PERMISSION_NOTFOUND;
	}

	logicalFile = dir;

	filename = dir.substr(pos + 1);
	if (pos) {
		dir = dir.substr(0, pos);
	}
	else {
		dir = L"/";
	}

	// dir now is the absolute path (logical server path of course)
	// while filename is the filename

	//Get the physical path
	t_directory directory;
	bool truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);

	if (res) {
		return res;
	}
	if (!directory.bFileRead && op&FOP_READ) {
		res |= PERMISSION_DENIED;
	}
	if (!directory.bFileDelete && op&FOP_DELETE) {
		res |= PERMISSION_DENIED;
	}
	if (!directory.bFileWrite && op&(FOP_CREATENEW | FOP_WRITE | FOP_APPEND)) {
		res |= PERMISSION_DENIED;
	}
	if ((!directory.bDirList || (!directory.bDirSubdirs && !truematch)) && op&FOP_LIST) {
		res |= PERMISSION_DENIED;
	}
	if (op&FOP_DELETE && user.GetAliasTarget(logicalFile + _T("/")) != _T("")) {
		res |= PERMISSION_DENIED;
	}

	physicalFile = directory.dir + L"\\" + static_cast<std::wstring>(filename);
	DWORD nAttributes = GetFileAttributesW(physicalFile.c_str());
	if (nAttributes == 0xFFFFFFFF) {
		if (!(op&(FOP_WRITE | FOP_APPEND | FOP_CREATENEW))) {
			res |= PERMISSION_NOTFOUND;
		}
	}
	else {
		if (nAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			res |= PERMISSION_DIRNOTFILE;
		}
		if (!directory.bFileAppend && op & FOP_APPEND) {
			res |= PERMISSION_DENIED;
		}
		if (!directory.bFileDelete && op & FOP_WRITE) {
			res |= PERMISSION_DENIED;
		}
		if (op & FOP_CREATENEW) {
			res |= PERMISSION_DOESALREADYEXIST;
		}
	}

	//If res is 0 we finally have a valid path+filename!
	return res;
}

std::wstring CPermissions::GetHomeDir(CUser const& user) const
{
	if (user.homedir.empty()) {
		return std::wstring();
	}

	std::wstring path = user.homedir;
	user.DoReplacements(path);

	return path;
}

int CPermissions::GetRealDirectory(std::wstring directory, const CUser &user, t_directory &ret, bool &truematch)
{
	/*
	 * This function translates pathnames from absolute server paths
	 * into absolute local paths.
	 * The given server directory is already an absolute canonified path, so
	 * parsing it is very quick.
	 * To find the absolute local path, we go though each segment of the server
	 * path. For the local path, we start form the homedir and append segments
	 * sequentially or resolve aliases if required.
	 */

	fz::ltrim(directory, L"/"s);

	// Split server path
	// --------------------

	// Split dir into pieces
	std::vector<std::wstring> pathPieces = fz::strtok(directory, L"/");

	// Get absolute local path
	// -----------------------

	//First get the home dir
	std::wstring homepath = GetHomeDir(user);
	if (homepath.empty()) {
		//No homedir found
		return PERMISSION_DENIED;
	}

	// Reassamble path to get local path
	std::wstring path = homepath; // Start with homedir as root

	std::wstring virtualPath = _T("/");
	if (pathPieces.empty()) {
		DWORD nAttributes = GetFileAttributesW(path.c_str());
		if (nAttributes == 0xFFFFFFFF) {
			return PERMISSION_NOTFOUND;
		}
		if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			return PERMISSION_FILENOTDIR;
		}
	}
	else {
		// Go through all pieces
		for (auto const& piece : pathPieces) {
			// Check if piece exists
			virtualPath += piece + _T("/");
			DWORD nAttributes = GetFileAttributesW((path + _T("\\") + piece).c_str());
			if (nAttributes != 0xFFFFFFFF) {
				if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					return PERMISSION_FILENOTDIR;
				}
				path += _T("\\") + piece;
				continue;
			}
			else {
				// Physical path did not exist, check aliases
				std::wstring const& target = user.GetAliasTarget(virtualPath);

				if (!target.empty()) {
					if (target.back() != ':') {
						nAttributes = GetFileAttributesW(target.c_str());
						if (nAttributes == 0xFFFFFFFF) {
							return PERMISSION_NOTFOUND;
						}
						else if (!(nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
							return PERMISSION_FILENOTDIR;
						}
					}
					path = target;
					continue;
				}

			}
			return PERMISSION_NOTFOUND;
		}
	}
	std::wstring const realpath = path;

	// Check permissions
	// -----------------

	/* We got a valid local path, now find the closest matching path within the
	 * permissions.
	 * We do this by sequentially comparing the path with all permissions and
	 * sequentially removing the last path segment until we found a match or
	 * all path segments have been removed
	 * Distinguish the case
	 */
	truematch = true;

	while (!path.empty()) {
		bool bFoundMatch = false;

		// Check user permissions
		for (size_t i = 0; i < user.permissions.size(); ++i) {
			std::wstring permissionPath = user.permissions[i].dir;
			user.DoReplacements(permissionPath);
			if (!fz::stricmp(permissionPath, path)) {
				bFoundMatch = true;
				ret = user.permissions[i];
				break;
			}
		}

		// Check owner (group) permissions
		if (!bFoundMatch && user.pOwner) {
			for (size_t i = 0; i < user.pOwner->permissions.size(); ++i) {
				std::wstring permissionPath = user.pOwner->permissions[i].dir;
				user.DoReplacements(permissionPath);
				if (!fz::stricmp(permissionPath, path)) {
					bFoundMatch = true;
					ret = user.pOwner->permissions[i];
					break;
				}
			}
		}

		if (!bFoundMatch) {
			// No match found, remove last segment and try again
			size_t pos = path.rfind('\\');
			if (pos != std::wstring::npos) {
				path = path.substr(0, pos);
			}
			else {
				return PERMISSION_DENIED;
			}
			truematch = FALSE;
			continue;
		}
		ret.dir = realpath;

		// We can check the bDirSubdirs permission right here
		if (!truematch && !ret.bDirSubdirs) {
			return PERMISSION_DENIED;
		}

		return 0;
	}
	return PERMISSION_NOTFOUND;
}

int CPermissions::ChangeCurrentDir(CUser const& user, std::wstring &currentdir, std::wstring &dir)
{
	std::wstring canonifiedDir = CanonifyServerDir(currentdir, dir);
	if (canonifiedDir.empty()) {
		return PERMISSION_INVALIDNAME;
	}
	dir = canonifiedDir;

	// Get the physical path
	t_directory directory;
	bool truematch;
	int res = GetRealDirectory(dir, user, directory, truematch);
	if (res) {
		return res;
	}
	if (!directory.bDirList) {
		if (!directory.bFileRead && !directory.bFileWrite) {
			return PERMISSION_DENIED;
		}
	}

	// Finally, a valid path!
	currentdir = dir; // Server paths are relative, so we can use the absolute server path

	return 0;
}

CUser CPermissions::GetUser(std::wstring const& username) const
{
	// Get user from username
	auto const& it = m_UsersList.find(username);
	if (it != m_UsersList.end()) {
		return it->second;
	}
	return CUser();
}

bool CPermissions::CheckUserLogin(CUser const& user, std::wstring const& pass, bool noPasswordCheck)
{
	if (user.user.empty()) {
		return false;
	}

	if (user.password.empty() || noPasswordCheck) {
		return true;
	}

	auto tmp = fz::to_utf8(pass);
	if (tmp.empty() && !pass.empty()) {
		// Network broke. Meh...
		return false;
	}

	if (user.salt.empty()) {
		// It should be an old MD5 hashed password
		if (user.password.size() != MD5_HEX_FORM_LENGTH) {
			// But it isn't...
			return false;
		}

		MD5 md5;
		md5.update((unsigned char const*)tmp.c_str(), tmp.size());
		md5.finalize();
		char *res = md5.hex_digest();
		std::string hash = res;
		delete[] res;

		return fz::to_wstring_from_utf8(hash) == user.password;
	}
	
	// It's a salted SHA-512 hash

	auto saltedPassword = fz::to_utf8(pass) + user.salt;
	if (saltedPassword.empty()) {
		return false;
	}

	CAsyncSslSocketLayer ssl(0);
	std::wstring hash = fz::to_wstring(ssl.SHA512(reinterpret_cast<unsigned char const*>(saltedPassword.c_str()), saltedPassword.size()));

	return !hash.empty() && user.password == hash;
}

void CPermissions::UpdateInstances()
{
	simple_lock lock(m_mutex);
	for (auto instance : m_sInstanceList) {
		if (instance != this) {
			ASSERT(instance->m_pPermissionsHelperWindow);
			::PostMessage(instance->m_pPermissionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
		}
	}
}

namespace {
void SetKey(pugi::xml_node & xml, std::string const& name, std::string const& value)
{
	auto option = xml.append_child("Option");
	option.append_attribute("Name").set_value(name.c_str());
	option.text().set(value.c_str());
}

void SetKey(pugi::xml_node & xml, std::string const& name, std::wstring const& value)
{
	SetKey(xml, name, fz::to_utf8(value));
}

void SetKey(pugi::xml_node & xml, std::string const& name, int value)
{
	SetKey(xml, name, fz::to_string(value));
}

void SavePermissions(pugi::xml_node & xml, const t_group &user)
{
	auto permissions = xml.append_child("Permissions");
	
	for (size_t i = 0; i < user.permissions.size(); ++i) {
		auto permission = permissions.append_child("Permission");

		permission.append_attribute("Dir").set_value(fz::to_utf8(user.permissions[i].dir).c_str());
		if (!user.permissions[i].aliases.empty()) {
			auto aliases = permission.append_child("Aliases");
			for (auto const& alias : user.permissions[i].aliases) {
				aliases.append_child("Alias").text().set(fz::to_utf8(alias).c_str());
			}
		}
		SetKey(permission, "FileRead", user.permissions[i].bFileRead ? _T("1") : _T("0"));
		SetKey(permission, "FileWrite", user.permissions[i].bFileWrite ? _T("1") : _T("0"));
		SetKey(permission, "FileDelete", user.permissions[i].bFileDelete ? _T("1") : _T("0"));
		SetKey(permission, "FileAppend", user.permissions[i].bFileAppend ? _T("1") : _T("0"));
		SetKey(permission, "DirCreate", user.permissions[i].bDirCreate ? _T("1") : _T("0"));
		SetKey(permission, "DirDelete", user.permissions[i].bDirDelete ? _T("1") : _T("0"));
		SetKey(permission, "DirList", user.permissions[i].bDirList ? _T("1") : _T("0"));
		SetKey(permission, "DirSubdirs", user.permissions[i].bDirSubdirs ? _T("1") : _T("0"));
		SetKey(permission, "IsHome", user.permissions[i].bIsHome ? _T("1") : _T("0"));
		SetKey(permission, "AutoCreate", user.permissions[i].bAutoCreate ? _T("1") : _T("0"));
	}
}

void ReadPermissions(pugi::xml_node const& xml, t_group &user, bool &bGotHome)
{
	bGotHome = false;
	for (auto permissions = xml.child("Permissions"); permissions; permissions = permissions.next_sibling("Permissions")) {
		for (auto permission = permissions.child("Permission"); permission; permission = permission.next_sibling("Permission")) {
			t_directory dir;
			dir.dir = fz::to_wstring_from_utf8(permission.attribute("Dir").value());
			fz::replace_substrings(dir.dir, L"/", L"\\");
			while (!dir.dir.empty() && dir.dir.back() == '\\') {
				dir.dir.pop_back();
			}
			if (dir.dir.empty()) {
				continue;
			}

			for (auto aliases = permission.child("Aliases"); aliases; aliases = aliases.next_sibling("Aliases")) {
				for (auto xalias = aliases.child("Alias"); xalias; xalias = xalias.next_sibling("Alias")) {
					std::wstring alias = fz::to_wstring_from_utf8(xalias.child_value());
					if (alias.empty() || alias.front() != '/') {
						continue;
					}

					fz::replace_substrings(alias, _T("\\"), _T("/"));
					while (fz::replace_substrings(alias, _T("//"), _T("/")));
					fz::rtrim(alias, L"/"s);
					if (!alias.empty() && alias != L"/"s) {
						dir.aliases.push_back(alias);
					}
				}
			}
			for (auto option = permission.child("Option"); option; option = option.next_sibling("Option")) {
				std::wstring const name = fz::to_wstring_from_utf8(option.attribute("Name").value());
				std::wstring const value = fz::to_wstring_from_utf8(option.child_value());

				if (name == _T("FileRead")) {
					dir.bFileRead = value == _T("1");
				}
				else if (name == _T("FileWrite")) {
					dir.bFileWrite = value == _T("1");
				}
				else if (name == _T("FileDelete")) {
					dir.bFileDelete = value == _T("1");
				}
				else if (name == _T("FileAppend")) {
					dir.bFileAppend = value == _T("1");
				}
				else if (name == _T("DirCreate")) {
					dir.bDirCreate = value == _T("1");
				}
				else if (name == _T("DirDelete")) {
					dir.bDirDelete = value == _T("1");
				}
				else if (name == _T("DirList")) {
					dir.bDirList = value == _T("1");
				}
				else if (name == _T("DirSubdirs")) {
					dir.bDirSubdirs = value == _T("1");
				}
				else if (name == _T("IsHome")) {
					dir.bIsHome = value == _T("1");
				}
				else if (name == _T("AutoCreate")) {
					dir.bAutoCreate = value == _T("1");
				}
			}

			//Avoid multiple home dirs
			if (dir.bIsHome) {
				if (!bGotHome) {
					bGotHome = true;
				}
				else {
					dir.bIsHome = false;
				}
			}

			if (user.permissions.size() < 20000) {
				user.permissions.push_back(dir);
			}
		}
	}
}

void ReadSpeedLimits(pugi::xml_node const& xml, t_group &group)
{
	std::string const prefixes[] = { "Dl"s, "Ul"s };
	const char* names[] = { "Download", "Upload" };

	for (auto speedLimits = xml.child("SpeedLimits"); speedLimits; speedLimits = speedLimits.next_sibling("SpeedLimits")) {
		for (int i = 0; i < 2; ++i) {
			group.nSpeedLimitType[i] = speedLimits.attribute((prefixes[i] + "Type").c_str()).as_int();
			if (group.nSpeedLimitType[i] < 0 || group.nSpeedLimitType[i] > 3) {
				group.nSpeedLimitType[i] = 0;
			}

			group.nSpeedLimit[i] = speedLimits.attribute((prefixes[i] + "Limit").c_str()).as_int();
			if (group.nSpeedLimit[i] < 0) {
				group.nSpeedLimit[i] = 0;
			}
			else if (group.nSpeedLimit[i] > 1048576) {
				group.nSpeedLimit[i] = 1048576;
			}

			group.nBypassServerSpeedLimit[i] = speedLimits.attribute(("Server" + prefixes[i] + "LimitBypass").c_str()).as_int();
			if (group.nBypassServerSpeedLimit[i] < 0 || group.nBypassServerSpeedLimit[i] > 2) {
				group.nBypassServerSpeedLimit[i] = 0;
			}

			for (auto direction = speedLimits.child(names[i]); direction; direction = direction.next_sibling(names[i])) {
				for (auto rule = direction.child("Rule"); rule; rule = rule.next_sibling("Rule")) {
					CSpeedLimit limit;
					if (!limit.Load(rule)) {
						continue;
					}

					if (group.SpeedLimits[i].size() < 20000) {
						group.SpeedLimits[i].push_back(limit);
					}
				}
			}
		}
	}
}

void SaveSpeedLimits(pugi::xml_node & xml, t_group const& group)
{
	auto speedLimits = xml.append_child("SpeedLimits");

	std::string const prefixes[] = { "Dl"s, "Ul"s };
	const char* names[] = { "Download", "Upload" };

	for (int i = 0; i < 2; ++i) {
		speedLimits.append_attribute((prefixes[i] + "Type").c_str()).set_value(group.nSpeedLimitType[i]);
		speedLimits.append_attribute((prefixes[i] + "Limit").c_str()).set_value(group.nSpeedLimit[i]);
		speedLimits.append_attribute(("Server" + prefixes[i] + "LimitBypass").c_str()).set_value(group.nBypassServerSpeedLimit[i]);

		auto direction = speedLimits.append_child(names[i]);

		for (auto const& limit : group.SpeedLimits[i]) {
			auto rule = direction.append_child("Rule");
			limit.Save(rule);
		}
	}
}

void SaveIpFilter(pugi::xml_node & xml, t_group const& group)
{
	auto filter = xml.append_child("IpFilter");

	auto disallowed = filter.append_child("Disallowed");
	for (auto const& disallowedIP : group.disallowedIPs) {
		disallowed.append_child("IP").text().set(disallowedIP.c_str());
	}

	auto allowed = filter.append_child("Allowed");
	for (auto const& allowedIP : group.allowedIPs) {
		allowed.append_child("IP").text().set(allowedIP.c_str());
	}
}
}

bool CPermissions::GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength)
{
	// This function returns all account data as a command string which will be
	// sent to the user interface.
	if (!pBuffer || !nBufferLength) {
		return false;
	}

	simple_lock lock(m_mutex);

	// First calculate the required buffer length
	DWORD len = 3 * 2;
	if (m_sGroupsList.size() > 0xffffff || m_sUsersList.size() > 0xffffff) {
		return false;
	}
	for (auto const& group : m_sGroupsList) {
		len += group.GetRequiredBufferLen();
	}
	for (auto const& iter : m_sUsersList) {
		len += iter.second.GetRequiredBufferLen();
	}

	// Allocate memory
	*pBuffer = new unsigned char[len];
	unsigned char* p  = *pBuffer;

	// Write groups to buffer
	*p++ = ((m_sGroupsList.size() / 256) / 256) & 255;
	*p++ = (m_sGroupsList.size() / 256) % 256;
	*p++ = m_sGroupsList.size() % 256;
	for (auto const& group : m_sGroupsList) {
		p = group.FillBuffer(p);
		if (!p) {
			delete [] *pBuffer;
			*pBuffer = nullptr;
			return false;
		}
	}

	// Write users to buffer
	*p++ = ((m_sUsersList.size() / 256) / 256) & 255;
	*p++ = (m_sUsersList.size() / 256) % 256;
	*p++ = m_sUsersList.size() % 256;
	for (auto const& iter : m_sUsersList ) {
		p = iter.second.FillBuffer(p);
		if (!p) {
			delete [] *pBuffer;
			*pBuffer = nullptr;
			return false;
		}
	}

	*nBufferLength = len;

	return true;
}

bool CPermissions::ParseUsersCommand(unsigned char *pData, DWORD dwDataLength)
{
	t_GroupsList groupsList;
	t_UsersList usersList;

	unsigned char *p = pData;
	unsigned char* endMarker = pData + dwDataLength;

	if (dwDataLength < 3) {
		return false;
	}
	int num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;

	int i;
	for (i = 0; i < num; ++i) {
		t_group group;
		p = group.ParseBuffer(p, endMarker - p);
		if (!p) {
			return false;
		}

		if (!group.group.empty()) {
			//Set a home dir if no home dir could be read
			bool bGotHome = false;
			for (unsigned int dir = 0; dir < group.permissions.size(); ++dir) {
				if (group.permissions[dir].bIsHome) {
					bGotHome = true;
					break;
				}
			}

			if (!bGotHome && !group.permissions.empty()) {
				group.permissions.begin()->bIsHome = true;
			}

			groupsList.push_back(group);
		}
	}

	if ((endMarker - p) < 3) {
		return false;
	}
	num = *p * 256 * 256 + p[1] * 256 + p[2];
	p += 3;

	for (i = 0; i < num; ++i) {
		CUser user;

		p = user.ParseBuffer(p, endMarker - p);
		if (!p) {
			return false;
		}

		if (!user.user.empty()) {
			user.pOwner = nullptr;
			if (!user.group.empty()) {
				for (auto const& group : groupsList) {
					if (group.group == user.group) {
						user.pOwner = &group;
						break;
					}
				}
				if (!user.pOwner) {
					user.group.clear();
				}
			}

			if (!user.pOwner) {
				//Set a home dir if no home dir could be read
				BOOL bGotHome = false;
				for (unsigned int dir = 0; dir < user.permissions.size(); ++dir) {
					if (user.permissions[dir].bIsHome) {
						bGotHome = true;
						break;
					}
				}

				if (!bGotHome && !user.permissions.empty()) {
					user.permissions.begin()->bIsHome = true;
				}
			}

			std::vector<t_directory>::iterator iter;
			for (iter = user.permissions.begin(); iter != user.permissions.end(); ++iter) {
				if (iter->bIsHome) {
					user.homedir = iter->dir;
					break;
				}
			}
			if (user.homedir.empty() && user.pOwner) {
				for (auto const& perm : user.pOwner->permissions) {
					if (perm.bIsHome) {
						user.homedir = perm.dir;
						break;
					}
				}
			}

			user.PrepareAliasMap();


			std::wstring name = user.user;
			std::transform(name.begin(), name.end(), name.begin(), std::towlower);
			usersList[name] = user;
		}
	}

	// Update the account list
	{
		simple_lock lock(m_mutex);
		m_sGroupsList = groupsList;
		m_sUsersList = usersList;
	}
	UpdatePermissions(true);
	UpdateInstances();

	return SaveSettings();
}

bool CPermissions::SaveSettings()
{
	// Write the new account data into xml file

	XML::file *xml = COptions::GetXML();
	if (!xml) {
		return false;
	}

	do {} while (xml->root.remove_child("Groups"));
	auto groups = xml->root.append_child("Groups");

	//Save the changed user details
	for (auto const& group : m_GroupsList) {
		auto xgroup = groups.append_child("Group");

		xgroup.append_attribute("Name").set_value(fz::to_utf8(group.group).c_str());

		SetKey(xgroup, "Bypass server userlimit", group.nBypassUserLimit);
		SetKey(xgroup, "User Limit", group.nUserLimit);
		SetKey(xgroup, "IP Limit", group.nIpLimit);
		SetKey(xgroup, "Enabled", group.nEnabled);
		SetKey(xgroup, "Comments", group.comment);
		SetKey(xgroup, "ForceSsl", group.forceSsl);

		SaveIpFilter(xgroup, group);
		SavePermissions(xgroup, group);
		SaveSpeedLimits(xgroup, group);
	}

	do {} while (xml->root.remove_child("Users"));
	auto users = xml->root.append_child("Users");

	//Save the changed user details
	for (auto const& iter : m_UsersList) {
		CUser const& user = iter.second;

		auto xuser = users.append_child("User");

		xuser.append_attribute("Name").set_value(fz::to_utf8(user.user).c_str());

		SetKey(xuser, "Pass", user.password.c_str());
		SetKey(xuser, "Salt", user.salt.c_str());
		SetKey(xuser, "Group", user.group.c_str());
		SetKey(xuser, "Bypass server userlimit", user.nBypassUserLimit);
		SetKey(xuser, "User Limit", user.nUserLimit);
		SetKey(xuser, "IP Limit", user.nIpLimit);
		SetKey(xuser, "Enabled", user.nEnabled);
		SetKey(xuser, "Comments", user.comment.c_str());
		SetKey(xuser, "ForceSsl", user.forceSsl);

		SaveIpFilter(xuser, user);
		SavePermissions(xuser, user);
		SaveSpeedLimits(xuser, user);
	}
	if (!COptions::FreeXML(xml, true)) {
		return false;
	}

	return true;
}

bool CPermissions::Init()
{
	simple_lock lock(m_mutex);
	m_pPermissionsHelperWindow = new CPermissionsHelperWindow(this);
	if (m_sInstanceList.empty() && m_sUsersList.empty() && m_sGroupsList.empty()) {
		// It's the first time Init gets called after application start, read
		// permissions from xml file.
		ReadSettings();
	}
	UpdatePermissions(false);

	auto it = std::find(m_sInstanceList.begin(), m_sInstanceList.end(), this);
	if (it == m_sInstanceList.end()) {
		m_sInstanceList.push_back(this);
	}

	return true;
}

void CPermissions::AutoCreateDirs(CUser const& user)
{
	// Create missing directores after a user has logged on
	for (auto const& permission : user.permissions) {
		if (permission.bAutoCreate) {
			std::wstring dir = permission.dir;
			user.DoReplacements(dir);

			dir += _T("\\");
			std::wstring str;
			while (!dir.empty()) {
				size_t pos = dir.find('\\');
				if (pos == std::wstring::npos) {
					break;
				}
				std::wstring piece = dir.substr(0, pos + 1);
				dir = dir.substr(pos + 1);

				str += piece;
				CreateDirectoryW(str.c_str(), 0);
			}
		}
	}
	if (user.pOwner) {
		for (auto const& permission : user.pOwner->permissions) {
			if (permission.bAutoCreate) {
				CStdString dir = permission.dir;
				user.DoReplacements(dir);

				dir += _T("\\");
				std::wstring str;
				while (!dir.empty()) {
					size_t pos = dir.find('\\');
					if (pos == std::wstring::npos) {
						break;
					}
					std::wstring piece = dir.substr(0, pos + 1);
					dir = dir.substr(pos + 1);

					str += piece;
					CreateDirectoryW(str.c_str(), 0);
				}
			}
		}
	}
}


void CPermissions::ReloadConfig()
{
	ReadSettings();
	UpdatePermissions(true);
	UpdateInstances();
}

namespace {
void ReadIpFilter(pugi::xml_node const& xml, t_group & group)
{
	for (auto filter = xml.child("IpFilter"); filter; filter = filter.next_sibling("IpFilter")) {
		for (auto disallowed = filter.child("Disallowed"); disallowed; disallowed = disallowed.next_sibling("Disallowed")) {
			for (auto xip = disallowed.child("IP"); xip; xip = xip.next_sibling("xip")) {
				std::wstring ip = fz::to_wstring_from_utf8(xip.child_value());
				if (ip.empty()) {
					continue;
				}

				if (group.disallowedIPs.size() >= 30000) {
					break;
				}

				if (ip == _T("*")) {
					group.disallowedIPs.push_back(ip);
				}
				else {
					if (IsValidAddressFilter(ip)) {
						group.disallowedIPs.push_back(ip);
					}
				}
			}
		}
		for (auto allowed = filter.child("Allowed"); allowed; allowed = allowed.next_sibling("Aallowed")) {
			for (auto xip = allowed.child("IP"); xip; xip = xip.next_sibling("xip")) {
				std::wstring ip = fz::to_wstring_from_utf8(xip.child_value());
				if (ip.empty()) {
					continue;
				}

				if (group.allowedIPs.size() >= 30000) {
					break;
				}

				if (ip == _T("*")) {
					group.allowedIPs.push_back(ip);
				}
				else {
					if (IsValidAddressFilter(ip)) {
						group.allowedIPs.push_back(ip);
					}
				}
			}
		}
	}
}
}

std::wstring CPermissions::CanonifyServerDir(std::wstring currentDir, std::wstring newDir) const
{
	/*
	 * CanonifyPath takes the current and the new server dir as parameter,
	 * concats the paths if neccessary and canonifies the dir:
	 * - remove dot-segments
	 * - convert backslashes into slashes
	 * - remove double slashes
	 */

	if (newDir.empty()) {
		return currentDir;
	}

	// Make segment separators pretty
	fz::replace_substrings(newDir, _T("\\"), _T("/"));
	while (fz::replace_substrings(newDir, _T("//"), _T("/")));

	if (newDir == _T("/")) {
		return newDir;
	}

	// This list will hold the individual path segments
	std::vector<std::wstring> piecelist;

	/*
	 * Check the type of the path: Absolute or relative?
	 * On relative paths, use currentDir as base, else use
	 * only dir.
	 */
	if (newDir.front() != '/') {
		// New relative path, split currentDir and add it to the piece list.
		piecelist = fz::strtok(currentDir, L"/"s);
	}

	/*
	 * Now split up the new dir into individual segments. Here we
	 * check for dot segments and remove the proper number of segments
	 * from the piece list on dots.
	 */

	for (auto const& segment : fz::strtok(newDir, L"/"s)) {
		bool allDots = true;
		int dotCount = 0;
		for (size_t i = 0; i < segment.size(); ++i) {
			if (segment[i] != '.') {
				allDots = false;
				break;
			}
			else {
				++dotCount;
			}
		}

		if (allDots) {
			while (--dotCount) {
				if (!piecelist.empty()) {
					piecelist.pop_back();
				}
			}
		}
		else {
			piecelist.push_back(segment);
		}
	}

	// Reassemble the directory
	std::wstring result;

	if (piecelist.empty()) {
		return L"/"s;
	}

	// List of reserved filenames which may not be used on a Windows system
	static std::array<std::wstring, 23> const reservedNames = {
		L"CON"s,  L"PRN"s,  L"AUX"s,  L"CLOCK$"s, L"NUL"s,
		L"COM1"s, L"COM2"s, L"COM3"s, L"COM4"s, L"COM5"s,
		L"COM6"s, L"COM7"s, L"COM8"s, L"COM9"s,
		L"LPT1"s, L"LPT2"s, L"LPT3"s, L"LPT4"s, L"LPT5"s,
		L"LPT6"s, L"LPT7"s, L"LPT8"s, L"LPT9"s,
	};

	for (auto const& piece : piecelist) {
		// Check for reserved filenames
		size_t pos = piece.find('.');

		std::wstring upper;
		if (pos != std::wstring::npos) {
			upper = fz::str_toupper_ascii(piece.substr(0, pos));
		}
		else {
			upper = fz::str_toupper_ascii(piece);
		}

		for (auto const& reserved : reservedNames) {
			if (upper == reserved) {
				return std::wstring();
			}
		}

		result += _T("/") + piece;
	}

	// Now dir is the canonified absolute server path.
	return result;
}

int CPermissions::GetFact(CUser const& user, std::wstring const& currentDir, std::wstring file, std::wstring& fact, std::wstring& logicalName, bool enabledFacts[3])
{
	std::wstring dir = CanonifyServerDir(currentDir, file);
	if (dir.empty()) {
		return PERMISSION_INVALIDNAME;
	}
	logicalName = dir;

	t_directory directory;
	bool bTruematch;
	int res = GetRealDirectory(dir, user, directory, bTruematch);
	if (res == PERMISSION_FILENOTDIR) {
		if (dir == _T("/")) {
			return res;
		}

		size_t pos = dir.rfind('/');
		if (pos == std::wstring::npos) {
			return res;
		}

		std::wstring dir2;
		if (pos) {
			dir2 = dir.substr(0, pos);
		}
		else {
			dir2 = _T("/");
		}

		std::wstring fn = dir.substr(pos + 1);
		int res = GetRealDirectory(dir2, user, directory, bTruematch);
		if (res) {
			return res | PERMISSION_FILENOTDIR;
		}

		if (!directory.bFileRead) {
			return PERMISSION_DENIED;
		}

		file = directory.dir + L"\\" + fn;

		if (enabledFacts[0]) {
			fact = _T("type=file;");
		}
		else {
			fact = _T("");
		}
	}
	else if (res) {
		return res;
	}
	else {
		if (!directory.bDirList) {
			return PERMISSION_DENIED;
		}

		if (!bTruematch && !directory.bDirSubdirs) {
			return PERMISSION_DENIED;
		}

		file = directory.dir;

		if (enabledFacts[0]) {
			fact = _T("type=dir;");
		}
		else {
			fact = _T("");
		}
	}

	WIN32_FILE_ATTRIBUTE_DATA status{};
	if (GetStatus64(file.c_str(), status)) {
		if (enabledFacts[1] && !(status.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			fact += fz::sprintf(L"size=%d;", GetLength64(status));
		}

		if (enabledFacts[2]) {
			// Get last modification time
			FILETIME ftime = status.ftLastWriteTime;
			if (ftime.dwHighDateTime || ftime.dwLowDateTime) {
				SYSTEMTIME time;
				FileTimeToSystemTime(&ftime, &time);
				fact += fz::sprintf(L"modify=%04d%02d%02d%02d%02d%02d;",
					time.wYear,
					time.wMonth,
					time.wDay,
					time.wHour,
					time.wMinute,
					time.wSecond);
			}
		}
	}

	fact += _T(" ") + logicalName;

	return 0;
}

void CUser::PrepareAliasMap()
{
	/*
	 * Prepare the alias map.
	 * For fast access, aliases are stored as key/value pairs.
	 * The key is the folder part of the alias.
	 * The value is a structure containing the name of the alias
	 * and the target folder.
	 * Example:
	 * Shared folder c:\myfolder, alias /myotherfolder/myalias
	 * Key: /myotherfolder, Value = myalias, c:\myfolder
	 */

	virtualAliases.clear();
	for (auto const& permission : permissions) {
		for (auto alias : permission.aliases) {
			DoReplacements(alias);

			if (alias[0] != '/') {
				continue;
			}

			size_t pos = alias.rfind('/');
			std::wstring dir = alias.substr(0, pos);
			if (dir.empty()) {
				dir = L"/";
			}
			virtualAliasNames.insert(std::make_pair(dir, alias.substr(pos + 1)));
			virtualAliases[alias + L"/"] = permission.dir;
			DoReplacements(virtualAliases[alias + L"/"]);
		}
	}

	if (!pOwner) {
		return;
	}

	for (auto const& permission : pOwner->permissions) {
		for (auto alias : permission.aliases) {
			DoReplacements(alias);

			if (alias[0] != '/') {
				continue;
			}

			int pos = alias.rfind('/');
			std::wstring dir = alias.substr(0, pos);
			if (dir.empty()) {
				dir = L"/";
			}
			virtualAliasNames.insert(std::pair<CStdString, CStdString>(dir, alias.substr(pos + 1)));
			virtualAliases[alias + L"/"] = permission.dir;
			DoReplacements(virtualAliases[alias + L"/"]);
		}
	}
}

std::wstring CUser::GetAliasTarget(std::wstring const& virtualPath) const
{
	// Find the target for the alias with the specified path and name
	for (auto const& alias : virtualAliases) {
		if (!fz::stricmp(alias.first, virtualPath)) {
			return alias.second;
		}
	}

	return std::wstring();
}

void ReadCommonOption(pugi::xml_node const& option, std::string const& name, t_group & group)
{
	if (name == "Bypass server userlimit") {
		group.nBypassUserLimit = option.text().as_int();
	}
	else if (name == "User Limit") {
		group.nUserLimit = option.text().as_int();

		if (group.nUserLimit < 0 || group.nUserLimit > 999999999) {
			group.nUserLimit = 0;
		}
		else if (group.nUserLimit > 0 && group.nUserLimit < 15) {
			group.nUserLimit = 15;
		}
	}
	else if (name == "IP Limit") {
		group.nIpLimit = option.text().as_int();

		if (group.nIpLimit < 0 || group.nIpLimit > 999999999) {
			group.nIpLimit = 0;
		}
		else if (group.nIpLimit > 0 && group.nIpLimit < 15) {
			group.nIpLimit = 15;
		}
	}
	else if (name == "Enabled") {
		group.nEnabled = option.text().as_int();
	}
	else if (name == "Comments") {
		group.comment = fz::to_wstring_from_utf8(option.child_value());
	}
	else if (name == "ForceSsl") {
		group.forceSsl = option.text().as_int();
	}
}

void CPermissions::ReadSettings()
{
	XML::file *xml  = COptions::GetXML();
	bool fileIsDirty = false; //By default, nothing gets changed when reading the file
	if (!xml) {
		return;
	}

	simple_lock lock(m_mutex);

	m_sGroupsList.clear();
	m_sUsersList.clear();

	auto groups = xml->root.child("Groups");
	if (groups) {
		for (auto xgroup = groups.child("Group"); xgroup; xgroup = xgroup.next_sibling("Group")) {
			t_group group;
			group.nBypassUserLimit = 2;
			group.group = fz::to_wstring_from_utf8(xgroup.attribute("Name").value());
			if (group.group.empty()) {
				continue;
			}

			for (auto option = xgroup.child("Option"); option; option = option.next_sibling("Option")) {
				std::string name = option.attribute("Name").value();
				
				ReadCommonOption(option, name, group);
			}
			
			ReadIpFilter(xgroup, group);

			bool bGotHome = false;
			ReadPermissions(xgroup, group, bGotHome);
			//Set a home dir if no home dir could be read
			if (!bGotHome && !group.permissions.empty()) {
				group.permissions.begin()->bIsHome = true;
			}

			ReadSpeedLimits(xgroup, group);

			if (m_sGroupsList.size() < 200000) {
				m_sGroupsList.push_back(group);
			}
		}
	}

	auto users = xml->root.child("Users");
	if (users) {
		for (auto xuser = users.child("User"); xuser; xuser = xuser.next_sibling("User")) {
			CUser user;
			user.nBypassUserLimit = 2;
			user.user = fz::to_wstring_from_utf8(xuser.attribute("Name").value());
			if (user.user.empty()) {
				continue;
			}

			for (auto option = xuser.child("Option"); option; option = option.next_sibling("Option")) {
				std::string name = option.attribute("Name").value();

				if (name == "Pass") {
					user.password = fz::to_wstring_from_utf8(option.child_value());
				}
				else if (name == "Salt") {
					user.salt = option.child_value();
				}
				else if (name == "Group") {
					user.group = fz::to_wstring_from_utf8(option.child_value());
				}
				else {
					ReadCommonOption(option, name, user);
				}
			}

			// If provided password is not a salted SHA512 hash and neither an old MD5 hash, convert it into a salted SHA512 hash
			if (!user.password.empty() && user.salt.empty() && user.password.size() != MD5_HEX_FORM_LENGTH) {
				user.generateSalt();

				auto saltedPassword = fz::to_utf8(user.password) + user.salt;
				if (saltedPassword.empty()) {
					// We skip this user
					continue;
				}

				CAsyncSslSocketLayer ssl(0);
				CStdString hash = ssl.SHA512(reinterpret_cast<unsigned char const*>(saltedPassword.c_str()), saltedPassword.size());

				if (hash.empty()) {
					// We skip this user
					continue;
				}

				user.password = hash;

				fileIsDirty = true;
			}

		
			if (!user.group.empty()) {
				for (auto const& group : m_sGroupsList) {
					if (group.group == user.group) {
						user.pOwner = &group;
						break;
					}
				}

				if (!user.pOwner) {
					user.group.clear();
				}
			}

			ReadIpFilter(xuser, user);

			bool bGotHome = false;
			ReadPermissions(xuser, user, bGotHome);
			user.PrepareAliasMap();

			// Set a home dir if no home dir could be read
			if (!bGotHome && !user.pOwner) {
				if (!user.permissions.empty()) {
					user.permissions.begin()->bIsHome = true;
				}
			}

			std::vector<t_directory>::iterator iter;
			for (iter = user.permissions.begin(); iter != user.permissions.end(); iter++) {
				if (iter->bIsHome) {
					user.homedir = iter->dir;
					break;
				}
			}
			if (user.homedir.empty() && user.pOwner) {
				for (auto const& perm : user.pOwner->permissions) {
					if (perm.bIsHome) {
						user.homedir = perm.dir;
						break;
					}
				}
			}

			ReadSpeedLimits(xuser, user);

			if (m_sUsersList.size() < 200000) {
				CStdString name = user.user;
				name.ToLower();
				m_sUsersList[name] = user;
			}
		}
	}
	
	COptions::FreeXML(xml, false);

	UpdatePermissions(false);

	if (fileIsDirty) {
		SaveSettings();
	}
}

// Replace :u and :g (if a group it exists)
void CUser::DoReplacements(std::wstring& path) const
{
	fz::replace_substrings(path, L":u", user);
	fz::replace_substrings(path, L":U", user);
	if (!group.empty()) {
		fz::replace_substrings(path, L":g", group);
		fz::replace_substrings(path, L":G", group);
	}
}

bool CPermissions::WildcardMatch(std::wstring string, std::wstring pattern) const
{
	if (pattern == _T("*") || pattern == _T("*.*")) {
		return true;
	}

	std::transform(string.begin(), string.end(), string.begin(), std::towlower);
	std::transform(pattern.begin(), pattern.end(), pattern.begin(), std::towlower);

	// Do a really primitive wildcard check, does even ignore ?

	bool starFirst = false;
	while (!pattern.empty()) {
		size_t pos = pattern.find('*');
		if (pos == std::wstring::npos) {
			if (starFirst) {
				if (string.size() > pattern.size()) {
					string = string.substr(string.size() - pattern.size());
				}
			}
			if (pattern != string) {
				return false;
			}
			else {
				return true;
			}
		}
		else if (!pos) {
			starFirst = true;
			pattern = pattern.substr(1);
		}
		else {
			size_t pos2 = string.find(pattern.substr(0, pos));
			if (pos2 == std::wstring::npos) {
				return false;
			}
			if (pos2 && !starFirst) {
				return false;
			}
			pattern = pattern.substr(pos + 1);
			string = string.substr(pos2 + pos);

			starFirst = true;
		}
	}
	return true;
}

void CPermissions::UpdatePermissions(bool notifyOwner)
{
	{
		simple_lock lock(m_mutex);

		// Clear old group data and copy over the new data
		m_GroupsList.clear();
		for (auto const& group : m_sGroupsList) {
			m_GroupsList.push_back(group);
		}

		// Clear old user data and copy over the new data
		m_UsersList.clear();
		for (auto const& it : m_sUsersList) {
			CUser user = it.second;
			user.pOwner = nullptr;
			if (!user.group.empty()) { // Set owner
				for (auto const& group : m_GroupsList) {
					if (group.group == user.group) {
						user.pOwner = &group;
						break;
					}
				}
			}
			m_UsersList[it.first] = user;
		}
	}

	if (notifyOwner && updateCallback_) {
		updateCallback_();
	}
}
