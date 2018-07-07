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

#ifndef FILEZILLA_SERVER_SERVICE_PERMISSIONS_HEADER
#define FILEZILLA_SERVER_SERVICE_PERMISSIONS_HEADER

#include "Accounts.h"

#include <functional>

#define FOP_READ		0x01
#define FOP_WRITE		0x02
#define FOP_DELETE		0x04
#define FOP_APPEND		0x08
#define FOP_CREATENEW	0x10
#define DOP_DELETE		0x20
#define DOP_CREATE		0x40
#define FOP_LIST		0x80

#define PERMISSION_DENIED			0x01
#define PERMISSION_NOTFOUND			0x02
#define PERMISSION_DIRNOTFILE		(0x04 | PERMISSION_DOESALREADYEXIST)
#define PERMISSION_FILENOTDIR		(0x08 | PERMISSION_DOESALREADYEXIST)
#define PERMISSION_DOESALREADYEXIST	0x10
#define PERMISSION_INVALIDNAME		0x20

#define MD5_HEX_FORM_LENGTH			 32
#define SHA512_HEX_FORM_LENGTH		128		//Length of the hex string representation of a SHA512 hash

class CPermissionsHelperWindow;
class COptions;

class CUser final : public t_user
{
public:
	std::wstring homedir;

	// Replace :u and :g (if a group it exists)
	void DoReplacements(std::wstring& path) const;

	/*
	 * t_alias is used in the alias maps.
	 * See implementation of PrepareAliasMap for a detailed
	 * description
	 */
	struct t_alias
	{
		std::wstring targetFolder;
		std::wstring name;
	};

	void PrepareAliasMap();

	// GetAliasTarget returns the target of the alias with the specified
	// path or returns an empty string if the alias can't be found.
	std::wstring GetAliasTarget(std::wstring const& virtualPath) const;

	std::map<std::wstring, std::wstring> virtualAliases;
	std::multimap<std::wstring, std::wstring> virtualAliasNames;
};

struct t_dirlisting
{
	t_dirlisting()
		: len()
	{
	}

	char buffer[8192];
	unsigned int len;
};

enum _facts {
	fact_type,
	fact_size,
	fact_modify,
	fact_perm
};

class CPermissions final
{
public:
	CPermissions(std::function<void()> const& updateCallback);
	~CPermissions();

	typedef void (*addFunc_t)(std::list<t_dirlisting> &result, bool isDir, std::string const& name, const t_directory& directory, __int64 size, FILETIME* pTime, std::string const& dirToDisplay, bool *enabledFacts);
protected:
	/*
	 * CanonifyPath takes the current and the new server dir as parameter,
	 * concats the paths if neccessary and canonifies the dir:
	 * - remove dot-segments
	 * - convert backslashes into slashes
	 * - remove double slashes
	 */
	std::wstring CanonifyServerDir(std::wstring currentDir, std::wstring newDir) const;

public:
	// Change current directory to the specified directory. Used by CWD and CDUP
	int ChangeCurrentDir(CUser const& user, std::wstring& currentdir, std::wstring &dir);

	// Retrieve a directory listing. Pass the actual formatting function as last parameter.
	int GetDirectoryListing(CUser const& user, std::wstring currentDir, std::wstring dirToDisplay,
							 std::list<t_dirlisting> &result, std::wstring& physicalDir,
							 std::wstring& logicalDir,
							 addFunc_t addFunc,
							 bool *enabledFacts = 0);

	// Full directory listing with all details. Used by LIST command
	static void AddLongListingEntry(std::list<t_dirlisting> &result, bool isDir, std::string const& name, t_directory const& directory, __int64 size, FILETIME* pTime, std::string const& dirToDisplay, bool *);

	// Directory listing with just the filenames. Used by NLST command
	static void AddShortListingEntry(std::list<t_dirlisting> &result, bool isDir, std::string const& name, t_directory const& directory, __int64 size, FILETIME* pTime, std::string const& dirToDisplay, bool *);

	// Directory listing format used by MLSD
	static void AddFactsListingEntry(std::list<t_dirlisting> &result, bool isDir, std::string const& name, t_directory const& directory, __int64 size, FILETIME* pTime, std::string const& dirToDisplay, bool *enabledFacts);

	int CheckDirectoryPermissions(CUser const& user, std::wstring dirname, std::wstring currentdir, int op, std::wstring &physicalDir, std::wstring &logicalDir);
	int CheckFilePermissions(CUser const& user, std::wstring filename, std::wstring currentdir, int op, std::wstring &physicalDir, std::wstring &logicalDir);

	CUser GetUser(std::wstring const& username) const;
	bool CheckUserLogin(CUser const& user, std::wstring const& pass, bool noPasswordCheck = false);

	bool GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength);
	bool ParseUsersCommand(unsigned char *pData, DWORD dwDataLength);
	void AutoCreateDirs(CUser const& user);
	void ReloadConfig();

	int GetFact(CUser const& user, std::wstring const& currentDir, std::wstring file, std::wstring& fact, std::wstring& logicalName, bool enabledFacts[3]);

protected:
	std::wstring GetHomeDir(CUser const& user) const;

	bool Init();
	void UpdateInstances();
	void UpdatePermissions(bool notifyOwner);

	void ReadSettings();
	bool SaveSettings();

	int GetRealDirectory(std::wstring directory, CUser const& user, t_directory &ret, bool &truematch);

	static std::recursive_mutex m_mutex;

	bool WildcardMatch(std::wstring string, std::wstring pattern) const;

	typedef std::map<std::wstring, CUser> t_UsersList;
	typedef std::vector<t_group> t_GroupsList;
	static t_UsersList m_sUsersList;
	static t_GroupsList m_sGroupsList;
	t_UsersList m_UsersList;
	t_GroupsList m_GroupsList;

	static std::vector<CPermissions *> m_sInstanceList;
	CPermissionsHelperWindow *m_pPermissionsHelperWindow;

	friend CPermissionsHelperWindow;

	std::function<void()> const updateCallback_;
};

#endif
