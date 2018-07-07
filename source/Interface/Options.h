// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2016 - Tim Kosse <tim.kosse@filezilla-project.org>

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

#ifndef FILEZILLA_SERVER_INTERFACE_OPTIONS_HEADER
#define FILEZILLA_SERVER_INTERFACE_OPTIONS_HEADER

#define IOPTION_STARTMINIMIZED 0
#define IOPTION_LASTSERVERADDRESS 1
#define IOPTION_LASTSERVERPORT 2
#define IOPTION_LASTSERVERPASS 3
#define IOPTION_ALWAYS 4
#define IOPTION_USERSORTING 5
#define IOPTION_FILENAMEDISPLAY 6
#define IOPTION_RECONNECTCOUNT 7

#define IOPTIONS_NUM 8

class COptionsDlg;
class COptions final
{
	friend COptionsDlg;

public:
	std::wstring GetOption(int nOptionID);
	__int64 GetOptionVal(int nOptionID);
	void SetOption(int nOptionID, std::wstring const& value);
	void SetOption(int nOptionID, int64_t value);

protected:
	std::wstring GetFileName(bool for_saving);

	void SaveOption(int nOptionID, std::wstring const& value);

	struct t_OptionsCache final
	{
		bool bCached{};
		CTime createtime;
		std::wstring str;
		int64_t value{};
	} m_OptionsCache[IOPTIONS_NUM];
	void Init();
	static bool m_bInitialized;
};

#endif
