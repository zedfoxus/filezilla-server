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

#ifndef FILEZILLA_SERVER_SERVICE_OPTIONS_HEADER
#define FILEZILLA_SERVER_SERVICE_OPTIONS_HEADER

#include "OptionTypes.h"
#include "SpeedLimit.h"

#include <Vector>

namespace pugi {
class xml_node;
}

namespace XML {
class file;
}

class COptionsHelperWindow;
class COptions final
{
	friend COptionsHelperWindow;

public:
	bool GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength);
	std::wstring GetOption(int nOptionID);
	_int64 GetOptionVal(int nOptionID);

	COptions();
	~COptions();

	static XML::file *GetXML();
	static bool FreeXML(XML::file *pXML, bool save);

	bool ParseOptionsCommand(unsigned char *pData, DWORD dwDataLength, bool bFromLocal = false);
	void SetOption(int nOptionID, std::wstring str, bool save = true);
	void SetOption(int nOptionID, int64_t value, bool save = true);
	int GetCurrentSpeedLimit(int nMode);
	void ReloadConfig();

protected:
	static std::recursive_mutex m_mutex;
	static std::vector<COptions *> m_InstanceList;

	void SaveOptions();

	bool ReadSpeedLimits(pugi::xml_node const& settings);
	bool SaveSpeedLimits(pugi::xml_node & settings);

	static SPEEDLIMITSLIST m_sSpeedLimits[2];
	SPEEDLIMITSLIST m_SpeedLimits[2];

	struct t_OptionsCache {
		bool bCached;
		std::wstring str;
		int64_t value;
	} m_OptionsCache[OPTIONS_NUM];
	static t_OptionsCache m_sOptionsCache[OPTIONS_NUM];

	void Init(bool reload = false);
	static bool m_bInitialized;

	static void UpdateInstances();
	COptionsHelperWindow *m_pOptionsHelperWindow{};
};

#endif
