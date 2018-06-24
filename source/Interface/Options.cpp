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

// Options.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "options.h"
#include "../version.h"
#include "filezilla server.h"
#include "../tinyxml/tinyxml.h"
#include "../xml_utils.h"

#include <libfilezilla/string.hpp>

BOOL COptions::m_bInitialized = FALSE;

namespace {
TiXmlElement* GetSettings(TiXmlDocument& document)
{
	TiXmlElement* pRoot = document.FirstChildElement("FileZillaServer");
	if (!pRoot) {
		if (document.FirstChildElement()) {
			return 0;
		}
		pRoot = document.LinkEndChild(new TiXmlElement("FileZillaServer"))->ToElement();
	}

	TiXmlElement* pSettings = pRoot->FirstChildElement("Settings");
	if (!pSettings)
		pSettings = pRoot->LinkEndChild(new TiXmlElement("Settings"))->ToElement();

	return pSettings;
}
}

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptions

struct t_Option final
{
	TCHAR name[30];
	int nType;
};

static const t_Option m_Options[IOPTIONS_NUM] = {
	{ _T("Start Minimized"),        1 },
	{ _T("Last Server Address"),    0 },
	{ _T("Last Server Port"),       1 },
	{ _T("Last Server Password"),   0 },
	{ _T("Always use last server"), 1 },
	{ _T("User Sorting"),           1 },
	{ _T("Filename Display"),       1 },
	{ _T("Max reconnect count"),    1 },
};

void COptions::SetOption(int nOptionID, __int64 value)
{
	Init();

	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 1;
	m_OptionsCache[nOptionID - 1].value = value;

	SaveOption(nOptionID, fz::to_wstring(value));
}

void COptions::SetOption(int nOptionID, std::wstring const& value)
{
	Init();

	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	m_OptionsCache[nOptionID - 1].str = value;

	SaveOption(nOptionID, value);
}

void COptions::SaveOption(int nOptionID, std::wstring const& value)
{
	std::wstring const file = GetFileName(true);

	TiXmlDocument document;
	XML::Load(document, file.c_str());

	TiXmlElement* pSettings = GetSettings(document);
	if (!pSettings) {
		return;
	}

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName) {
			continue;
		}
		CString name(pName);
		if (name != m_Options[nOptionID - 1].name) {
			continue;
		}

		break;
	}

	if (!pItem) {
		pItem = pSettings->LinkEndChild(new TiXmlElement("Item"))->ToElement();
	}
	pItem->Clear();
	pItem->SetAttribute("name", fz::to_utf8(m_Options[nOptionID - 1].name).c_str());
	pItem->SetAttribute("type", m_OptionsCache[nOptionID - 1].nType ? "numeric" : "string");
	pItem->LinkEndChild(new TiXmlText(fz::to_utf8(value).c_str()));

	XML::Save(document, file.c_str());
}

std::wstring COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID > 0 && nOptionID <= IOPTIONS_NUM);
	ASSERT(!m_Options[nOptionID - 1].nType);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached) {
		return m_OptionsCache[nOptionID - 1].str;
	}

	//Verification
	switch (nOptionID)
	{
	case IOPTION_LASTSERVERADDRESS:
		m_OptionsCache[nOptionID - 1].str = _T("localhost");
		break;
	default:
		m_OptionsCache[nOptionID - 1].str.clear();
		break;
	}
	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	return m_OptionsCache[nOptionID - 1].str;
}

__int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=IOPTIONS_NUM);
	ASSERT(m_Options[nOptionID - 1].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached) {
		return m_OptionsCache[nOptionID - 1].value;
	}

	switch (nOptionID) {
	case IOPTION_LASTSERVERPORT:
		m_OptionsCache[nOptionID - 1].value = 14147;
		break;
	case IOPTION_RECONNECTCOUNT:
		m_OptionsCache[nOptionID - 1].value = 15;
		break;
	default:
		m_OptionsCache[nOptionID - 1].value = 0;
	}
	m_OptionsCache[nOptionID - 1].bCached = TRUE;
	m_OptionsCache[nOptionID - 1].nType = 0;
	return m_OptionsCache[nOptionID - 1].value;
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
	m_bInitialized = TRUE;
		
	for (int i = 0; i < IOPTIONS_NUM; ++i) {
		m_OptionsCache[i].bCached = FALSE;
	}

	TiXmlDocument document;
	if (!XML::Load(document, GetFileName(false).c_str())) {
		return;
	}

	TiXmlElement* pSettings = GetSettings(document);
	if (!pSettings) {
		return;
	}

	TiXmlElement* pItem;
	for (pItem = pSettings->FirstChildElement("Item"); pItem; pItem = pItem->NextSiblingElement("Item")) {
		const char* pName = pItem->Attribute("name");
		if (!pName) {
			continue;
		}
		CString name(pName);
		const char* pType = pItem->Attribute("type");
		if (!pType) {
			continue;
		}
		CString type(pType);
		TiXmlNode* textNode = pItem->FirstChild();
		if (!textNode || !textNode->ToText()) {
			continue;
		}
		std::wstring value = ConvFromNetwork(textNode->Value());

		for (int i = 0; i < IOPTIONS_NUM; ++i) {
			if (name != m_Options[i].name) {
				continue;
			}

			if (m_OptionsCache[i].bCached) {
				break;
			}

			if (type == "numeric") {
				if (m_Options[i].nType != 1) {
					break;
				}
				m_OptionsCache[i].bCached = TRUE;
				m_OptionsCache[i].nType = 1;
				_int64 value64 = _ttoi64(value.c_str());

				switch (i + 1)
				{
				case IOPTION_LASTSERVERPORT:
					if (value64 < 1 || value64 > 65535) {
						value64 = 14147;
					}
					break;
				default:
					break;
				}

				m_OptionsCache[i].value = value64;
			}
			else {
				if (type != "string")
					::AfxTrace( _T("Unknown option type '%s' for item '%s', assuming string\n"), type, name);
				if (m_Options[i].nType != 0)
				{
					::AfxTrace( _T("Type mismatch for option '%s'. Type is %d, should be %d"), name, m_Options[i].nType, 0);
					break;
				}
				m_OptionsCache[i].bCached = TRUE;
				m_OptionsCache[i].nType = 0;

				m_OptionsCache[i].str = value;
			}
		}
	}
}

bool COptions::IsNumeric(LPCTSTR str)
{
	if (!str)
		return false;
	LPCTSTR p = str;
	while (*p) {
		if (*p < '0' || *p > '9') {
			return false;
		}
		p++;
	}
	return true;
}
