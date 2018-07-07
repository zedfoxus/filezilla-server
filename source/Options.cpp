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
#include "platform.h"
#include "version.h"
#include "pugixml/pugixml.hpp"
#include "iputils.h"
#include "OptionLimits.h"
#include "xml_utils.h"

#include <libfilezilla/string.hpp>

std::vector<COptions *> COptions::m_InstanceList;
std::recursive_mutex COptions::m_mutex;
COptions::t_OptionsCache COptions::m_sOptionsCache[OPTIONS_NUM];
bool COptions::m_bInitialized{};

SPEEDLIMITSLIST COptions::m_sSpeedLimits[2];

// Backslash-terminated
std::wstring GetExecutableDirectory()
{
	std::wstring ret;

	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	if (GetModuleFileName(0, buffer, MAX_PATH) > 0) {
		LPTSTR pos = _tcsrchr(buffer, '\\');
		if (pos) {
			*++pos = 0;
			ret = buffer;
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////
// COptionsHelperWindow

class COptionsHelperWindow final
{
public:
	COptionsHelperWindow() = delete;

	COptionsHelperWindow(COptions *pOptions)
	{
		ASSERT(pOptions);
		m_pOptions = pOptions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize = sizeof(wndclass);
		wndclass.style = 0;
		wndclass.lpfnWndProc = WindowProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = GetModuleHandle(0);
		wndclass.hIcon = 0;
		wndclass.hCursor = 0;
		wndclass.hbrBackground = 0;
		wndclass.lpszMenuName = 0;
		wndclass.lpszClassName = _T("COptions Helper Window");
		wndclass.hIconSm = 0;

		RegisterClassEx(&wndclass);

		m_hWnd = CreateWindow(_T("COptions Helper Window"), _T("COptions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	~COptionsHelperWindow()
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
	static LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if (message == WM_USER) {
			COptionsHelperWindow *pWnd = (COptionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd) {
				return 0;
			}
			ASSERT(pWnd);
			ASSERT(pWnd->m_pOptions);
			for (int i = 0; i < OPTIONS_NUM; ++i) {
				pWnd->m_pOptions->m_OptionsCache[i].bCached = false;
			}
			simple_lock lock(COptions::m_mutex);
			pWnd->m_pOptions->m_SpeedLimits[0] = COptions::m_sSpeedLimits[0];
			pWnd->m_pOptions->m_SpeedLimits[1] = COptions::m_sSpeedLimits[1];
		}
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	COptions *m_pOptions;

private:
	HWND m_hWnd;
};

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptions

COptions::COptions()
{
	for (int i = 0; i < OPTIONS_NUM; ++i) {
		m_OptionsCache[i].bCached = false;
	}
	m_pOptionsHelperWindow = new COptionsHelperWindow(this);
	simple_lock lock(m_mutex);
	m_InstanceList.push_back(this);
	m_SpeedLimits[0] = m_sSpeedLimits[0];
	m_SpeedLimits[1] = m_sSpeedLimits[1];
}

COptions::~COptions()
{
	{
		simple_lock lock(m_mutex);
		auto iter = std::find(m_InstanceList.begin(), m_InstanceList.end(), this);
		ASSERT(iter != m_InstanceList.end());
		if (iter != m_InstanceList.end()) {
			m_InstanceList.erase(iter);
		}
	}

	if (m_pOptionsHelperWindow) {
		delete m_pOptionsHelperWindow;
		m_pOptionsHelperWindow = nullptr;
	}
}

void COptions::SetOption(int nOptionID, int64_t value, bool save)
{
	switch (nOptionID)
	{
	case OPTION_MAXUSERS:
		if (value < 0) {
			value = 0;
		}
		else if (value < 15) {
			value = 15;
		}
		break;
	case OPTION_THREADNUM:
		if (value < 1) {
			value = 4;
		}
		else if (value > 50) {
			value = 4;
		}
		break;
	case OPTION_TIMEOUT:
		if (value < 0) {
			value = 120;
		}
		else if (value > 9999) {
			value = 120;
		}
		break;
	case OPTION_NOTRANSFERTIMEOUT:
		if (value < 600 && value != 0) {
			value = 600;
		}
		else if (value > 9999) {
			value = 600;
		}
		break;
	case OPTION_LOGINTIMEOUT:
		if (value < 0) {
			value = 60;
		}
		else if (value > 9999) {
			value = 60;
		}
		break;
	case OPTION_ADMINPORT:
		if (value > 65535) {
			value = 14147;
		}
		else if (value < 1) {
			value = 14147;
		}
		break;
	case OPTION_LOGTYPE:
		if (value != 0 && value != 1) {
			value = 0;
		}
		break;
	case OPTION_LOGLIMITSIZE:
		if ((value > 999999 || value < 10) && value != 0) {
			value = 100;
		}
		break;
	case OPTION_LOGDELETETIME:
		if (value > 999 || value < 0) {
			value = 14;
		}
		break;
	case OPTION_DOWNLOADSPEEDLIMITTYPE:
	case OPTION_UPLOADSPEEDLIMITTYPE:
		if (value < 0 || value > 2) {
			value = 0;
		}
		break;
	case OPTION_DOWNLOADSPEEDLIMIT:
	case OPTION_UPLOADSPEEDLIMIT:
		if (value < 1) {
			value = 1;
		}
		else if (value > 1048576) {
			value = 1048576;
		}
		break;
	case OPTION_BUFFERSIZE:
		if (value < 256 || value >(1024 * 1024)) {
			value = 32768;
		}
		break;
	case OPTION_BUFFERSIZE2:
		if (value < 256 || value >(1024 * 1024 * 128)) {
			value = 262144;
		}
		break;
	case OPTION_CUSTOMPASVIPTYPE:
		if (value < 0 || value > 2) {
			value = 0;
		}
		break;
	case OPTION_MODEZ_USE:
		if (value < 0 || value > 1) {
			value = 0;
		}
		break;
	case OPTION_MODEZ_LEVELMIN:
		if (value < 0 || value > 8) {
			value = 1;
		}
		break;
	case OPTION_MODEZ_LEVELMAX:
		if (value < 8 || value > 9) {
			value = 9;
		}
		break;
	case OPTION_MODEZ_ALLOWLOCAL:
		if (value < 0 || value > 1) {
			value = 0;
		}
		break;
	case OPTION_AUTOBAN_ATTEMPTS:
		if (value < OPTION_AUTOBAN_ATTEMPTS_MIN) {
			value = OPTION_AUTOBAN_ATTEMPTS_MIN;
		}
		if (value > OPTION_AUTOBAN_ATTEMPTS_MAX) {
			value = OPTION_AUTOBAN_ATTEMPTS_MAX;
		}
		break;
	case OPTION_AUTOBAN_BANTIME:
		if (value < 1) {
			value = 1;
		}
		if (value > 999) {
			value = 999;
		}
		break;
	}

	Init();

	std::string valuestr = fz::to_string(value);

	{
		simple_lock lock(m_mutex);
		m_sOptionsCache[nOptionID - 1].value = value;
		m_sOptionsCache[nOptionID - 1].str = fz::to_wstring(valuestr);
		m_sOptionsCache[nOptionID - 1].bCached = true;
		m_OptionsCache[nOptionID - 1] = m_sOptionsCache[nOptionID - 1];
	}

	if (!save) {
		return;
	}

	std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	XML::file file = XML::Load(xmlFileName);
	if (!file) {
		return;
	}

	auto settings = file.root.child("Settings");
	if (!settings) {
		settings = file.root.append_child("Settings");
	}

	pugi::xml_node setting;
	pugi::xml_node_iterator it = settings.find_child_by_attribute("Item", "name", m_Options[nOptionID - 1].name);
	if (it == settings.end()) {
		setting = settings.append_child("Item");
		setting.append_attribute("name").set_value(m_Options[nOptionID - 1].name);
	}
	else {
		setting = *it;
	}

	setting.text().set(valuestr.c_str());

	XML::Save(file.document, xmlFileName);
}

void COptions::SetOption(int nOptionID, std::wstring str, bool save)
{
	Init();

	switch (nOptionID)
	{
	case OPTION_SERVERPORT:
	case OPTION_TLSPORTS:
		{
			std::set<int> portSet;

			size_t oldpos = 0;
			size_t pos = str.find_first_of(L" ,");
			while (pos != std::wstring::npos) {
				if (pos != oldpos) {
					int port = fz::to_integral<int>(str.substr(oldpos, pos));
					if (port >= 1 && port <= 65535) {
						portSet.insert(port);
					}
				}
				oldpos = pos + 1;
				pos = str.find_first_of(L" ,", oldpos);
			}
			if (oldpos < str.size()) {
				int port = fz::to_integral<int>(str.substr(oldpos));
				if (port >= 1 && port <= 65535) {
					portSet.insert(port);
				}
			}

			str.clear();
			for (int port : portSet) {
				str += fz::to_wstring(port) + L" ";
			}
			if (!str.empty()) {
				str.pop_back();
			}
		}
		break;
	case OPTION_WELCOMEMESSAGE:
		{
			std::vector<std::wstring> msgLines;
			size_t oldpos = 0;
			fz::replace_substrings(str, L"\r\n", L"\n");
			size_t pos = str.find('\n');
			while (pos != std::wstring::npos) {
				if (pos != oldpos) {
					std::wstring line = str.substr(oldpos, std::min(pos - oldpos, size_t(CONST_WELCOMEMESSAGE_LINESIZE)));
					fz::rtrim(line);
					if (msgLines.size() || !line.empty()) {
						msgLines.push_back(line);
					}
				}
				oldpos = pos + 1;
				pos = str.find('\n', oldpos);
			}
			if (oldpos < str.size()) {
				msgLines.push_back(str.substr(0, CONST_WELCOMEMESSAGE_LINESIZE));
			}

			str.clear();
			for (unsigned int i = 0; i < msgLines.size(); ++i) {
				str += msgLines[i] + L"\r\n";
			}
			if (str.size() > 2) {
				str.resize(str.size() - 2);
			}
			if (str.empty()) {
				str = L"%v";
				str += L"\r\nwritten by Tim Kosse (tim.kosse@filezilla-project.org)";
				str += L"\r\nPlease visit https://filezilla-project.org/";
			}
			fz::replace_substrings(str, L"tim.kosse@gmx.de", L"tim.kosse@filezilla-project.org");
			fz::replace_substrings(str, L"http://sourceforge.net/projects/filezilla/", L"https://filezilla-project.org/");
		}
		break;
	case OPTION_ADMINIPBINDINGS:
		{
			std::wstring ips;

			std::wstring sub;
			for (size_t i = 0; i < str.size(); ++i) {
				wchar_t cur = str[i];
				if ((cur < '0' || cur > '9') && cur != '.' && cur != ':') {
					if (sub.empty() && cur == '*') {
						ips = L"*";
						break;
					}

					if (!sub.empty()) {
						if (IsIpAddress(sub) && !IsLocalhost(sub)) {
							ips += sub + L" ";
						}
						sub.clear();
					}
				}
				else {
					sub += cur;
				}
			}
			if (!sub.empty() && IsIpAddress(sub) && !IsLocalhost(sub)) {
				ips += sub + L" ";
			}
			
			fz::rtrim(ips);
			str = ips;
		}
		break;
	case OPTION_ADMINPASS:
		if (!str.empty() && str.size() < 6) {
			return;
		}
		break;
	case OPTION_MODEZ_DISALLOWED_IPS:
	case OPTION_IPFILTER_ALLOWED:
	case OPTION_IPFILTER_DISALLOWED:
	case OPTION_ADMINIPADDRESSES:
		{
			std::wstring ips;

			size_t oldpos = 0;
			size_t pos = str.find_first_of(L" \r\n\t");
			while (pos != std::wstring::npos) {
				if (pos != oldpos) {
					std::wstring sub = str.substr(oldpos, pos - oldpos);
					if (sub == L"*") {
						ips = sub;
						break;
					}
					else {
						if (IsValidAddressFilter(sub)) {
							ips += sub + L" ";
						}
					}
				}
				oldpos = pos + 1;
				pos = str.find_first_of(L" \r\n\t", oldpos);
			}
			if (oldpos < str.size() && ips != L"*") {
				std::wstring sub = str.substr(oldpos);
				if (sub == L"*") {
					ips = sub;
				}
				else {
					if (IsValidAddressFilter(sub)) {
						ips += sub + L" ";
					}
				}
			}
			fz::rtrim(ips);

			str = ips;
		}
		break;
	case OPTION_IPBINDINGS:
		{
			std::wstring ips;

			std::wstring sub;
			for (size_t i = 0; i < str.size(); ++i) {
				wchar_t cur = str[i];
				if ((cur < '0' || cur > '9') && cur != '.' && cur != ':') {
					if (sub.empty() && cur == '*') {
						ips = L"*";
						break;
					}

					if (!sub.empty()) {
						if (IsIpAddress(sub, true)) {
							ips += sub + L" ";
						}
						sub.clear();
					}
				}
				else {
					sub += cur;
				}
			}
			if (!sub.empty() && IsIpAddress(sub, true)) {
				ips += sub + L" ";
			}

			if (ips.empty()) {
				ips = L"*";
			}

			fz::rtrim(ips);
			str = ips;
		}
		break;
	case OPTION_CUSTOMPASVIPSERVER:
		if (str.find(_T("filezilla.sourceforge.net")) != std::wstring::npos) {
			str = L"http://ip.filezilla-project.org/ip.php";
		}
		break;
	}

	{
		simple_lock lock(m_mutex);
		m_sOptionsCache[nOptionID - 1].bCached = true;
		m_sOptionsCache[nOptionID - 1].str = str;
		m_sOptionsCache[nOptionID - 1].value = fz::to_integral<int64_t>(str);
		m_OptionsCache[nOptionID - 1] = m_sOptionsCache[nOptionID - 1];
	}

	if (!save) {
		return;
	}

	std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	XML::file file = XML::Load(xmlFileName);
	if (!file) {
		return;
	}

	auto settings = file.root.child("Settings");
	if (!settings) {
		settings = file.root.append_child("Settings");
	}

	pugi::xml_node setting;
	pugi::xml_node_iterator it = settings.find_child_by_attribute("Item", "name", m_Options[nOptionID - 1].name);
	if (it == settings.end()) {
		setting = settings.append_child("Item");
		setting.append_attribute("name").set_value(m_Options[nOptionID - 1].name);
	}
	else {
		setting = *it;
	}

	setting.text().set(fz::to_utf8(str).c_str());

	XML::Save(file.document, xmlFileName);
}

std::wstring COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID > 0 && nOptionID <= OPTIONS_NUM);
	ASSERT(!m_Options[nOptionID - 1].nType);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached) {
		return m_OptionsCache[nOptionID - 1].str;
	}

	simple_lock lock(m_mutex);

	if (!m_sOptionsCache[nOptionID - 1].bCached) {
		//Default values
		switch (nOptionID)
		{
		case OPTION_SERVERPORT:
			m_sOptionsCache[nOptionID - 1].str = _T("21");
			break;
		case OPTION_WELCOMEMESSAGE:
			m_sOptionsCache[nOptionID - 1].str = _T("%v");
			m_sOptionsCache[nOptionID - 1].str += _T("\r\nwritten by Tim Kosse (tim.kosse@filezilla-project.org)");
			m_sOptionsCache[nOptionID - 1].str += _T("\r\nPlease visit https://filezilla-project.org/");
			break;
		case OPTION_CUSTOMPASVIPSERVER:
			m_sOptionsCache[nOptionID - 1].str = _T("http://ip.filezilla-project.org/ip.php");
			break;
		case OPTION_IPBINDINGS:
			m_sOptionsCache[nOptionID - 1].str = _T("*");
			break;
		case OPTION_TLSPORTS:
			m_sOptionsCache[nOptionID - 1].str = _T("990");
			break;
		default:
			m_sOptionsCache[nOptionID - 1].str = _T("");
			break;
		}
		m_sOptionsCache[nOptionID - 1].bCached = true;
	}
	m_OptionsCache[nOptionID - 1] = m_sOptionsCache[nOptionID - 1];
	return m_OptionsCache[nOptionID - 1].str;
}

_int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID > 0 && nOptionID <= OPTIONS_NUM);
	ASSERT(m_Options[nOptionID - 1].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID - 1].bCached) {
		return m_OptionsCache[nOptionID - 1].value;
	}

	simple_lock lock(m_mutex);

	if (!m_sOptionsCache[nOptionID - 1].bCached) {
		//Default values
		switch (nOptionID)
		{
			case OPTION_CHECK_DATA_CONNECTION_IP:
				m_sOptionsCache[nOptionID - 1].value = 2;
				break;
			case OPTION_MAXUSERS:
				m_sOptionsCache[nOptionID - 1].value = 0;
				break;
			case OPTION_THREADNUM:
				m_sOptionsCache[nOptionID - 1].value = 4;
				break;
			case OPTION_TIMEOUT:
			case OPTION_NOTRANSFERTIMEOUT:
				m_sOptionsCache[nOptionID - 1].value = 120;
				break;
			case OPTION_LOGINTIMEOUT:
				m_sOptionsCache[nOptionID - 1].value = 60;
				break;
			case OPTION_ADMINPORT:
				m_sOptionsCache[nOptionID - 1].value = 14147;
				break;
			case OPTION_DOWNLOADSPEEDLIMIT:
			case OPTION_UPLOADSPEEDLIMIT:
				m_sOptionsCache[nOptionID - 1].value = 10;
				break;
			case OPTION_BUFFERSIZE:
				m_sOptionsCache[nOptionID - 1].value = 32768;
				break;
			case OPTION_CUSTOMPASVIPTYPE:
				m_sOptionsCache[nOptionID - 1].value = 0;
				break;
			case OPTION_MODEZ_USE:
				m_sOptionsCache[nOptionID - 1].value = 0;
				break;
			case OPTION_MODEZ_LEVELMIN:
				m_sOptionsCache[nOptionID - 1].value = 1;
				break;
			case OPTION_MODEZ_LEVELMAX:
				m_sOptionsCache[nOptionID - 1].value = 9;
				break;
			case OPTION_MODEZ_ALLOWLOCAL:
				m_sOptionsCache[nOptionID - 1].value = 0;
				break;
			case OPTION_ALLOWEXPLICITTLS:
			case OPTION_FORCEPROTP:
			case OPTION_TLS_REQUIRE_SESSION_RESUMPTION:
				m_sOptionsCache[nOptionID - 1].value = 1;
				break;
			case OPTION_BUFFERSIZE2:
				m_sOptionsCache[nOptionID - 1].value = 65536*4;
				break;
			case OPTION_NOEXTERNALIPONLOCAL:
				m_sOptionsCache[nOptionID - 1].value = 1;
				break;
			case OPTION_ACTIVE_IGNORELOCAL:
				m_sOptionsCache[nOptionID - 1].value = 1;
				break;
			case OPTION_AUTOBAN_BANTIME:
				m_sOptionsCache[nOptionID - 1].value = 1;
				break;
			case OPTION_AUTOBAN_ATTEMPTS:
				m_sOptionsCache[nOptionID - 1].value = 10;
				break;
			default:
				m_sOptionsCache[nOptionID - 1].value = 0;
		}
		m_sOptionsCache[nOptionID - 1].bCached = true;
	}
	m_OptionsCache[nOptionID - 1] = m_sOptionsCache[nOptionID - 1];
	return m_OptionsCache[nOptionID - 1].value;
}

void COptions::UpdateInstances()
{
	simple_lock lock(m_mutex);
	for (auto const& pOptions : m_InstanceList) {
		ASSERT(pOptions->m_pOptionsHelperWindow);
		::PostMessage(pOptions->m_pOptionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
	}
}

void COptions::Init(bool reload)
{
	if (m_bInitialized && !reload) {
		return;
	}
	simple_lock lock(m_mutex);
	m_bInitialized = true;

	for (int i = 0; i < OPTIONS_NUM; ++i) {
		m_sOptionsCache[i].bCached = false;
	}

	std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	XML::file file = XML::Load(xmlFileName);
	if (!file) {
		return;
	}
	
	auto settings = file.root.child("Settings");
	if (settings) {
		settings = file.root.append_child("Settings");
	}

	for (auto setting = settings.child("Item"); setting; setting = setting.next_sibling("Item")) {
		std::string name = setting.attribute("name").value();
		if (name.empty()) {
			continue;
		}

		for (int i = 0; i < OPTIONS_NUM; ++i) {
			if (name != m_Options[i].name) {
				continue;
			}
			if (m_sOptionsCache[i].bCached) {
				break;
			}

			std::wstring value = fz::to_wstring_from_utf8(setting.child_value());
			if (m_Options[i].nType) {
				SetOption(i, fz::to_integral<int64_t>(value), false);
			}
			else {
				SetOption(i, value, false);
			}
			break;
		}
	}
	ReadSpeedLimits(settings);

	UpdateInstances();
}

XML::file* COptions::GetXML()
{
	simple_lock lock(m_mutex);

	std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	XML::file *pFile = new XML::file;
	*pFile = XML::Load(xmlFileName);
	if (!*pFile) {
		delete pFile;
		return nullptr;
	}
	
	// Must call FreeXML
	m_mutex.lock();
	
	return pFile;
}

bool COptions::FreeXML(XML::file *pXML, bool save)
{
	ASSERT(pXML);
	if (!pXML) {
		return false;
	}

	simple_lock lock(m_mutex);

	// As locked by GetXML
	m_mutex.unlock();

	bool ret = true;
	if (save) {
		std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");
		ret = XML::Save(pXML->document, xmlFileName);
	}
	delete pXML;

	return ret;
}

bool COptions::GetAsCommand(unsigned char **pBuffer, DWORD *nBufferLength)
{
	int i;
	DWORD len = 2;

	simple_lock lock(m_mutex);
	for (i = 0; i < OPTIONS_NUM; ++i) {
		len += 1;
		if (!m_Options[i].nType) {
			len += 3;
			auto utf8 = fz::to_utf8(GetOption(i + 1));

			if ((i + 1) != OPTION_ADMINPASS) {
				len += utf8.size();
			}
			else {
				if (GetOption(i + 1).size() < 6 && utf8.size() > 0) {
					len++;
				}
			}
		}
		else {
			len += 8;
		}
	}

	len += 4;
	for (i = 0; i < 2; ++i) {
		if (m_sSpeedLimits[i].size() > 0xffff) {
			return false;
		}
		for (auto const& limit : m_sSpeedLimits[i]) {
			len += limit.GetRequiredBufferLen();
		}
	}

	*pBuffer = new unsigned char[len];
	unsigned char *p = *pBuffer;
	*p++ = OPTIONS_NUM / 256;
	*p++ = OPTIONS_NUM % 256;
	for (i = 0; i < OPTIONS_NUM; ++i) {
		*p++ = m_Options[i].nType;
		switch (m_Options[i].nType) {
		case 0:
			{
				std::wstring str = GetOption(i+1);
				if ((i+1) == OPTION_ADMINPASS) //Do NOT send admin password,
											 //instead send empty string if admin pass is set
											 //and send a single char if admin pass is invalid (len < 6)
				{
					if (str.size() >= 6 || str.empty()) {
						str.clear();
					}
					else {
						str = _T("*");
					}
				}

				auto utf8 = fz::to_utf8(str);

				int len = utf8.size();
				*p++ = (len / 256) / 256;
				*p++ = (len / 256) % 256;
				*p++ = len % 256;
				memcpy(p, utf8.c_str(), len);
				p += len;
			}
			break;
		case 1:
			{
				_int64 value = GetOptionVal(i+1);
				memcpy(p, &value, 8);
				p+=8;
			}
			break;
		default:
			ASSERT(false);
		}
	}

	for (i = 0; i < 2; ++i) {
		*p++ = (m_sSpeedLimits[i].size() >> 8) & 0xffu;
		*p++ = m_sSpeedLimits[i].size() % 256;
		for (auto const& limit : m_sSpeedLimits[i]) {
			p = limit.FillBuffer(p);
		}
	}

	*nBufferLength = len;

	return true;
}

bool COptions::ParseOptionsCommand(unsigned char *pData, DWORD dwDataLength, bool bFromLocal)
{
	unsigned char *p = pData;
	int num = *p * 256 + p[1];
	p+=2;
	if (num != OPTIONS_NUM) {
		return false;
	}

	int i;
	for (i = 0; i < num; ++i) {
		if ((DWORD)(p - pData) >= dwDataLength) {
			return false;
		}
		int nType = *p++;
		if (!nType) {
			if ((DWORD)(p - pData + 3) >= dwDataLength) {
				return false;
			}
			int len = *p * 256 * 256 + p[1] * 256 + p[2];
			p += 3;
			if ((DWORD)(p - pData + len) > dwDataLength) {
				return false;
			}
			char *pBuffer = new char[len + 1];
			memcpy(pBuffer, p, len);
			pBuffer[len] = 0;
			if (!m_Options[i].bOnlyLocal || bFromLocal) { //Do not change admin interface settings from remote connections
				SetOption(i + 1, ConvFromNetwork(pBuffer), false);
			}
			delete [] pBuffer;
			p += len;
		}
		else if (nType == 1) {
			if ((DWORD)(p - pData + 8) > dwDataLength) {
				return false;
			}
			if (!m_Options[i].bOnlyLocal || bFromLocal) { //Do not change admin interface settings from remote connections				
				SetOption(i + 1, GET64(p), false);
			}
			p += 8;
		}
		else {
			return false;
		}
	}

	SPEEDLIMITSLIST dl;
	SPEEDLIMITSLIST ul;

	if ((DWORD)(p - pData + 2) > dwDataLength) {
		return false;
	}
	num = *p++ << 8;
	num |= *p++;

	simple_lock lock(m_mutex);

	for (i=0; i<num; ++i) {
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p) {
			return false;
		}
		dl.push_back(limit);
	}

	if ((DWORD)(p-pData+2)>dwDataLength) {
		return false;
	}
	num = *p++ << 8;
	num |= *p++;
	for (i=0; i<num; ++i) {
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p) {
			return false;
		}
		ul.push_back(limit);
	}

	m_sSpeedLimits[0] = dl;
	m_sSpeedLimits[1] = ul;

	SaveOptions();

	UpdateInstances();

	return true;
}

bool COptions::SaveSpeedLimits(pugi::xml_node & settings)
{
	do {} while (settings.remove_child("SpeedLimits"));

	auto speedLimits = settings.append_child("SpeedLimits");

	const char* names[] = { "Download", "Upload" };

	for (int i = 0; i < 2; ++i) {
		auto direction = speedLimits.append_child(names[i]);

		for (unsigned int j = 0; j < m_sSpeedLimits[i].size(); ++j) {
			CSpeedLimit limit = m_sSpeedLimits[i][j];

			auto rule = direction.append_child("Rule");
			limit.Save(rule);
		}
	}

	return true;
}

bool COptions::ReadSpeedLimits(pugi::xml_node const& settings)
{
	const char* names[] = { "Download", "Upload" };

	auto speedLimits = settings.child("SpeedLimits");

	for (int i = 0; i < 2; ++i) {
		auto direction = speedLimits.child(names[i]);

		for (auto rule = direction.child("Rule"); rule; rule = rule.next_sibling("Rule")) {
			CSpeedLimit limit;
			if (!limit.Load(rule)) {
				continue;
			}

			if (m_sSpeedLimits[i].size() < 20000) {
				m_sSpeedLimits[i].push_back(limit);
			}
		}
	}

	return true;
}

int COptions::GetCurrentSpeedLimit(int nMode)
{
	Init();

	int type[2] = { OPTION_DOWNLOADSPEEDLIMITTYPE, OPTION_UPLOADSPEEDLIMITTYPE };
	int limit[2] = { OPTION_DOWNLOADSPEEDLIMIT, OPTION_UPLOADSPEEDLIMIT };

	int nType = (int)GetOptionVal(type[nMode]);
	switch (nType)
	{
	case 0:
		return -1;
	case 1:
		return (int)GetOptionVal(limit[nMode]);
	default:
		{
			SYSTEMTIME s;
			GetLocalTime(&s);
			for (SPEEDLIMITSLIST::const_iterator iter = m_SpeedLimits[nMode].begin(); iter != m_SpeedLimits[nMode].end(); ++iter) {
				if (iter->IsItActive(s)) {
					return iter->m_Speed;
				}
			}
			return -1;
		}
	}
}

void COptions::ReloadConfig()
{
	Init(true);
}

void COptions::SaveOptions()
{
	std::wstring const xmlFileName = GetExecutableDirectory() + _T("FileZilla Server.xml");

	XML::file file = XML::Load(xmlFileName);
	if (!file) {
		return;
	}

	do {} while (file.root.remove_child("SpeedLimits"));
	auto settings = file.root.append_child("Settings");

	for (unsigned int i = 0; i < OPTIONS_NUM; ++i) {
		if (!m_OptionsCache[i].bCached) {
			continue;
		}

		std::string valuestr;
		if (!m_Options[i].nType) {
			valuestr = fz::to_utf8(m_OptionsCache[i].str);
		}
		else {
			valuestr = fz::to_string(m_OptionsCache[i].value);
		}

		auto item = settings.append_child("Item");
		item.append_attribute("name").set_value(m_Options[i].name);
	}

	SaveSpeedLimits(settings);

	XML::Save(file.document, xmlFileName);
}
