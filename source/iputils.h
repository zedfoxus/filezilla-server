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

#ifndef FZS_IPUTILS_HEADER
#define FZS_IPUTILS_HEADER

bool IsLocalhost(std::wstring const& ip);
bool IsValidAddressFilter(std::wstring& filter);
bool MatchesFilter(std::wstring const& filter, std::wstring ip);
bool IsIpAddress(std::wstring const& address, bool allowNull = false);

// Also verifies that it is a correct IPv6 address
std::wstring GetIPV6LongForm(std::wstring short_address);
std::wstring GetIPV6ShortForm(std::wstring const& ip);
bool IsRoutableAddress(std::string const& address);
bool IsRoutableAddress(std::wstring const& address);

bool ParseIPFilter(std::wstring const& in, std::vector<std::wstring>* output = 0);

// Checks if FZS is running behind an IPv4 NAT
// Simple heuristic: Returns true if at least one adapter has an
// IPv4 address and no IPv4 address is a public one
bool IsBehindIPv4Nat();

#endif
