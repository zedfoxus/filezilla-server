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

// OptionsGeneralWelcomemessagePage.cpp: Implementierungsdatei
//

#include "stdafx.h"
#include "filezilla server.h"
#include "OptionsDlg.h"
#include "OptionsPage.h"
#include "OptionsGeneralWelcomemessagePage.h"

#include <libfilezilla/string.hpp>

/////////////////////////////////////////////////////////////////////////////
// Dialogfeld COptionsGeneralWelcomemessagePage


COptionsGeneralWelcomemessagePage::COptionsGeneralWelcomemessagePage(COptionsDlg *pOptionsDlg, CWnd* pParent /*=NULL*/)
	: COptionsPage(pOptionsDlg, COptionsGeneralWelcomemessagePage::IDD, pParent)
	, m_hideWelcomeMessage(FALSE)
{
	//{{AFX_DATA_INIT(COptionsGeneralWelcomemessagePage)
	m_WelcomeMessage = _T("");
	//}}AFX_DATA_INIT
}


void COptionsGeneralWelcomemessagePage::DoDataExchange(CDataExchange* pDX)
{
	COptionsPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsGeneralWelcomemessagePage)
	DDX_Text(pDX, IDC_OPTIONS_GENERAL_WELCOMEMESSAGE_WELCOMEMESSAGE, m_WelcomeMessage);
	DDV_MaxChars(pDX, m_WelcomeMessage, 7500);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_OPTIONS_WELCOMEMESSAGE_HIDE, m_hideWelcomeMessage);
}


BEGIN_MESSAGE_MAP(COptionsGeneralWelcomemessagePage, COptionsPage)
	//{{AFX_MSG_MAP(COptionsGeneralWelcomemessagePage)
		// HINWEIS: Der Klassen-Assistent fügt hier Zuordnungsmakros für Nachrichten ein
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Behandlungsroutinen für Nachrichten COptionsGeneralWelcomemessagePage

void COptionsGeneralWelcomemessagePage::LoadData()
{
	m_WelcomeMessage = m_pOptionsDlg->GetOption(OPTION_WELCOMEMESSAGE).c_str();
	m_hideWelcomeMessage = m_pOptionsDlg->GetOptionVal(OPTION_WELCOMEMESSAGE_HIDE) != 0;
}

void COptionsGeneralWelcomemessagePage::SaveData()
{
	std::wstring msg;

	auto lines = fz::strtok(fz::replaced_substrings(m_WelcomeMessage.GetString(), L"\r\n", L"\n"), '\n');
	for (auto const& line : lines) {
		auto trimmedLine = fz::trimmed(line.substr(0, CONST_WELCOMEMESSAGE_LINESIZE));
		msg += line + _T("\r\n");
	}
	msg = fz::trimmed(msg);

	m_pOptionsDlg->SetOption(OPTION_WELCOMEMESSAGE, msg);
	m_pOptionsDlg->SetOption(OPTION_WELCOMEMESSAGE_HIDE, m_hideWelcomeMessage);
}