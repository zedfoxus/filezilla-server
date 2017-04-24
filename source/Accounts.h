#ifndef ACCOUNTS_H_INCLUDED
#define ACCOUNTS_H_INCLUDED

#include "SpeedLimit.h"

class t_directory
{
public:
	std::wstring dir;
	std::vector<std::wstring> aliases;
	BOOL bFileRead{}, bFileWrite{}, bFileDelete{}, bFileAppend{};
	BOOL bDirCreate{}, bDirDelete{}, bDirList{}, bDirSubdirs{}, bIsHome{};
	BOOL bAutoCreate{};
};

enum sltype
{
	download = 0,
	upload = 1
};

class t_group
{
public:
	t_group();
	virtual ~t_group() {}

	virtual int GetRequiredBufferLen() const;
	virtual int GetRequiredStringBufferLen(std::wstring const& str) const;
	virtual unsigned char * FillBuffer(unsigned char *p) const;
	virtual void FillString(unsigned char *&p, std::wstring const& str) const;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length);

	virtual bool BypassUserLimit() const;
	virtual int GetUserLimit() const;
	virtual int GetIpLimit() const;
	virtual bool IsEnabled() const;
	virtual bool ForceSsl() const;

	virtual int GetCurrentSpeedLimit(sltype type) const;
	virtual bool BypassServerSpeedLimit(sltype type) const;

	bool AccessAllowed(std::wstring const& ip) const;

	std::wstring group;
	std::vector<t_directory> permissions;
	int nBypassUserLimit{};
	int nUserLimit{}, nIpLimit{};
	int nEnabled{};
	int forceSsl{};

	int nSpeedLimitType[2];
	int nSpeedLimit[2];
	SPEEDLIMITSLIST SpeedLimits[2];
	int nBypassServerSpeedLimit[2];

	std::vector<std::wstring> allowedIPs, disallowedIPs;

	std::wstring comment;

	t_group const* pOwner{};

	bool b8plus3{};

protected:
	bool ParseString(unsigned char const* endMarker, unsigned char *&p, std::wstring &string);
};

class t_user : public t_group
{
public:
	t_user();

	virtual int GetRequiredBufferLen() const;
	virtual unsigned char * FillBuffer(unsigned char *p) const;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length);
	void generateSalt(); // Generates a new random salt of length 64, using all printable ASCII characters.

	std::wstring user;
	std::wstring password;
	std::wstring salt;
};

#endif //#define ACCOUNTS_H_INCLUDED
