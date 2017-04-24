// FileZilla Server - a Windows ftp server

// Copyright (C) 2004 - Tim Kosse <tim.kosse@filezilla-project.org>

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

#include <memory>
#include <iphlpapi.h>

#include <libfilezilla/format.hpp>
#include <libfilezilla/string.hpp>

bool IsLocalhost(std::wstring const& ip)
{
	if (ip.substr(0, 4) == L"127.") {
		return true;
	}
	if (GetIPV6ShortForm(ip) == L"::1") {
		return true;
	}

	return false;
}

bool IsValidAddressFilter(std::wstring& filter)
{
	std::wstring left;
	auto const pos = filter.find('/');
	int prefixLength = 0;
	if (!pos) {
		return false;
	}
	else if (pos != std::wstring::npos) {
		left = filter.substr(0, pos);
		prefixLength = fz::to_integral<int>(filter.substr(pos + 1), -1);
		if (prefixLength < 0 || prefixLength > 128) {
			return false;
		}
	}
	else {
		left = filter;
	}

	if (!IsIpAddress(left, true)) {
		return false;
	}

	if (left.find(':') != std::wstring::npos) {
		left = GetIPV6ShortForm(left);
	}
	if (pos != -1) {
		filter = fz::sprintf(L"%s/%d", left, prefixLength);
	}
	else {
		filter = left;
	}

	return true;
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

static unsigned long const prefixMasksV4[] = {
	0x00000000u,
	0x80000000u,
	0xc0000000u,
	0xe0000000u,
	0xf0000000u,
	0xf8000000u,
	0xfc000000u,
	0xfe000000u,
	0xff000000u,
	0xff800000u,
	0xffc00000u,
	0xffe00000u,
	0xfff00000u,
	0xfff80000u,
	0xfffc0000u,
	0xfffe0000u,
	0xffff0000u,
	0xffff8000u,
	0xffffc000u,
	0xffffe000u,
	0xfffff000u,
	0xfffff800u,
	0xfffffc00u,
	0xfffffe00u,
	0xffffff00u,
	0xffffff80u,
	0xffffffc0u,
	0xffffffe0u,
	0xfffffff0u,
	0xfffffff8u,
	0xfffffffcu,
	0xfffffffeu,
	0xffffffffu
};

bool MatchesFilter(std::wstring const& filter, std::wstring ip)
{
	// A single asterix matches all IPs.
	if (filter == L"*") {
		return true;
	}

	// Check for IP range syntax.
	size_t pos = filter.find('/');
	if (pos != std::wstring::npos) {
		// CIDR filter
		int prefixLength = fz::to_integral<int>(filter.substr(pos + 1));

		if (ip.find(':') != std::wstring::npos) {
			// IPv6 address
			std::wstring left = GetIPV6LongForm(filter.substr(0, pos));
			if (left.find(':') == std::wstring::npos) {
				return false;
			}
			ip = GetIPV6LongForm(ip);
			wchar_t const* i = ip.c_str();
			wchar_t const* f = left.c_str();
			while (prefixLength >= 4) {
				if (*i != *f) {
					return false;
				}

				if (!*i) {
					return true;
				}

				if (*i == ':') {
					++i;
					++f;
				}
				++i;
				++f;

				prefixLength -= 4;
			}
			if (!prefixLength) {
				return true;
			}

			int mask;
			if (prefixLength == 1) {
				mask = 0x8;
			}
			else if (prefixLength == 2) {
				mask = 0xc;
			}
			else {
				mask = 0xe;
			}

			return (DigitHexToDecNum(*i) & mask) == (DigitHexToDecNum(*f) & mask);
		}
		else {
			if (prefixLength < 0) {
				prefixLength = 0;
			}
			else if (prefixLength > 32) {
				prefixLength = 32;
			}

			// IPv4 address
			std::wstring left = filter.substr(0, pos);
			if (left.find(':') != std::wstring::npos) {
				return false;
			}

			unsigned long i = ntohl(inet_addr(fz::to_string(ip).c_str()));
			unsigned long f = ntohl(inet_addr(fz::to_string(left).c_str()));

			i &= prefixMasksV4[prefixLength];
			f &= prefixMasksV4[prefixLength];
			return i == f;
		}
	}
	else {
		// Literal filter
		if (filter.find(':') != std::wstring::npos) {
			return filter == GetIPV6ShortForm(ip);
		}
		else {
			return filter == ip;
		}
	}
}

bool ParseIPFilter(CStdString in, std::vector<std::wstring>* output)
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
	while ((pos = in.Find(_T(" "))) != -1) {
		std::wstring ip = in.Left(pos);
		if (ip.empty()) {
			break;
		}
		in = in.Mid(pos + 1);

		if (ip == _T("*") || IsValidAddressFilter(ip)) {
			if (output) {
				output->push_back(static_cast<std::wstring>(ip));
			}
		}
		else {
			valid = false;
		}
	}

	return valid;
}

std::wstring GetIPV6LongForm(std::wstring short_address)
{
	if (short_address[0] == '[') {
		if (short_address.back() != ']') {
			return std::wstring();
		}
		short_address = short_address.substr(1, short_address.size() - 2);
	}
	short_address = fz::str_tolower_ascii(short_address);

	wchar_t buffer[40] = { '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', ':',
						 '0', '0', '0', '0', 0
					   };
	wchar_t* out = buffer;

	size_t const len  = short_address.size();
	if (len > 39) {
		return std::wstring();
	}

	// First part, before possible ::
	unsigned int i = 0;
	unsigned int grouplength = 0;
	for (i = 0; i < len + 1; ++i) {
		wchar_t const& c = short_address[i];
		if (c == ':' || !c) {
			if (!grouplength) {
				// Empty group length, not valid
				if (!c || short_address[i + 1] != ':') {
					return std::wstring();
				}
				++i;
				break;
			}

			out += 4 - grouplength;
			for (unsigned int j = grouplength; j > 0; --j) {
				*out++ = short_address[i - j];
			}
			// End of string...
			if (!c) {
				if (!*out) {
					// ...on time
					return buffer;
				}
				else {
					// ...premature
					return std::wstring();
				}
			}
			else if (!*out) {
				// Too long
				return std::wstring();
			}

			++out;

			grouplength = 0;
			if (short_address[i + 1] == ':') {
				++i;
				break;
			}
			continue;
		}
		else if ((c < '0' || c > '9') &&
			(c < 'a' || c > 'f'))
		{
			// Invalid character
			return std::wstring();
		}
		// Too long group
		if (++grouplength > 4) {
			return std::wstring();
		}
	}

	// Second half after ::

	wchar_t* end_first = out;
	out = &buffer[38];
	unsigned int stop = i;
	for (i = len - 1; i > stop; --i) {
		if (out < end_first) {
			// Too long
			return std::wstring();
		}

		wchar_t const& c = short_address[i];
		if (c == ':') {
			if (!grouplength) {
				// Empty group length, not valid
				return std::wstring();
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
		if (++grouplength > 4) {
			return std::wstring();
		}
		*out-- = c;
	}
	if (!grouplength && stop + 1 < len) {
		// Empty group length, not valid
		return std::wstring();
	}
	out -= 5 - grouplength;
	out += 2;

	int diff = out - end_first;
	if (diff < 0 || diff % 5) {
		return std::wstring();
	}

	return buffer;
}

bool IsRoutableAddress(const CStdString& address)
{
	if (address.Find(_T(":")) != -1) {
		std::wstring long_address = GetIPV6LongForm(address.GetString());
		if (long_address.empty()) {
			return false;
		}
		if (long_address[0] == '0') {
			// ::/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0000")) {
				return false;
			}
			// ::1/128
			if (long_address == _T("0000:0000:0000:0000:0000:0000:0000:0001")) {
				return false;
			}

			if (long_address.substr(0, 30) == _T("0000:0000:0000:0000:0000:ffff:")) {
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
		if (long_address[0] == 'f') {
			if (long_address[1] == 'e') {
				// fe80::/10 (link local)
				const TCHAR& c = long_address[2];
				int v;
				if (c >= 'a') {
					v = c - 'a' + 10;
				}
				else {
					v = c - '0';
				}
				if ((v & 0xc) == 0x8) {
					return false;
				}

				return true;
			}
			else if (long_address[1] == 'c' || long_address[1] == 'd') {
				// fc00::/7 (site local)
				return false;
			}
		}

		return true;
	}
	else {
		// Assumes address is already a valid IP address
		if (address.Left(3) == _T("127") ||
			address.Left(3) == _T("10.") ||
			address.Left(7) == _T("192.168") ||
			address.Left(7) == _T("169.254"))
			return false;

		if (address.Left(3) == _T("172")) {
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

bool IsIpAddress(std::wstring const& address, bool allowNull)
{
	if (!GetIPV6LongForm(address).empty()) {
		return true;
	}

	int segment = 0;
	int dotcount = 0;
	for (size_t i = 0; i < address.size(); ++i) {
		wchar_t const& c = address[i];
		if (c == '.') {
			if (address[i + 1] == '.') {
				// Disallow multiple dots in a row
				return false;
			}

			if (segment > 255) {
				return false;
			}
			if (!dotcount && !segment && !allowNull) {
				return false;
			}
			dotcount++;
			segment = 0;
		}
		else if (c < '0' || c > '9') {
			return false;
		}

		segment = segment * 10 + c - '0';
	}
	if (dotcount != 3) {
		return false;
	}

	if (segment > 255) {
		return false;
	}

	return true;
}

std::wstring GetIPV6ShortForm(std::wstring const& ip)
{
	// This could be optimized a lot.

	// First get the long form in a well-known format
	std::wstring l = GetIPV6LongForm(ip);
	if (l.empty()) {
		return std::wstring();
	}

	wchar_t const* p = l.c_str();

	wchar_t outbuf[42];
	*outbuf = ':';
	wchar_t* out = outbuf + 1;

	bool segmentStart = true;
	bool readLeadingZero = false;
	while (*p)
	{
		switch (*p)
		{
		case ':':
			if (readLeadingZero) {
				*(out++) = '0';
			}
			*out++ = ':';
			readLeadingZero = false;
			segmentStart = true;
			break;
		case '0':
			if (segmentStart) {
				readLeadingZero = true;
			}
			else {
				*out++ = '0';
				readLeadingZero = false;
			}
			break;
		default:
			readLeadingZero = false;
			segmentStart = false;
			*out++ = *p;
			break;
		}

		++p;
	}
	*(out++) = ':';
	*out = 0;

	// Replace longest run of consecutive zeroes
	std::wstring shortIp(outbuf);

	std::wstring s = L":0:0:0:0:0:0:0:0:";
	while (s.size() > 2) {
		size_t pos = shortIp.find(s);
		if (pos != std::wstring::npos) {
			shortIp = shortIp.substr(0, pos + 1) + shortIp.substr(pos + s.size() - 1);
			break;
		}

		s = s.substr(2);
	}
	fz::replace_substrings(shortIp, L":::", L"::");
	if (shortIp[0] == ':' && shortIp[1] != ':') {
		shortIp = shortIp.substr(1);
	}
	if (shortIp.size() >= 2 && shortIp[shortIp.size() - 1] == ':' && shortIp[shortIp.size() - 2] != ':') {
		shortIp = shortIp.substr(0, shortIp.size() - 1);
	}

	return shortIp;
}

bool foo = IsBehindIPv4Nat();

bool IsBehindIPv4Nat()
{

	ULONG len = 0;
	int ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_FRIENDLY_NAME, 0, 0, &len);
	if (ret != ERROR_BUFFER_OVERFLOW) {
		return false;
	}

	bool has_ipv4 = false;
	bool has_public_ipv4 = false;

	auto buf = std::unique_ptr<char[]>(new char[len]);
	IP_ADAPTER_ADDRESSES *addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
	ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_FRIENDLY_NAME, 0, addrs, &len);
	if (ret == ERROR_SUCCESS) {
		for (auto addr = addrs; addr; addr = addr->Next) {
			for (auto ip = addr->FirstUnicastAddress; ip; ip = ip->Next) {
				if (ip->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) {
					continue;
				}

				if (ip->Address.lpSockaddr && ip->Address.lpSockaddr->sa_family == AF_INET) {
					has_ipv4 = true;

					CStdString ipStr = inet_ntoa(reinterpret_cast<SOCKADDR_IN*>(ip->Address.lpSockaddr)->sin_addr);
					if (!ipStr.IsEmpty()) {
						has_public_ipv4 = has_public_ipv4 || IsRoutableAddress(ipStr);
					}
				}
			}
		}
	}

	return has_ipv4 && !has_public_ipv4;
}