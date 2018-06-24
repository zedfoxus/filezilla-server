#ifndef FILEZILLA_SERVER_SERVICE_ACCOUNTS_HEADER
#define FILEZILLA_SERVER_SERVICE_ACCOUNTS_HEADER

#include "SpeedLimit.h"

class t_directory
{
public:
	std::wstring dir;
	std::vector<std::wstring> aliases;
	bool bFileRead{}, bFileWrite{}, bFileDelete{}, bFileAppend{};
	bool bDirCreate{}, bDirDelete{}, bDirList{}, bDirSubdirs{}, bIsHome{};
	bool bAutoCreate{};
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
	virtual ~t_group() = default;

	virtual int GetRequiredBufferLen() const;
	virtual unsigned char * FillBuffer(unsigned char *p) const;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length);
	
	int GetRequiredStringBufferLen(std::string const& str) const;
	int GetRequiredStringBufferLen(std::wstring const& str) const;
	
	void FillString(unsigned char *&p, std::string const& str) const;
	void FillString(unsigned char *&p, std::wstring const& str) const;
	
	bool BypassUserLimit() const;
	int GetUserLimit() const;
	int GetIpLimit() const;
	bool IsEnabled() const;
	bool ForceSsl() const;

	int GetCurrentSpeedLimit(sltype type) const;
	bool BypassServerSpeedLimit(sltype type) const;

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

protected:
	bool ParseString(unsigned char const* endMarker, unsigned char *&p, std::string &string);
	bool ParseString(unsigned char const* endMarker, unsigned char *&p, std::wstring &string);
};

class t_user : public t_group
{
public:
	t_user() = default;

	virtual int GetRequiredBufferLen() const override;
	virtual unsigned char * FillBuffer(unsigned char *p) const override;
	virtual unsigned char * ParseBuffer(unsigned char *pBuffer, int length)  override;
	void generateSalt(); // Generates a new random salt of length 64, using all printable ASCII characters.

	std::wstring user;
	std::wstring password;
	std::string salt;
};

#endif
