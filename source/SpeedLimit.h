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

#ifndef FILEZILLA_SERVER_SERVICE_SPEEDLIMIT_HEADER
#define FILEZILLA_SERVER_SERVICE_SPEEDLIMIT_HEADER

namespace pugi {
class xml_node;
}

class CSpeedLimit final
{
public:
	bool IsItActive(const SYSTEMTIME &time) const;

	int GetRequiredBufferLen() const;
	unsigned char * FillBuffer(unsigned char *p) const;
	unsigned char * ParseBuffer(unsigned char *pBuffer, int length);

	bool m_DateCheck{};
	struct t_date {
		int y{};
		int m{};
		int d{};
	} m_Date;

	bool m_FromCheck{};
	bool m_ToCheck{};

	struct t_time {
		int h{};
		int m{};
		int s{};
	} m_FromTime, m_ToTime;
	int	m_Speed{100};
	int m_Day{};

	void Save(pugi::xml_node & element) const;
	bool Load(pugi::xml_node const& element);
};

typedef std::vector<CSpeedLimit> SPEEDLIMITSLIST;

#endif
