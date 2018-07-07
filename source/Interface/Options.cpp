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

#include "stdafx.h"
#include "options.h"
#include "../version.h"
#include "filezilla server.h"
#include "../xml_utils.h"

#include <libfilezilla/string.hpp>

bool COptions::m_bInitialized = false;

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptions

struct t_Option final
{
	std::string name;
	int nType;
};

static const t_Option m_Options[IOPTIONS_NUM] = {
	{ "Start Minimized",        1 },
	{ "Last Server Address",    0 },
	{ "Last Server Port",       1 },
	{ "Last Server Password",   0 },
	{ "Always use last server", 1 },
	{ "User Sorting",           1 },
	{ "Filename Display",       1 },
	{ "Max reconnect count",    1 },
};

void COptions::SetOption(int nOptionID, int64_t value)
{
	Init();

	m_OptionsCache[nOptionID].bCached = true;
	m_OptionsCache[nOptionID].value = value;

	SaveOption(nOptionID, fz::to_wstring(value));
}

void COptions::SetOption(int nOptionID, std::wstring const& value)
{
	Init();

	m_OptionsCache[nOptionID].bCached = true;
	m_OptionsCache[nOptionID].str = value;

	SaveOption(nOptionID, value);
}

void COptions::SaveOption(int nOptionID, std::wstring const& value)
{
	std::wstring const file = GetFileName(true);

	XML::file xml = XML::Load(file);
	if (!xml) {
		return;
	}

	auto settings = xml.root.child("Settings");
	if (!settings) {
		settings = xml.root.append_child("Settings");
	}

	pugi::xml_node item;
	for (item = settings.child("Item"); item; item = item.next_sibling("Item")) {
		if (item.attribute("name").value() == m_Options[nOptionID].name) {
			break;
		}
	}

	if (!item) {
		item = settings.append_child("Item");
		item.append_attribute("name").set_value(m_Options[nOptionID].name.c_str());
	}

	item.text().set(fz::to_utf8(value).c_str());

	xml.document.save_file(file.c_str());
}

std::wstring COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID >= 0 && nOptionID < IOPTIONS_NUM);
	ASSERT(!m_Options[nOptionID].nType);
	Init();

	if (m_OptionsCache[nOptionID].bCached) {
		return m_OptionsCache[nOptionID].str;
	}

	//Verification
	switch (nOptionID)
	{
	case IOPTION_LASTSERVERADDRESS:
		m_OptionsCache[nOptionID].str = _T("localhost");
		break;
	default:
		m_OptionsCache[nOptionID].str.clear();
		break;
	}
	m_OptionsCache[nOptionID].bCached = true;
	return m_OptionsCache[nOptionID].str;
}

__int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID >= 0 && nOptionID < IOPTIONS_NUM);
	ASSERT(m_Options[nOptionID].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID].bCached) {
		return m_OptionsCache[nOptionID].value;
	}

	switch (nOptionID) {
	case IOPTION_LASTSERVERPORT:
		m_OptionsCache[nOptionID].value = 14147;
		break;
	case IOPTION_RECONNECTCOUNT:
		m_OptionsCache[nOptionID].value = 15;
		break;
	default:
		m_OptionsCache[nOptionID].value = 0;
	}
	m_OptionsCache[nOptionID].bCached = TRUE;
	return m_OptionsCache[nOptionID].value;
}

std::wstring COptions::GetFileName(bool for_saving)
{
	std::wstring ret;

	PWSTR str = 0;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, 0, &str) == S_OK && str && *str) {
		std::wstring path = str;
		path += L"\\FileZilla Server\\";

		if (for_saving) {
			CreateDirectory(path.c_str(), 0);
		}
		path += L"FileZilla Server Interface.xml";
		
		if (!for_saving) {
			CFileStatus status;
			if (CFile::GetStatus(path.c_str(), status)) {
				ret = path;
			}
		}
		else {
			ret = path;
		}

		CoTaskMemFree(str);
	}

	if (ret.empty() && !for_saving) {
		TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
		GetModuleFileName(0, buffer, MAX_PATH);
		LPTSTR pos = _tcsrchr(buffer, '\\');
		if (pos) {
			*++pos = 0;
		}
		ret = buffer;
		ret += L"FileZilla Server Interface.xml";
	}

	return ret;
}

void COptions::Init()
{
	if (m_bInitialized) {
		return;
	}
	m_bInitialized = true;
		
	for (int i = 0; i < IOPTIONS_NUM; ++i) {
		m_OptionsCache[i].bCached = false;
	}

	std::wstring file = GetFileName(false).c_str();
	XML::file xml = XML::Load(file);
	if (!xml) {
		return;
	}

	auto settings = xml.root.child("Settings");
	if (!settings) {
		return;
	}

	for (auto item = settings.child("Item"); item; item = item.next_sibling("Item")) {
		std::string name = item.attribute("name").value();
		if (name.empty()) {
			continue;
		}

		for (int i = 0; i < IOPTIONS_NUM; ++i) {
			if (name != m_Options[i].name) {
				continue;
			}

			if (m_OptionsCache[i].bCached) {
				break;
			}
			m_OptionsCache[i].bCached = true;

			if (m_Options[i].nType == 1) {
				m_OptionsCache[i].value = item.text().as_llong();

				switch (i)
				{
				case IOPTION_LASTSERVERPORT:
					if (m_OptionsCache[i].value < 1 || m_OptionsCache[i].value > 65535) {
						m_OptionsCache[i].value = 14147;
					}
					break;
				default:
					break;
				}
			}
			else {
				m_OptionsCache[i].str = fz::to_wstring_from_utf8(item.child_value());
			}
		}
	}
}
