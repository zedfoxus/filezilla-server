// FileZilla Server - a Windows ftp server

// Copyright (C) 2004 - Tim Kosse <tim.kosse@gmx.de>

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
#include "iputils.h"
#include <boost/regex.hpp>

bool IsLocalhost(const CStdString& ip)
{
	if (ip.Left(4) == _T("127."))
		return true;
	if (ip == _T("::1"))
		return true;

	return false;
}

bool IsValidAddressFilter(CStdString& filter, bool allowWildcards /*=true*/)
{
	// Check for regular expression syntax (to match against the hostname).
	if (filter.Left(1) == _T("/") && filter.Right(1) == _T("/"))
	{
		if (filter.GetLength() < 3)
			return false;

#ifdef _UNICODE
		const CStdStringA localFilter = ConvToLocal(filter.Mid(1, filter.GetLength() - 2));
#else
		const CStdString localFilter = filter;
#endif
		try
		{
			boost::regex e(localFilter);
		}
		catch (std::runtime_error)
		{
			return false;
		}
		return true;
	}

	if (filter.Find(_T("..")) != -1)
		return false;

	// Check for IP/subnet syntax.
	TCHAR chr = '/';
	int pos = filter.Find(chr);
	if (pos == -1)
	{
		// Check for IP range syntax.
		chr = '-';
		pos = filter.Find(chr);
	}
	if (pos != -1)
	{
		// Only one slash or dash is allowed.
		CStdString right = filter.Mid(pos + 1);
		if (right.Find('/') != -1 || right.Find('-') != -1)
			return false;

		// When using IP/subnet or IP range syntax, no wildcards are allowed.
		if (!IsValidAddressFilter(right, false))
			return false;
		CStdString left = filter.Left(pos);
		if (!IsValidAddressFilter(left, false))
			return false;

		filter = left + chr + right;
	}
	else
	{
		int dotCount = 0;
		for (const TCHAR *cur = filter; *cur; cur++)
		{
			// Check for valid characters.
			if ((*cur < '0' || *cur > '9') && *cur != '.' && (!allowWildcards || (*cur != '*' && *cur != '?')))
				return false;
			else
			{
				// Count the number of contained dots.
				if (*cur == '.')
					dotCount++;
			}
		}
		if (dotCount != 3 || filter.Right(1) == _T(".") || filter.Left(1) == _T("."))
			return false;
	}

	// Merge redundant wildcards.
	while (filter.Replace(_T("?*"), _T("*")));
	while (filter.Replace(_T("*?"), _T("*")));
	while (filter.Replace(_T("**"), _T("*")));

	return true;
}

bool MatchesFilter(const CStdString& filter, unsigned int ip, LPCTSTR pIp)
{
	// ip and pIp are the same IP, one as number, the other one as string.

	// A single asterix matches all IPs.
	if (filter == _T("*"))
		return true;

	// Check for regular expression filter.
	if (filter.Left(1) == _T("/") && filter.Right(1) == _T("/"))
		return MatchesRegExp(filter.Mid(1, filter.GetLength() - 2), ntohl(ip));

	// Check for IP range syntax.
	int pos2 = filter.Find('-');
	if (pos2 != -1)
	{
#ifdef _UNICODE
		CStdStringA offset = ConvToLocal(filter.Left(pos2));
		CStdStringA range = ConvToLocal(filter.Mid(pos2 + 1));
#else
		CStdString offset = filter.Left(pos2);
		CStdString range = filter.Mid(pos2 + 1);
#endif
		if (offset == "" || range == "")
			return false;
		unsigned int min = htonl(inet_addr(offset));
		unsigned int max = htonl(inet_addr(range));
		return (ip >= min) && (ip <= max);
	}

	// Check for IP/subnet syntax.
	pos2 = filter.Find('/');
	if (pos2 != -1)
	{
#ifdef _UNICODE
		CStdStringA ipStr = ConvToLocal(filter.Left(pos2));
		CStdStringA subnet = ConvToLocal(filter.Mid(pos2 + 1));
#else
		CStdString ipStr = filter.Left(pos2);
		CStdString subnet = filter.Mid(pos2 + 1);
#endif
		if (ipStr == "" || subnet == "")
			return false;

		unsigned int ip2 = htonl(inet_addr(ipStr));
		unsigned int mask = htonl(inet_addr(subnet));
		return (ip & mask) == (ip2 & mask);
	}

	// Validate the IP address.
	LPCTSTR c = filter;
	LPCTSTR p = pIp;

	// Look if remote and local IP match.
	while (*c && *p)
	{
		if (*c == '*')
		{
			if (*p == '.')
				break;

			// Advance to the next dot.
			while (*p && *p != '.')
				p++;
			c++;
			continue;
		}
		else if (*c != '?') // If it's '?', we don't have to compare.
			if (*c != *p)
				break;
		p++;
		c++;
	}
	return (!*c) && (!*p);
}

bool MatchesRegExp(const CStdString& filter, unsigned int addr)
{
	// If the IP could not be resolved to a hostname, return false.
	HOSTENT *host = gethostbyaddr((char*)&addr, sizeof(addr), AF_INET);
	if (!host)
		return false;
#ifdef _UNICODE
	boost::regex e(ConvToLocal(filter));
#else
	boost::regex e(filter);
#endif
	return regex_match(host->h_name, e);
}

bool IsUnroutableIP(unsigned int ip)
{
	if (MatchesFilter(_T("127.0.0.1/255.0.0.0"), ip, 0))
		return true;
	
	if (MatchesFilter(_T("10.0.0.0/255.0.0.0"), ip, 0))
		return true;
	
	if (MatchesFilter(_T("172.16.0.0/255.240.0.0"), ip, 0))
		return true;
	
	if (MatchesFilter(_T("192.168.0.0/255.255.0.0"), ip, 0))
		return true;
	
	if (MatchesFilter(_T("169.254.0.0/255.255.0.0"), ip, 0))
		return true;

	return false;
}

bool ParseIPFilter(CStdString in, std::list<CStdString>* output /*=0*/)
{
	bool valid = true;

	in.Replace(_T("\n"), _T(" "));
	in.Replace(_T("\r"), _T(" "));
	in.Replace(_T("\t"), _T(" "));
	while (in.Replace(_T("  "), _T(" ")));
	in.TrimLeft(_T(" "));
	in.TrimRight(_T(" "));
	in += _T(" ");

	int pos;
	while ((pos = in.Find(_T(" "))) != -1)
	{
		CStdString ip = in.Left(pos);
		if (ip == _T(""))
			break;
		in = in.Mid(pos + 1);

		if (ip == _T("*") || IsValidAddressFilter(ip))
		{
			if (output)
				output->push_back(ip);
		}
		else
			valid = false;
	}

	return valid;
}

CStdString GetIPV6LongForm(CStdString short_address)
{
	if (short_address[0] == '[')
	{
		if (short_address[short_address.GetLength() - 1] != ']')
			return _T("");
		short_address = short_address.Mid(1, short_address.GetLength() - 2);
	}
	short_address.MakeLower();

	TCHAR buffer[40] = { '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', 0
					   };
	TCHAR* out = buffer;

	const unsigned int len = short_address.GetLength();
	if (len > 39)
		return _T("");

	// First part, before possible ::
	unsigned int i = 0;
	unsigned int grouplength = 0;
	for (i = 0; i < len + 1; i++)
	{
		const TCHAR& c = short_address[i];
		if (c == ':' || !c)
		{
			if (!grouplength)
			{
				// Empty group length, not valid
				if (!c || short_address[i + 1] != ':')
					return _T("");
				i++;
				break;
			}

			out += 4 - grouplength;
			for (unsigned int j = grouplength; j > 0; j--)
				*out++ = short_address[i - j];
			// End of string...
			if (!c)
			{
				if (!*out)
					// ...on time
					return buffer;
				else
					// ...premature
					return _T("");
			}
			else if (!*out)
			{
				// Too long
				return _T("");
			}

			out++;

			grouplength = 0;
			if (short_address[i + 1] == ':')
			{
				i++;
				break;
			}
			continue;
		}
		else if ((c < '0' || c > '9') &&
				 (c < 'a' || c > 'f'))
		{
			// Invalid character
			return _T("");
		}
		// Too long group
		if (++grouplength > 4)
			return _T("");
	}

	// Second half after ::

	TCHAR* end_first = out;
	out = &buffer[38];
	unsigned int stop = i;
	for (i = len - 1; i > stop; i--)
	{
		if (out < end_first)
		{
			// Too long
			return _T("");
		}

		const TCHAR& c = short_address[i];
		if (c == ':')
		{
			if (!grouplength)
			{
				// Empty group length, not valid
				return _T("");
			}

			out -= 5 - grouplength;

			grouplength = 0;
			continue;
		}
		else if ((c < '0' || c > '9') &&
				 (c < 'a' || c > 'f'))
		{
			// Invalid character
			return _T("");
		}
		// Too long group
		if (++grouplength > 4)
			return _T("");
		*out-- = c;
	}
	if (!grouplength)
	{
		// Empty group length, not valid
		return _T("");
	}
	out -= 5 - grouplength;
	out += 2;

	int diff = out - end_first;
	if (diff < 0 || diff % 5)
		return _T("");

	return buffer;
}

int DigitHexToDecNum(TCHAR c)
{
	if (c >= 'a')
		return c - 'a' + 10;
	if (c >= 'A')
		return c - 'A' + 10;
	else
		return c - '0';
}

bool IsRoutableAddress(const CStdString& address)
{
	if (address.Find(_T(":")) != -1)
	{
		CStdString long_address = GetIPV6LongForm(address);
		if (long_address.empty())
			return false;
		if (long_address[0] == '0')
		{
			// ::/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0000"))
				return false;
			// ::1/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0001"))
				return false;

			if (long_address.Left(30) == _T("0000:0000:0000:0000:0000:ffff:"))
			{
				// IPv4 mapped
				CStdString ipv4;
				ipv4.Format(_T("%d.%d.%d.%d"),
						DigitHexToDecNum(long_address[30]) * 16 + DigitHexToDecNum(long_address[31]),
						DigitHexToDecNum(long_address[32]) * 16 + DigitHexToDecNum(long_address[33]),
						DigitHexToDecNum(long_address[35]) * 16 + DigitHexToDecNum(long_address[36]),
						DigitHexToDecNum(long_address[37]) * 16 + DigitHexToDecNum(long_address[38]));
				return IsRoutableAddress(ipv4);
			}

			return true;
		}
		if (long_address[0] == 'f')
		{
			if (long_address[1] == 'e')
			{
				// fe80::/10 (link local)
				const TCHAR& c = long_address[2];
				int v;
				if (c >= 'a')
					v = c - 'a' + 10;
				else
					v = c - '0';
				if ((v & 0xc) == 0x8)
					return false;

				return true;
			}
			else if (long_address[1] == 'c' || long_address[1] == 'd')
			{
				// fc00::/7 (site local)
				return false;
			}
		}

		return true;
	}
	else
	{
		// Assumes address is already a valid IP address
		if (address.Left(3) == _T("127") ||
			address.Left(3) == _T("10.") ||
			address.Left(7) == _T("192.168") ||
			address.Left(7) == _T("169.254"))
			return false;

		if (address.Left(3) == _T("172"))
		{
			CStdString middle = address.Mid(4);
			int pos = address.Find(_T("."));
			if (pos == -1)
				return false;
			int part = _ttoi(middle.Left(pos));

			if (part >= 16 && part <= 31)
				return false;
		}

		return true;
	}
}
