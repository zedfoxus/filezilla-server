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

#ifndef FZS_OPTION_TYPES_INCLUDED
#define FZS_OPTION_TYPES_INCLUDED

#define OPTION_SERVERPORT 1
#define OPTION_THREADNUM 2
#define OPTION_MAXUSERS 3
#define OPTION_TIMEOUT 4
#define OPTION_NOTRANSFERTIMEOUT 5
#define OPTION_CHECK_DATA_CONNECTION_IP 6
#define OPTION_SERVICE_NAME 7
#define OPTION_SERVICE_DISPLAY_NAME 8
#define OPTION_TLS_REQUIRE_SESSION_RESUMPTION 9
#define OPTION_LOGINTIMEOUT 10
#define OPTION_LOGSHOWPASS 11
#define OPTION_CUSTOMPASVIPTYPE 12
#define OPTION_CUSTOMPASVIP 13
#define OPTION_CUSTOMPASVMINPORT 14
#define OPTION_CUSTOMPASVMAXPORT 15
#define OPTION_WELCOMEMESSAGE 16
#define OPTION_ADMINPORT 17
#define OPTION_ADMINPASS 18
#define OPTION_ADMINIPBINDINGS 19
#define OPTION_ADMINIPADDRESSES 20
#define OPTION_ENABLELOGGING 21
#define OPTION_LOGLIMITSIZE 22
#define OPTION_LOGTYPE 23
#define OPTION_LOGDELETETIME 24
#define OPTION_DISABLE_IPV6 25
#define OPTION_ENABLE_HASH 26
#define OPTION_DOWNLOADSPEEDLIMITTYPE 27
#define OPTION_UPLOADSPEEDLIMITTYPE 28
#define OPTION_DOWNLOADSPEEDLIMIT 29
#define OPTION_UPLOADSPEEDLIMIT 30
#define OPTION_BUFFERSIZE 31
#define OPTION_CUSTOMPASVIPSERVER 32
#define OPTION_USECUSTOMPASVPORT 33
#define OPTION_MODEZ_USE 34
#define OPTION_MODEZ_LEVELMIN 35
#define OPTION_MODEZ_LEVELMAX 36
#define OPTION_MODEZ_ALLOWLOCAL 37
#define OPTION_MODEZ_DISALLOWED_IPS 38
#define OPTION_IPBINDINGS 39
#define OPTION_IPFILTER_ALLOWED 40
#define OPTION_IPFILTER_DISALLOWED 41
#define OPTION_WELCOMEMESSAGE_HIDE 42
#define OPTION_ENABLETLS 43
#define OPTION_ALLOWEXPLICITTLS 44
#define OPTION_TLSKEYFILE 45
#define OPTION_TLSCERTFILE 46
#define OPTION_TLSPORTS 47
#define OPTION_TLSFORCEEXPLICIT 48
#define OPTION_BUFFERSIZE2 49
#define OPTION_FORCEPROTP 50
#define OPTION_TLSKEYPASS 51
#define OPTION_SHAREDWRITE 52
#define OPTION_NOEXTERNALIPONLOCAL 53
#define OPTION_ACTIVE_IGNORELOCAL 54
#define OPTION_AUTOBAN_ENABLE 55
#define OPTION_AUTOBAN_ATTEMPTS 56
#define OPTION_AUTOBAN_TYPE 57
#define OPTION_AUTOBAN_BANTIME 58
#define OPTION_TLS_MINVERSION 59

#define OPTIONS_NUM 59

#define CONST_WELCOMEMESSAGE_LINESIZE 75

struct t_Option
{
	char const name[30];
	int nType;
	bool bOnlyLocal; //If TRUE, setting can only be changed from local connections
};

const DWORD SERVER_VERSION = 0x00096000;
const DWORD PROTOCOL_VERSION = 0x00014000;

//												Name					Type		Not remotely
//																(0=str, 1=numeric)   changeable
static const t_Option m_Options[OPTIONS_NUM]={	"Serverports",				0,	false,
												"Number of Threads",		1,	false,
												"Maximum user count",		1,	false,
												"Timeout",					1,	false,
												"No Transfer Timeout",		1,	false,
												"Check data connection IP",	1,	false,
												"Service name",				0,	true,
												"Service display name",		0,	true,
												"Force TLS session resumption", 1, false,
												"Login Timeout",			1,	false,
												"Show Pass in Log",			1,	false,
												"Custom PASV IP type",		1,	false,
												"Custom PASV IP",			0,	false,
												"Custom PASV min port",		1,	false,
												"Custom PASV max port",		1,	false,
												"Initial Welcome Message",	0,	false,
												"Admin port",				1,	true,
												"Admin Password",			0,	true,
												"Admin IP Bindings",		0,	true,
												"Admin IP Addresses",		0,	true,
												"Enable logging",			1,	false,
												"Logsize limit",			1,	false,
												"Logfile type",				1,	false,
												"Logfile delete time",		1,	false,
												"Disable IPv6",				1,  false,
												"Enable HASH",				1,  false,
												"Download Speedlimit Type",	1,	false,
												"Upload Speedlimit Type",	1,	false,
												"Download Speedlimit",		1,	false,
												"Upload Speedlimit",		1,	false,
												"Buffer Size",				1,	false,
												"Custom PASV IP server",	0,	false,
												"Use custom PASV ports",	1,	false,
												"Mode Z Use",				1,	false,
												"Mode Z min level",			1,	false,
												"Mode Z max level",			1,	false,
												"Mode Z allow local",		1,	false,
												"Mode Z disallowed IPs",	0,	false,
												"IP Bindings",				0,	false,
												"IP Filter Allowed",		0,	false,
												"IP Filter Disallowed",		0,	false,
												"Hide Welcome Message",		1,	false,
												"Enable SSL",				1,	false,
												"Allow explicit SSL",		1,	false,
												"SSL Key file",				0,	false,
												"SSL Certificate file",		0,	false,
												"Implicit SSL ports",		0,	false,
												"Force explicit SSL",		1,	false,
												"Network Buffer Size",		1,	false,
												"Force PROT P",				1,	false,
												"SSL Key Password",			0,	false,
												"Allow shared write",		1,	false,
												"No External IP On Local",	1,	false,
												"Active ignore local",		1,	false,
												"Autoban enable",			1,	false,
												"Autoban attempts",			1,	false,
												"Autoban type",				1,	false,
												"Autoban time",				1,	false,
												"Minimum TLS version",		1,	false
											};

#endif
