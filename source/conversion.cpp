#include "stdafx.h"
#include "conversion.h"

std::wstring ConvFromNetwork(std::string_view const& buffer)
{
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer.data(), buffer.size(), 0, 0);
	if (len) {
		std::wstring str;
		str.resize(len);
		len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer.data(), buffer.size(), str.data(), len);
		if (!len) {
			return std::wstring();
		}
		return str;
	}

	return ConvFromLocal(buffer);
}

std::string ConvToLocal(std::wstring_view const& str)
{
	int len = WideCharToMultiByte(CP_ACP, 0, str.data(), str.size(), 0, 0, 0, 0);
	if (!len) {
		return std::string();
	}

	std::string outStr;
	outStr.resize(len);
	char* output = outStr.data();
	if (!WideCharToMultiByte(CP_ACP, 0, str.data(), str.size(), output, len, 0, 0)) {
		return std::string();
	}
	return outStr;
}

std::wstring ConvFromLocal(std::string_view const& local)
{
	int len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, local.data(), local.size(), 0, 0);
	if (len) {
		std::wstring str;
		str.resize(len);
		len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, local.data(), local.size(), str.data(), len);
		if (!len) {
			return std::wstring();
		}
		return str;
	}

	return std::wstring();
}
