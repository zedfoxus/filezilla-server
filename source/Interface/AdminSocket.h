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

#ifndef FILEZILLA_SERVER_INTERFACE_ADMINSOCKET_HEADER
#define FILEZILLA_SERVER_INTERFACE_ADMINSOCKET_HEADER

#include "../AsyncSocketEx.h"
#include <memory>

#include <libfilezilla/buffer.hpp>

class CMainFrame;
class CAdminSocket : public CAsyncSocketEx
{
public:
	std::wstring m_Password;
	bool IsConnected();
	bool SendCommand(int nType);
	bool SendCommand(int nType, void *pData, int nDataLength);
	bool ParseRecvBuffer();
	virtual void Close();
	CAdminSocket(CMainFrame *pMainFrame);
	virtual ~CAdminSocket();
	void DoClose();
	bool IsClosed() { return m_bClosed; }
	bool IsLocal();

protected:
	bool SendPendingData();

	fz::buffer sendBuffer_;

	bool m_bClosed{};
	virtual void OnReceive(int nErrorCode);
	virtual void OnConnect(int nErrorCode);
	virtual void OnSend(int nErrorCode);
	virtual void OnClose(int nErrorCode);
	CMainFrame *m_pMainFrame;

	unsigned char *m_pRecvBuffer;
	unsigned int m_nRecvBufferLen;
	unsigned int m_nRecvBufferPos{};

	int m_nConnectionState{};
};

#endif
