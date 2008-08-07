// FileZilla Server - a Windows ftp server

// Copyright (C) 2002-2004 - Tim Kosse <tim.kosse@gmx.de>

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
#include "platform.h"
#include "version.h"
#include "misc\MarkupSTL.h"
#include "iputils.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

std::list<COptions *> COptions::m_InstanceList;
CCriticalSectionWrapper COptions::m_Sync;
COptions::t_OptionsCache COptions::m_sOptionsCache[OPTIONS_NUM];
BOOL COptions::m_bInitialized=FALSE;

SPEEDLIMITSLIST COptions::m_sDownloadSpeedLimits;
SPEEDLIMITSLIST COptions::m_sUploadSpeedLimits;

/////////////////////////////////////////////////////////////////////////////
// COptionsHelperWindow

class COptionsHelperWindow
{
public:
	COptionsHelperWindow(COptions *pOptions)
	{
		ASSERT(pOptions);
		m_pOptions=pOptions;

		//Create window
		WNDCLASSEX wndclass;
		wndclass.cbSize=sizeof wndclass;
		wndclass.style=0;
		wndclass.lpfnWndProc=WindowProc;
		wndclass.cbClsExtra=0;
		wndclass.cbWndExtra=0;
		wndclass.hInstance=GetModuleHandle(0);
		wndclass.hIcon=0;
		wndclass.hCursor=0;
		wndclass.hbrBackground=0;
		wndclass.lpszMenuName=0;
		wndclass.lpszClassName=_T("COptions Helper Window");
		wndclass.hIconSm=0;

		RegisterClassEx(&wndclass);

		m_hWnd=CreateWindow(_T("COptions Helper Window"), _T("COptions Helper Window"), 0, 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0));
		ASSERT(m_hWnd);
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG)this);
	};

	virtual ~COptionsHelperWindow()
	{
		//Destroy window
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd=0;
		}
	}

	HWND GetHwnd()
	{
		return m_hWnd;
	}

protected:
	static LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if (message==WM_USER)
		{
			COptionsHelperWindow *pWnd=(COptionsHelperWindow *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (!pWnd)
				return 0;
			ASSERT(pWnd);
			ASSERT(pWnd->m_pOptions);
			for (int i=0;i<OPTIONS_NUM;i++)
				pWnd->m_pOptions->m_OptionsCache[i].bCached = FALSE;
			EnterCritSection(COptions::m_Sync);
			pWnd->m_pOptions->m_DownloadSpeedLimits = COptions::m_sDownloadSpeedLimits;
			pWnd->m_pOptions->m_UploadSpeedLimits = COptions::m_sUploadSpeedLimits;
			LeaveCritSection(COptions::m_Sync);
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
	for (int i=0;i<OPTIONS_NUM;i++)
		m_OptionsCache[i].bCached=FALSE;
	m_pOptionsHelperWindow=new COptionsHelperWindow(this);
	EnterCritSection(m_Sync);
#ifdef _DEBUG
	for (std::list<COptions *>::iterator iter=m_InstanceList.begin(); iter!=m_InstanceList.end(); iter++)
		ASSERT(*iter!=this);
#endif _DEBUG
	m_InstanceList.push_back(this);
	m_DownloadSpeedLimits = m_sDownloadSpeedLimits;
	m_UploadSpeedLimits = m_sUploadSpeedLimits;
	LeaveCritSection(m_Sync);
}

COptions::~COptions()
{
	EnterCritSection(m_Sync);
	std::list<COptions *>::iterator iter;
	for (iter=m_InstanceList.begin(); iter!=m_InstanceList.end(); iter++)
		if (*iter==this)
			break;

	ASSERT(iter!=m_InstanceList.end());
	if (iter != m_InstanceList.end())
		m_InstanceList.erase(iter);
	LeaveCritSection(m_Sync);

	if (m_pOptionsHelperWindow)
		delete m_pOptionsHelperWindow;
	m_pOptionsHelperWindow=0;
}

void COptions::SetOption(int nOptionID, _int64 value)
{
	switch (nOptionID)
	{
	case OPTION_MAXUSERS:
		if (value<0)
			value=0;
		break;
	case OPTION_THREADNUM:
		if (value<1)
			value=2;
		else if (value>50)
			value=2;
		break;
	case OPTION_TIMEOUT:
		if (value<0)
			value=120;
		else if (value>9999)
			value=120;
		break;
	case OPTION_NOTRANSFERTIMEOUT:
		if (value<600 && value != 0)
			value=600;
		else if (value>9999)
			value=600;
		break;
	case OPTION_LOGINTIMEOUT:
		if (value<0)
			value=60;
		else if (value>9999)
			value=60;
		break;
	case OPTION_ADMINPORT:
		if (value>65535)
			value=14147;
		else if (value<1)
			value=14147;
		break;
	case OPTION_LOGTYPE:
		if (value!=0 && value!=1)
			value = 0;
		break;
	case OPTION_LOGLIMITSIZE:
		if ((value > 999999 || value < 10) && value!=0)
			value = 100;
		break;
	case OPTION_LOGDELETETIME:
		if (value > 999 || value < 0)
			value = 14;
		break;
	case OPTION_DOWNLOADSPEEDLIMITTYPE:
	case OPTION_UPLOADSPEEDLIMITTYPE:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_DOWNLOADSPEEDLIMIT:
	case OPTION_UPLOADSPEEDLIMIT:
		if (value > 65535 || value < 1)
			value = 10;
		break;
	case OPTION_BUFFERSIZE:
		if (value < 256 || value > (1024*1024))
			value = 32768;
		break;
	case OPTION_BUFFERSIZE2:
		if (value < 256 || value > (1024*1024))
			value = 65536;
		break;
	case OPTION_CUSTOMPASVIPTYPE:
		if (value < 0 || value > 2)
			value = 0;
		break;
	case OPTION_MODEZ_USE:
		if (value < 0 || value > 1)
			value = 0;
		break;
	case OPTION_MODEZ_LEVELMIN:
		if (value < 0 || value > 8)
			value = 1;
		break;
	case OPTION_MODEZ_LEVELMAX:
		if (value < 8 || value > 9)
			value = 9;
		break;
	case OPTION_MODEZ_ALLOWLOCAL:
		if (value < 0 || value > 1)
			value = 0;
		break;
	case OPTION_AUTOBAN_ATTEMPTS:
		if (value < 5)
			value = 5;
		if (value > 999)
			value = 999;
		break;
	case OPTION_AUTOBAN_BANTIME:
		if (value < 1)
			value = 1;
		if (value > 999)
			value = 999;
		break;
	}

	Init();

	EnterCritSection(m_Sync);
	m_sOptionsCache[nOptionID-1].nType = 1;
	m_sOptionsCache[nOptionID-1].value = value;
	m_sOptionsCache[nOptionID-1].bCached = TRUE;
	m_OptionsCache[nOptionID-1] = m_sOptionsCache[nOptionID-1];

	LeaveCritSection(m_Sync);

	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));
	CMarkupSTL xml;
	if (!xml.Load(buffer))
		return;

	if (!xml.FindElem( _T("FileZillaServer") ))
		return;

	if (!xml.FindChildElem( _T("Settings") ))
		xml.AddChildElem( _T("Settings") );

	CStdString valuestr;
	valuestr.Format( _T("%I64d"), value);
	xml.IntoElem();
	BOOL res = xml.FindChildElem();
	while (res)
	{
		CStdString name=xml.GetChildAttrib( _T("name"));
		if (!_tcscmp(name, m_Options[nOptionID-1].name))
		{
			xml.SetChildAttrib(_T("name"), m_Options[nOptionID-1].name);
			xml.SetChildAttrib(_T("type"), _T("numeric"));
			xml.SetChildData(valuestr);
			break;
		}
		res=xml.FindChildElem();
	}
	if (!res)
	{
		xml.InsertChildElem(_T("Item"), valuestr);
		xml.SetChildAttrib(_T("name"), m_Options[nOptionID-1].name);
		xml.SetChildAttrib(_T("type"), _T("numeric"));
	}
	xml.Save(buffer);
}

void COptions::SetOption(int nOptionID, LPCTSTR value)
{
	CStdString str = value;
	Init();

	switch (nOptionID)
	{
	case OPTION_SERVERPORT:
	case OPTION_SSLPORTS:
		{
			std::set<int> portSet;
			
			str.TrimLeft(_T(" ,"));

			int pos = str.FindOneOf(_T(" ,"));
			while (pos != -1)
			{
				int port = _ttoi(str.Left(pos));
				if (port >= 1 && port <= 65535)
					portSet.insert(port);
				str = str.Mid(pos + 1);
				str.TrimLeft(_T(" ,"));
				pos = str.FindOneOf(_T(" ,"));
			}
			if (str != _T(""))
			{
				int port = _ttoi(str);
				if (port >= 1 && port <= 65535)
					portSet.insert(port);
			}

			str = _T("");
			for (std::set<int>::const_iterator iter = portSet.begin(); iter != portSet.end(); iter++)
			{
				CStdString tmp;
				tmp.Format(_T("%d "), *iter);
				str += tmp;
			}
			str.TrimRight(' ');
		}
		break;
	case OPTION_WELCOMEMESSAGE:
		{
			std::vector<CStdString> msgLines;
			int oldpos = 0;
			str.Replace(_T("\r\n"), _T("\n"));
			int pos = str.Find(_T("\n"));
			CStdString line;
			while (pos != -1)
			{
				if (pos)
				{
					line = str.Mid(oldpos, pos - oldpos);
					line = line.Left(CONST_WELCOMEMESSAGE_LINESIZE);
					line.TrimRight(_T(" "));
					if (msgLines.size() || line != _T(""))
						msgLines.push_back(line);
				}
				oldpos = pos + 1;
				pos = str.Find(_T("\n"), oldpos);
			}
			line = str.Mid(oldpos);
			if (line != _T(""))
			{
				line = line.Left(CONST_WELCOMEMESSAGE_LINESIZE);
				msgLines.push_back(line);
			}
			str = _T("");
			for (unsigned int i = 0; i < msgLines.size(); i++)
				str += msgLines[i] + _T("\r\n");
			str.TrimRight(_T("\r\n"));
			if (str == _T(""))
			{
				str = _T("%v");
				str += _T("\r\nwritten by Tim Kosse (Tim.Kosse@gmx.de)");
				str += _T("\r\nPlease visit http://sourceforge.net/projects/filezilla/");
			}
		}
		break;
	case OPTION_ADMINIPBINDINGS:
		{
			CStdString sub;
			std::list<CStdString> ipBindList;
			for (unsigned int i = 0; i<_tcslen(value); i++)
			{
				TCHAR cur = value[i];
				if ((cur < '0' || cur > '9') && cur != '.')
				{
					if (sub == _T("") && cur == '*')
					{
						ipBindList.clear();
						ipBindList.push_back(_T("*"));
						break;
					}

					if (sub != _T(""))
					{
						//Parse IP
						SOCKADDR_IN sockAddr;
						memset(&sockAddr,0,sizeof(sockAddr));

						sockAddr.sin_family = AF_INET;
#ifdef _UNICODE
						sockAddr.sin_addr.s_addr = inet_addr(ConvToLocal(sub));
#else
						sockAddr.sin_addr.s_addr = inet_addr(sub);
#endif

						if (sockAddr.sin_addr.s_addr != INADDR_NONE)
						{
#ifdef _UNICODE
							sub = ConvFromLocal(inet_ntoa(sockAddr.sin_addr));
#else
							sub = inet_ntoa(sockAddr.sin_addr);
#endif
							std::list<CStdString>::iterator iter;
							for (iter = ipBindList.begin(); iter!=ipBindList.end(); iter++)
								if (*iter==sub)
									break;
							if (iter == ipBindList.end())
								ipBindList.push_back(sub);
						}
						sub = _T("");
					}
				}
				else
					sub += cur;
			}
			if (sub != _T(""))
			{
				//Parse IP
				SOCKADDR_IN sockAddr;
				memset(&sockAddr,0,sizeof(sockAddr));

				sockAddr.sin_family = AF_INET;
#ifdef _UNICODE
				sockAddr.sin_addr.s_addr = inet_addr(ConvToLocal(sub));
#else
				sockAddr.sin_addr.s_addr = inet_addr(sub);
#endif

				if (sockAddr.sin_addr.s_addr != INADDR_NONE)
				{
#ifdef _UNICODE
					sub = ConvFromLocal(inet_ntoa(sockAddr.sin_addr));
#else
					sub = inet_ntoa(sockAddr.sin_addr);
#endif
					std::list<CStdString>::iterator iter;
					for (iter = ipBindList.begin(); iter!=ipBindList.end(); iter++)
						if (*iter==sub)
							break;
					if (iter == ipBindList.end())
						ipBindList.push_back(sub);
				}
				sub = _T("");
			}
			str = _T("");
			for (std::list<CStdString>::iterator iter = ipBindList.begin(); iter!=ipBindList.end(); iter++)
				if (*iter != _T("127.0.0.1"))
					str += *iter + _T(" ");

			str.TrimRight(_T(" "));
		}
		break;
	case OPTION_ADMINPASS:
		if (str != _T("") && str.GetLength() < 6)
			return;
		break;
	case OPTION_MODEZ_DISALLOWED_IPS:
	case OPTION_IPFILTER_ALLOWED:
	case OPTION_IPFILTER_DISALLOWED:
	case OPTION_ADMINIPADDRESSES:
		{
			str.Replace('\r', ' ');
			str.Replace('\n', ' ');
			str.Replace('\r', ' ');
			while (str.Replace(_T("  "), _T(" ")));
			str += _T(" ");

			CStdString ips;

			int pos = str.Find(' ');
			while (pos != -1)
			{
				CStdString sub = str.Left(pos);
				str = str.Mid(pos + 1);
				str.TrimLeft(' ');

				if (sub == _T("*"))
					ips += _T(" ") + sub;
				else
				{
					if (IsValidAddressFilter(sub))
						ips += " " + sub;
					pos = str.Find(' ');
				}
			}
			ips.TrimLeft(' ');

			str = ips;
		}
		break;
	case OPTION_IPBINDINGS:
		{
			CStdString sub;
			std::list<CStdString> ipBindList;
			for (unsigned int i = 0; i<_tcslen(value); i++)
			{
				TCHAR cur = value[i];
				if ((cur < '0' || cur > '9') && cur != '.')
				{
					if (sub == _T("") && cur == '*')
					{
						ipBindList.clear();
						ipBindList.push_back(_T("*"));
						break;
					}

					if (sub != _T(""))
					{
						//Parse IP
						SOCKADDR_IN sockAddr;
						memset(&sockAddr, 0, sizeof(sockAddr));

						sockAddr.sin_family = AF_INET;
#ifdef _UNICODE
						sockAddr.sin_addr.s_addr = inet_addr(ConvToLocal(sub));
#else
						sockAddr.sin_addr.s_addr = inet_addr(sub);
#endif
						if (sockAddr.sin_addr.s_addr != INADDR_NONE)
						{
#ifdef _UNICODE
							sub = ConvFromLocal(inet_ntoa(sockAddr.sin_addr));
#else
							sub = inet_ntoa(sockAddr.sin_addr);
#endif
							std::list<CStdString>::iterator iter;
							for (iter = ipBindList.begin(); iter != ipBindList.end(); iter++)
								if (*iter == sub)
									break;
							if (iter == ipBindList.end())
								ipBindList.push_back(sub);
						}
						sub = _T("");
					}
				}
				else
					sub += cur;
			}
			if (sub != _T(""))
			{
				//Parse IP
				SOCKADDR_IN sockAddr;
				memset(&sockAddr,0,sizeof(sockAddr));

				sockAddr.sin_family = AF_INET;
#ifdef _UNICODE
				sockAddr.sin_addr.s_addr = inet_addr(ConvToLocal(sub));
#else
				sockAddr.sin_addr.s_addr = inet_addr(sub);
#endif
				if (sockAddr.sin_addr.s_addr != INADDR_NONE)
				{
#ifdef _UNICODE
					sub = ConvFromLocal(inet_ntoa(sockAddr.sin_addr));
#else
					sub = inet_ntoa(sockAddr.sin_addr);
#endif
					std::list<CStdString>::iterator iter;
					for (iter = ipBindList.begin(); iter != ipBindList.end(); iter++)
						if (*iter == sub)
							break;
					if (iter == ipBindList.end())
						ipBindList.push_back(sub);
				}
				sub = "";
			}

			if (ipBindList.empty())
				ipBindList.push_back(_T("*"));

			str = _T("");
			for (std::list<CStdString>::iterator iter = ipBindList.begin(); iter != ipBindList.end(); iter++)
				str += *iter + _T(" ");

			str.TrimRight(_T(" "));
		}
		break;
	case OPTION_CUSTOMPASVIPSERVER:
		if (str.Find(_T("filezilla.sourceforge.net")) != -1)
			str = _T("http://ip.filezilla-project.org/ip.php");
		break;
	}
	EnterCritSection(m_Sync);
	m_sOptionsCache[nOptionID-1].bCached = TRUE;
	m_sOptionsCache[nOptionID-1].nType = 0;
	m_sOptionsCache[nOptionID-1].str = str;
	m_OptionsCache[nOptionID-1]=m_sOptionsCache[nOptionID-1];
	LeaveCritSection(m_Sync);

	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));
	CMarkupSTL xml;
	if (!xml.Load(buffer))
		return;

	if (!xml.FindElem( _T("FileZillaServer") ))
		return;

	if (!xml.FindChildElem( _T("Settings") ))
		xml.AddChildElem( _T("Settings") );

	xml.IntoElem();
	BOOL res=xml.FindChildElem();
	while (res)
	{
		CStdString name=xml.GetChildAttrib( _T("name"));
		if (!_tcscmp(name, m_Options[nOptionID-1].name))
		{
			xml.SetChildAttrib(_T("name"), m_Options[nOptionID-1].name);
			xml.SetChildAttrib(_T("type"), _T("string"));
			xml.SetChildData(str);
			break;
		}
		res=xml.FindChildElem();
	}
	if (!res)
	{
		xml.InsertChildElem( _T("Item"), str );
		xml.SetChildAttrib(_T("name"), m_Options[nOptionID-1].name);
		xml.SetChildAttrib(_T("type"), _T("string"));
	}
	xml.Save(buffer);
}

CStdString COptions::GetOption(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=OPTIONS_NUM);
	ASSERT(!m_Options[nOptionID-1].nType);
	Init();

	if (m_OptionsCache[nOptionID-1].bCached)
		return m_OptionsCache[nOptionID-1].str;

	EnterCritSection(m_Sync);

	if (!m_sOptionsCache[nOptionID-1].bCached)
	{
		//Default values
		switch (nOptionID)
		{
		case OPTION_SERVERPORT:
			m_sOptionsCache[nOptionID-1].str = _T("21");
			break;
		case OPTION_WELCOMEMESSAGE:
			m_sOptionsCache[nOptionID-1].str = _T("%v");
			m_sOptionsCache[nOptionID-1].str += _T("\r\nwritten by Tim Kosse (Tim.Kosse@gmx.de)");
			m_sOptionsCache[nOptionID-1].str += _T("\r\nPlease visit http://sourceforge.net/projects/filezilla/");
			break;
		case OPTION_CUSTOMPASVIPSERVER:
			m_sOptionsCache[nOptionID-1].str = _T("http://ip.filezilla-project.org/ip.php");
			break;
		case OPTION_IPBINDINGS:
			m_sOptionsCache[nOptionID-1].str = _T("*");
			break;
		case OPTION_SSLPORTS:
			m_sOptionsCache[nOptionID-1].str = _T("990");
			break;
		default:
			m_sOptionsCache[nOptionID-1].str = _T("");
			break;
		}
		m_sOptionsCache[nOptionID-1].bCached = TRUE;
		m_sOptionsCache[nOptionID-1].nType = 0;
	}
	m_OptionsCache[nOptionID-1] = m_sOptionsCache[nOptionID - 1];
	LeaveCritSection(m_Sync);
	return m_OptionsCache[nOptionID-1].str;
}

_int64 COptions::GetOptionVal(int nOptionID)
{
	ASSERT(nOptionID>0 && nOptionID<=OPTIONS_NUM);
	ASSERT(m_Options[nOptionID-1].nType == 1);
	Init();

	if (m_OptionsCache[nOptionID-1].bCached)
		return m_OptionsCache[nOptionID-1].value;

	EnterCritSection(m_Sync);

	if (!m_sOptionsCache[nOptionID-1].bCached)
	{
		//Default values
		switch (nOptionID)
		{
			case OPTION_MAXUSERS:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_THREADNUM:
				m_sOptionsCache[nOptionID-1].value = 2;
				break;
			case OPTION_TIMEOUT:
			case OPTION_NOTRANSFERTIMEOUT:
				m_sOptionsCache[nOptionID-1].value = 120;
				break;
			case OPTION_LOGINTIMEOUT:
				m_sOptionsCache[nOptionID-1].value = 60;
				break;
			case OPTION_ADMINPORT:
				m_sOptionsCache[nOptionID-1].value = 14147;
				break;
			case OPTION_DOWNLOADSPEEDLIMIT:
			case OPTION_UPLOADSPEEDLIMIT:
				m_sOptionsCache[nOptionID-1].value = 10;
				break;
			case OPTION_BUFFERSIZE:
				m_sOptionsCache[nOptionID-1].value = 32768;
				break;
			case OPTION_CUSTOMPASVIPTYPE:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_MODEZ_USE:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_MODEZ_LEVELMIN:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_MODEZ_LEVELMAX:
				m_sOptionsCache[nOptionID-1].value = 9;
				break;
			case OPTION_MODEZ_ALLOWLOCAL:
				m_sOptionsCache[nOptionID-1].value = 0;
				break;
			case OPTION_ALLOWEXPLICITSSL:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_BUFFERSIZE2:
				m_sOptionsCache[nOptionID-1].value = 65536;
				break;
			case OPTION_NOEXTERNALIPONLOCAL:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_ACTIVE_IGNORELOCAL:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_AUTOBAN_BANTIME:
				m_sOptionsCache[nOptionID-1].value = 1;
				break;
			case OPTION_AUTOBAN_ATTEMPTS:
				m_sOptionsCache[nOptionID-1].value = 5;
				break;
			default:
				m_sOptionsCache[nOptionID-1].value = 0;
		}
		m_sOptionsCache[nOptionID-1].bCached=TRUE;
		m_sOptionsCache[nOptionID-1].nType=1;
	}
	m_OptionsCache[nOptionID-1]=m_sOptionsCache[nOptionID-1];
	LeaveCritSection(m_Sync);
	return m_OptionsCache[nOptionID-1].value;
}

void COptions::UpdateInstances()
{
	EnterCritSection(m_Sync);
	for (std::list<COptions *>::iterator iter=m_InstanceList.begin(); iter!=m_InstanceList.end(); iter++)
	{
		ASSERT((*iter)->m_pOptionsHelperWindow);
		::PostMessage((*iter)->m_pOptionsHelperWindow->GetHwnd(), WM_USER, 0, 0);
	}
	LeaveCritSection(m_Sync);
}

void COptions::Init()
{
	if (m_bInitialized)
		return;
	EnterCritSection(m_Sync);
	m_bInitialized = TRUE;
	CFileStatus64 status;
	CMarkupSTL xml;
	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));

	for (int i=0; i<OPTIONS_NUM; i++)
		m_sOptionsCache[i].bCached=FALSE;

	if (!GetStatus64(buffer, status) )
	{
		xml.AddElem( _T("FileZillaServer") );
		if (!xml.Save(buffer))
		{
			LeaveCritSection(m_Sync);
			return;
		}
	}
	else if (status.m_attribute&FILE_ATTRIBUTE_DIRECTORY)
	{
		LeaveCritSection(m_Sync);
		return;
	}

	if (xml.Load(buffer))
	{
		if (xml.FindElem( _T("FileZillaServer") ))
		{
			if (!xml.FindChildElem( _T("Settings") ))
				xml.AddChildElem( _T("Settings") );

			CStdString str;
			xml.IntoElem();
			str=xml.GetTagName();
			while (xml.FindChildElem())
			{
				CStdString value=xml.GetChildData();
				CStdString name=xml.GetChildAttrib( _T("name") );
				CStdString type=xml.GetChildAttrib( _T("type") );
				for (int i=0;i<OPTIONS_NUM;i++)
				{
					if (!_tcscmp(name, m_Options[i].name))
					{
						if (m_sOptionsCache[i].bCached)
							break;
						else
						{
							if (type==_T("numeric"))
							{
								if (m_Options[i].nType!=1)
									break;
								_int64 value64=_ttoi64(value);
								if (IsNumeric(value))
									SetOption(i+1, value64);
							}
							else
							{
								if (m_Options[i].nType!=0)
									break;
								SetOption(i+1, value);
							}
						}
						break;
					}
				}
			}
			ReadSpeedLimits(&xml);

			LeaveCritSection(m_Sync);
			UpdateInstances();
			return;
		}
	}
	LeaveCritSection(m_Sync);
}

bool COptions::IsNumeric(LPCTSTR str)
{
	if (!str)
		return false;
	LPCTSTR p=str;
	while(*p)
	{
		if (*p<'0' || *p>'9')
		{
			return false;
		}
		p++;
	}
	return true;
}

CMarkupSTL *COptions::GetXML()
{
	EnterCritSection(m_Sync);
	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));
	CMarkupSTL *pXML=new CMarkupSTL;
	if (!pXML->Load(buffer))
	{
		LeaveCritSection(m_Sync);
		delete pXML;
		return NULL;
	}

	if (!pXML->FindElem( _T("FileZillaServer") ))
	{
		LeaveCritSection(m_Sync);
		delete pXML;
		return NULL;
	}
	return pXML;
}

BOOL COptions::FreeXML(CMarkupSTL *pXML)
{
	ASSERT(pXML);
	if (!pXML)
		return FALSE;
	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));
	if (!pXML->Save(buffer))
		return FALSE;
	delete pXML;
	LeaveCritSection(m_Sync);
	return TRUE;
}

BOOL COptions::GetAsCommand(char **pBuffer, DWORD *nBufferLength)
{
	int i;
	DWORD len = 2;

	EnterCritSection(m_Sync);
	for (i=0; i<OPTIONS_NUM; i++)
	{
		len+=1;
		if (!m_Options[i].nType)
		{
			len += 3;
			char* utf8 = ConvToNetwork(GetOption(i + 1));
			if (utf8)
			{
				if ((i+1) != OPTION_ADMINPASS)
					len += strlen(utf8);
				else
				{
					if (GetOption(i+1).GetLength() < 6 && *utf8)
						len++;
				}
				delete [] utf8;
			}
		}
		else
			len+=8;
	}

	len +=4;
	SPEEDLIMITSLIST::const_iterator iter;
	for (iter = m_sDownloadSpeedLimits.begin(); iter != m_sDownloadSpeedLimits.end(); iter++)
		len += iter->GetRequiredBufferLen();
	for (iter = m_sUploadSpeedLimits.begin(); iter != m_sUploadSpeedLimits.end(); iter++)
		len += iter->GetRequiredBufferLen();

	*pBuffer=new char[len];
	char *p=*pBuffer;
	*p++ = OPTIONS_NUM/256;
	*p++ = OPTIONS_NUM%256;
	for (i=0; i<OPTIONS_NUM; i++)
	{
		*p++ = m_Options[i].nType;
		switch(m_Options[i].nType) {
		case 0:
			{
				CStdString str = GetOption(i+1);
				if ((i+1)==OPTION_ADMINPASS) //Do NOT send admin password,
											 //instead send empty string if admin pass is set
											 //and send a single char if admin pass is invalid (len < 6)
				{
					if (str.GetLength() >= 6 || str == _T(""))
						str = _T("");
					else
						str = _T("*");
				}

				char* utf8 = ConvToNetwork(str);
				if (!utf8)
				{
					*p++ = 0;
					*p++ = 0;
					*p++ = 0;
				}
				else
				{
					int len = strlen(utf8);
					*p++ = (len / 256) / 256;
					*p++ = len / 256;
					*p++ = len % 256;
					memcpy(p, utf8, len);
					p += len;
					delete [] utf8;
				}
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
			ASSERT(FALSE);
		}
	}

	*p++ = m_sDownloadSpeedLimits.size() << 8;
	*p++ = m_sDownloadSpeedLimits.size() %256;
	for (iter = m_sDownloadSpeedLimits.begin(); iter != m_sDownloadSpeedLimits.end(); iter++)
		p = iter->FillBuffer(p);

	*p++ = m_sUploadSpeedLimits.size() << 8;
	*p++ = m_sUploadSpeedLimits.size() %256;
	for (iter = m_sUploadSpeedLimits.begin(); iter != m_sUploadSpeedLimits.end(); iter++)
		p = iter->FillBuffer(p);

	LeaveCritSection(m_Sync);

	*nBufferLength = len;

	return TRUE;
}

BOOL COptions::ParseOptionsCommand(unsigned char *pData, DWORD dwDataLength, BOOL bFromLocal /*=FALSE*/)
{
	unsigned char *p = pData;
	int num = *p * 256 + p[1];
	p+=2;
	if (num!=OPTIONS_NUM)
		return FALSE;

	int i;
	for (i = 0; i < num; i++)
	{
		if ((DWORD)(p-pData)>=dwDataLength)
			return FALSE;
		int nType = *p++;
		if (!nType)
		{
			if ((DWORD)(p-pData+3) >= dwDataLength)
				return 2;
			int len = *p * 256 * 256 + p[1] * 256 + p[2];
			p += 3;
			if ((DWORD)(p - pData + len) > dwDataLength)
				return FALSE;
			char *pBuffer = new char[len + 1];
			memcpy(pBuffer, p, len);
			pBuffer[len]=0;
			if (!m_Options[i].bOnlyLocal || bFromLocal) //Do not change admin interface settings from remote connections
#ifdef _UNICODE
				SetOption(i+1, ConvFromNetwork(pBuffer));
#else
				SetOption(i+1, ConvToLocal(ConvFromNetwork(pBuffer)));
#endif
			delete [] pBuffer;
			p+=len;
		}
		else if (nType == 1)
		{
			if ((DWORD)(p-pData+8)>dwDataLength)
				return FALSE;
			if (!m_Options[i].bOnlyLocal || bFromLocal) //Do not change admin interface settings from remote connections
				SetOption(i+1, GET64(p));
			p+=8;
		}
		else
			return FALSE;
	}

	SPEEDLIMITSLIST dl;
	SPEEDLIMITSLIST ul;

	if ((DWORD)(p-pData+2)>dwDataLength)
		return FALSE;
	num = *p++ << 8;
	num |= *p++;
	EnterCritSection(m_Sync);
	for (i=0; i<num; i++)
	{
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p)
		{
			LeaveCritSection(m_Sync);
			return FALSE;
		}
		dl.push_back(limit);
	}

	if ((DWORD)(p-pData+2)>dwDataLength)
	{
		LeaveCritSection(m_Sync);
		return FALSE;
	}
	num = *p++ << 8;
	num |= *p++;
	for (i=0; i<num; i++)
	{
		CSpeedLimit limit;
		p = limit.ParseBuffer(p, dwDataLength - (p - pData));
		if (!p)
		{
			LeaveCritSection(m_Sync);
			return FALSE;
		}
		ul.push_back(limit);
	}

	m_sDownloadSpeedLimits = dl;
	m_sUploadSpeedLimits = ul;
	VERIFY(SaveSpeedLimits());

	LeaveCritSection(m_Sync);

	UpdateInstances();
	return TRUE;
}

BOOL COptions::SaveSpeedLimits()
{
	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));
	CMarkupSTL xml;
	if (!xml.Load(buffer))
		return FALSE;

	if (!xml.FindElem( _T("FileZillaServer") ))
		return FALSE;

	EnterCritSection(m_Sync);

	if (!xml.FindChildElem( _T("Settings") ))
		xml.AddChildElem( _T("Settings") );

	xml.IntoElem();

	while (xml.FindChildElem(_T("SpeedLimits")))
		xml.RemoveChildElem();
	xml.AddChildElem(_T("SpeedLimits"));

	CStdString str;

	xml.IntoElem();

	SPEEDLIMITSLIST::iterator iter;

	xml.AddChildElem(_T("Download"));
	xml.IntoElem();
	for (iter = m_sDownloadSpeedLimits.begin(); iter != m_sDownloadSpeedLimits.end(); iter++)
	{
		const CSpeedLimit& limit = *iter;
		xml.AddChildElem(_T("Rule"));

		xml.SetChildAttrib(_T("Speed"), limit.m_Speed);

		xml.IntoElem();

		str.Format(_T("%d"), limit.m_Day);
		xml.AddChildElem(_T("Days"), str);

		if (limit.m_DateCheck)
		{
			xml.AddChildElem(_T("Date"));
			xml.SetChildAttrib(_T("Year"), limit.m_Date.y);
			xml.SetChildAttrib(_T("Month"), limit.m_Date.m);
			xml.SetChildAttrib(_T("Day"), limit.m_Date.d);
		}

		if (limit.m_FromCheck)
		{
			xml.AddChildElem(_T("From"));
			xml.SetChildAttrib(_T("Hour"), limit.m_FromTime.h);
			xml.SetChildAttrib(_T("Minute"), limit.m_FromTime.m);
			xml.SetChildAttrib(_T("Second"), limit.m_FromTime.s);
		}

		if (limit.m_ToCheck)
		{
			xml.AddChildElem(_T("To"));
			xml.SetChildAttrib(_T("Hour"), limit.m_ToTime.h);
			xml.SetChildAttrib(_T("Minute"), limit.m_ToTime.m);
			xml.SetChildAttrib(_T("Second"), limit.m_ToTime.s);
		}

		xml.OutOfElem();
	}
	xml.OutOfElem();

	xml.AddChildElem(_T("Upload"));
	xml.IntoElem();
	for (iter = m_sUploadSpeedLimits.begin(); iter != m_sUploadSpeedLimits.end(); iter++)
	{
		const CSpeedLimit& limit = *iter;
		xml.AddChildElem(_T("Rule"));

		xml.SetChildAttrib(_T("Speed"), limit.m_Speed);

		xml.IntoElem();

		str.Format(_T("%d"), limit.m_Day);
		xml.AddChildElem(_T("Days"), str);

		if (limit.m_DateCheck)
		{
			xml.AddChildElem(_T("Date"));
			xml.SetChildAttrib(_T("Year"), limit.m_Date.y);
			xml.SetChildAttrib(_T("Month"), limit.m_Date.m);
			xml.SetChildAttrib(_T("Day"), limit.m_Date.d);
		}

		if (limit.m_FromCheck)
		{
			xml.AddChildElem(_T("From"));
			xml.SetChildAttrib(_T("Hour"), limit.m_FromTime.h);
			xml.SetChildAttrib(_T("Minute"), limit.m_FromTime.m);
			xml.SetChildAttrib(_T("Second"), limit.m_FromTime.s);
		}

		if (limit.m_ToCheck)
		{
			xml.AddChildElem(_T("To"));
			xml.SetChildAttrib(_T("Hour"), limit.m_ToTime.h);
			xml.SetChildAttrib(_T("Minute"), limit.m_ToTime.m);
			xml.SetChildAttrib(_T("Second"), limit.m_ToTime.s);
		}

		xml.OutOfElem();
	}
	xml.OutOfElem();

	xml.OutOfElem();

	xml.OutOfElem();

	xml.Save(buffer);

	LeaveCritSection(m_Sync);
	return TRUE;
}

BOOL COptions::ReadSpeedLimits(CMarkupSTL *pXML)
{
	EnterCritSection(m_Sync);
	pXML->ResetChildPos();

	while (pXML->FindChildElem(_T("SpeedLimits")))
	{
		CStdString str;
		int n;

		pXML->IntoElem();

		while (pXML->FindChildElem(_T("Download")))
		{
			pXML->IntoElem();

			while (pXML->FindChildElem(_T("Rule")))
			{
				CSpeedLimit limit;
				str = pXML->GetChildAttrib(_T("Speed"));
				n = _ttoi(str);
				if (n < 0 || n > 65535)
					n = 10;
				limit.m_Speed = n;

				pXML->IntoElem();

				if (pXML->FindChildElem(_T("Days")))
				{
					str = pXML->GetChildData();
					if (str != _T(""))
						n = _ttoi(str);
					else
						n = 0x7F;
					limit.m_Day = n & 0x7F;
				}
				pXML->ResetChildPos();

				limit.m_DateCheck = FALSE;
				if (pXML->FindChildElem(_T("Date")))
				{
					limit.m_DateCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Year"));
					n = _ttoi(str);
					if (n < 1900 || n > 3000)
						n = 2003;
					limit.m_Date.y = n;
					str = pXML->GetChildAttrib(_T("Month"));
					n = _ttoi(str);
					if (n < 1 || n > 12)
						n = 1;
					limit.m_Date.m = n;
					str = pXML->GetChildAttrib(_T("Day"));
					n = _ttoi(str);
					if (n < 1 || n > 31)
						n = 1;
					limit.m_Date.d = n;
				}
				pXML->ResetChildPos();

				limit.m_FromCheck = FALSE;
				if (pXML->FindChildElem(_T("From")))
				{
					limit.m_FromCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Hour"));
					n = _ttoi(str);
					if (n < 0 || n > 23)
						n = 0;
					limit.m_FromTime.h = n;
					str = pXML->GetChildAttrib(_T("Minute"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_FromTime.m = n;
					str = pXML->GetChildAttrib(_T("Second"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_FromTime.s = n;
				}
				pXML->ResetChildPos();

				limit.m_ToCheck = FALSE;
				if (pXML->FindChildElem(_T("To")))
				{
					limit.m_ToCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Hour"));
					n = _ttoi(str);
					if (n < 0 || n > 23)
						n = 0;
					limit.m_ToTime.h = n;
					str = pXML->GetChildAttrib(_T("Minute"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_ToTime.m = n;
					str = pXML->GetChildAttrib(_T("Second"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_ToTime.s = n;
				}
				pXML->ResetChildPos();

				pXML->OutOfElem();

				m_sDownloadSpeedLimits.push_back(limit);
			}
			pXML->OutOfElem();
		}
		pXML->ResetChildPos();

		while (pXML->FindChildElem(_T("Upload")))
		{
			pXML->IntoElem();

			while (pXML->FindChildElem(_T("Rule")))
			{
				CSpeedLimit limit;
				str = pXML->GetChildAttrib(_T("Speed"));
				n = _ttoi(str);
				if (n < 0 || n > 65535)
					n = 10;
				limit.m_Speed = n;

				pXML->IntoElem();

				if (pXML->FindChildElem(_T("Days")))
				{
					str = pXML->GetChildData();
					if (str != _T(""))
						n = _ttoi(str);
					else
						n = 0x7F;
					limit.m_Day = n & 0x7F;
				}
				pXML->ResetChildPos();

				limit.m_DateCheck = FALSE;
				if (pXML->FindChildElem(_T("Date")))
				{
					limit.m_DateCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Year"));
					n = _ttoi(str);
					if (n < 1900 || n > 3000)
						n = 2003;
					limit.m_Date.y = n;
					str = pXML->GetChildAttrib(_T("Month"));
					n = _ttoi(str);
					if (n < 1 || n > 12)
						n = 1;
					limit.m_Date.m = n;
					str = pXML->GetChildAttrib(_T("Day"));
					n = _ttoi(str);
					if (n < 1 || n > 31)
						n = 1;
					limit.m_Date.d = n;
				}
				pXML->ResetChildPos();

				limit.m_FromCheck = FALSE;
				if (pXML->FindChildElem(_T("From")))
				{
					limit.m_FromCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Hour"));
					n = _ttoi(str);
					if (n < 0 || n > 23)
						n = 0;
					limit.m_FromTime.h = n;
					str = pXML->GetChildAttrib(_T("Minute"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_FromTime.m = n;
					str = pXML->GetChildAttrib(_T("Second"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_FromTime.s = n;
				}
				pXML->ResetChildPos();

				limit.m_ToCheck = FALSE;
				if (pXML->FindChildElem(_T("To")))
				{
					limit.m_ToCheck = TRUE;
					str = pXML->GetChildAttrib(_T("Hour"));
					n = _ttoi(str);
					if (n < 0 || n > 23)
						n = 0;
					limit.m_ToTime.h = n;
					str = pXML->GetChildAttrib(_T("Minute"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_ToTime.m = n;
					str = pXML->GetChildAttrib(_T("Second"));
					n = _ttoi(str);
					if (n < 0 || n > 59)
						n = 0;
					limit.m_ToTime.s = n;
				}
				pXML->ResetChildPos();

				pXML->OutOfElem();

				m_sUploadSpeedLimits.push_back(limit);
			}
			pXML->OutOfElem();
		}

		pXML->OutOfElem();
	}
	LeaveCritSection(m_Sync);

	return TRUE;
}

int COptions::GetCurrentSpeedLimit(int nMode)
{
	Init();
	if (nMode)
	{
		int nType = (int)GetOptionVal(OPTION_UPLOADSPEEDLIMITTYPE);
		switch (nType)
		{
			case 0:
				return -1;
			case 1:
				return (int)GetOptionVal(OPTION_UPLOADSPEEDLIMIT);
			default:
				{
					SYSTEMTIME s;
					GetLocalTime(&s);
					for (SPEEDLIMITSLIST::const_iterator iter = m_UploadSpeedLimits.begin(); iter != m_UploadSpeedLimits.end(); iter++)
						if (iter->IsItActive(s))
							return iter->m_Speed;
					return -1;
				}
		}
	}
	else
	{
		int nType = (int)GetOptionVal(OPTION_DOWNLOADSPEEDLIMITTYPE);
		switch (nType)
		{
			case 0:
				return -1;
			case 1:
				return (int)GetOptionVal(OPTION_DOWNLOADSPEEDLIMIT);
			default:
				{
					SYSTEMTIME s;
					GetLocalTime(&s);
					for (SPEEDLIMITSLIST::const_iterator iter = m_DownloadSpeedLimits.begin(); iter != m_DownloadSpeedLimits.end(); iter++)
						if (iter->IsItActive(s))
							return iter->m_Speed;
					return -1;
				}
		}
	}
}

void COptions::ReloadConfig()
{
	EnterCritSection(m_Sync);

	m_bInitialized = TRUE;
	CFileStatus64 status;
	CMarkupSTL xml;
	TCHAR buffer[MAX_PATH + 1000]; //Make it large enough
	GetModuleFileName( 0, buffer, MAX_PATH );
	LPTSTR pos=_tcsrchr(buffer, '\\');
	if (pos)
		*++pos=0;
	_tcscat(buffer, _T("FileZilla Server.xml"));

	for (int i=0; i<OPTIONS_NUM; i++)
		m_sOptionsCache[i].bCached = FALSE;

	if (!GetStatus64(buffer, status) )
	{
		xml.AddElem( _T("FileZillaServer") );
		if (!xml.Save(buffer))
		{
			LeaveCritSection(m_Sync);
			return;
		}
	}
	else if (status.m_attribute&FILE_ATTRIBUTE_DIRECTORY)
	{
		LeaveCritSection(m_Sync);
		return;
	}

	if (xml.Load(buffer))
	{
		if (xml.FindElem( _T("FileZillaServer") ))
		{
			if (!xml.FindChildElem( _T("Settings") ))
				xml.AddChildElem( _T("Settings") );

			CStdString str;
			xml.IntoElem();
			str=xml.GetTagName();
			while (xml.FindChildElem())
			{
				CStdString value=xml.GetChildData();
				CStdString name=xml.GetChildAttrib( _T("name") );
				CStdString type=xml.GetChildAttrib( _T("type") );
				for (int i=0;i<OPTIONS_NUM;i++)
				{
					if (!_tcscmp(name, m_Options[i].name))
					{
						if (m_sOptionsCache[i].bCached)
							break;
						else
						{
							if (type == _T("numeric"))
							{
								if (m_Options[i].nType!=1)
									break;
								_int64 value64 = _ttoi64(value);
								if (IsNumeric(value))
									SetOption(i+1, value64);
							}
							else
							{
								if (m_Options[i].nType!=0)
									break;
								SetOption(i+1, value);
							}
						}
						break;
					}
				}
			}
			ReadSpeedLimits(&xml);

			LeaveCritSection(m_Sync);
			UpdateInstances();
			return;
		}
	}
	LeaveCritSection(m_Sync);
}
