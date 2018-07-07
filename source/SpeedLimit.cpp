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

// Speedcpp: implementation of the CSpeedLimit class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SpeedLimit.h"
#include "xml_utils.h"
#include "pugixml/pugixml.hpp"

#include <libfilezilla/string.hpp>

bool CSpeedLimit::IsItActive(const SYSTEMTIME &time) const
{
	if (m_DateCheck) {
		if ((m_Date.y != time.wYear) ||
			(m_Date.m != time.wMonth) ||
			(m_Date.d != time.wDay))
		{
			return false;
		}
	}
	else {
		int i = (time.wDayOfWeek + 6) % 7;

		if (!(m_Day & (1 << i))) {
			return false;
		}
	}

	int curTime = time.wHour * 60 * 60 +
				  time.wMinute * 60 +
				  time.wSecond;

	int fromTime = 0;
	int toTime = 0;
	if (m_ToCheck) {
		toTime = m_ToTime.h * 60 * 60 +
		         m_ToTime.m * 60 +
		         m_ToTime.s;
	}
	if (m_FromCheck) {
		fromTime = m_FromTime.h * 60 * 60 +
		           m_FromTime.m * 60 +
		           m_FromTime.s;
	}

	if (m_FromCheck && m_ToCheck) {
		int span = toTime - fromTime;
		if (span < 0) {
			span += 24 * 60 * 60;
		}
		int ref = curTime - fromTime;
		if (ref < 0) {
			ref += 24 * 60 * 60;
		}
		if (span < ref) {
			return false;
		}
	}
	else {
		if (m_ToCheck) {
			if (toTime < curTime) {
				return false;
			}
		}

		if (m_FromCheck) {
			if (fromTime > curTime) {
				return false;
			}
		}
	}

	return true;
}

int CSpeedLimit::GetRequiredBufferLen() const
{
	return	4 + //Speed
			4 + //date
			6 +	//2 * time
			1;  //Weekday

}

unsigned char * CSpeedLimit::FillBuffer(unsigned char *p) const
{
	*p++ = m_Speed >> 24;
	*p++ = (m_Speed >> 16) % 256;
	*p++ = (m_Speed >> 8) % 256;
	*p++ = m_Speed % 256;

	if (m_DateCheck) {
		*p++ = m_Date.y >> 8;
		*p++ = m_Date.y % 256;
		*p++ = m_Date.m;
		*p++ = m_Date.d;
	}
	else {
		memset(p, 0, 4);
		p += 4;
	}

	if (m_FromCheck) {
		*p++ = m_FromTime.h;
		*p++ = m_FromTime.m;
		*p++ = m_FromTime.s;
	}
	else {
		memset(p, 0, 3);
		p += 3;
	}

	if (m_ToCheck) {
		*p++ = m_ToTime.h;
		*p++ = m_ToTime.m;
		*p++ = m_ToTime.s;
	}
	else {
		memset(p, 0, 3);
		p += 3;
	}

	*p++ = m_Day;

	return p;
}

unsigned char * CSpeedLimit::ParseBuffer(unsigned char *pBuffer, int length)
{
	if (length < GetRequiredBufferLen()) {
		return 0;
	}

	unsigned char *p = pBuffer;

	m_Speed = *p++ << 24;
	m_Speed |= *p++ << 16;
	m_Speed |= *p++ << 8;
	m_Speed |= *p++;

	if (m_Speed > 1048576) {
		m_Speed = 1048576;
	}

	char tmp[4] = {0};

	if (memcmp(p, tmp, 4)) {
		m_DateCheck = true;
		m_Date.y = *p++ << 8;
		m_Date.y |= *p++;
		m_Date.m = *p++;
		m_Date.d = *p++;
		if (m_Date.y < 1900 || m_Date.y > 3000 || m_Date.m < 1 || m_Date.m > 12 || m_Date.d < 1 || m_Date.d > 31) {
			return false;
		}
	}
	else {
		p += 4;
		m_DateCheck = false;
	}

	if (memcmp(p, tmp, 3)) {
		m_FromCheck = true;
		m_FromTime.h = *p++;
		m_FromTime.m = *p++;
		m_FromTime.s = *p++;
		if (m_FromTime.h > 23 || m_FromTime.m > 59 || m_FromTime.s > 59) {
			return false;
		}
	}
	else {
		p += 3;
		m_FromCheck = false;
	}

	if (memcmp(p, tmp, 3)) {
		m_ToCheck = true;
		m_ToTime.h = *p++;
		m_ToTime.m = *p++;
		m_ToTime.s = *p++;
		if (m_ToTime.h > 23 || m_ToTime.m > 59 || m_ToTime.s > 59) {
			return false;
		}
	}
	else {
		p += 3;
		m_ToCheck = false;
	}

	m_Day = *p++;

	return p;
}

static void SaveTime(pugi::xml_node element, CSpeedLimit::t_time t)
{
	element.append_attribute("Hour").set_value(t.h);
	element.append_attribute("Minute").set_value(t.m);
	element.append_attribute("Second").set_value(t.s);
}

void CSpeedLimit::Save(pugi::xml_node & element) const
{
	element.append_attribute("Speed").set_value(m_Speed);

	auto days = element.append_child("Days");
	days.text().set(m_Day);

	if (m_DateCheck) {
		auto date = element.append_child("Date");
		date.append_attribute("Year").set_value(m_Date.y);
		date.append_attribute("Month").set_value(m_Date.m);
		date.append_attribute("Day").set_value(m_Date.d);
	}

	if (m_FromCheck) {
		auto from = element.append_child("From");
		SaveTime(from, m_FromTime);
	}

	if (m_ToCheck) {
		auto to = element.append_child("To");
		SaveTime(to, m_ToTime);
	}
}

static CSpeedLimit::t_time ReadTime(pugi::xml_node const& element)
{
	CSpeedLimit::t_time t;

	t.h = element.attribute("Hour").as_int();
	if (t.h < 0 || t.h > 23) {
		t.h = 0;
	}

	t.m = element.attribute("Minute").as_int();
	if (t.m < 0 || t.m > 59) {
		t.m = 0;
	}

	t.s = element.attribute("Second").as_int();
	if (t.s < 0 || t.s > 59) {
		t.s = 0;
	}
	
	return t;
}

bool CSpeedLimit::Load(pugi::xml_node const& element)
{
	m_Speed = element.attribute("Speed").as_int();
	if (m_Speed < 0) {
		m_Speed = 0;
	}
	else if (m_Speed > 1048576) {
		m_Speed = 1048576;
	}

	m_Day = element.child("Days").text().as_int(0x7F) & 0x7F;

	m_DateCheck = false;

	auto date = element.child("Date");
	if (date) {
		m_DateCheck = true;

		m_Date.y = element.attribute("Year").as_int();
		if (m_Date.y < 1900 || m_Date.y > 3000) {
			m_Date.y = 2018;
		}

		m_Date.m = element.attribute("Month").as_int();
		if (m_Date.m < 1 || m_Date.m > 12) {
			m_Date.m = 1;
		}

		m_Date.d = element.attribute("Day").as_int();
		if (m_Date.d < 1 || m_Date.d > 31) {
			m_Date.d = 1;
		}
	}

	auto from = element.child("From");
	if (from) {
		m_FromCheck = true;
		m_FromTime = ReadTime(from);
	}
	else {
		m_FromCheck = false;
	}

	auto to = element.child("To");
	if (to) {
		m_ToCheck = true;
		m_ToTime = ReadTime(to);
	}
	else {
		m_ToCheck = false;
	}

	return true;
}
