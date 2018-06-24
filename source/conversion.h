#ifndef FILEZILLA_SERVER_SERVICE_CONVERSION_HEADER
#define FILEZILLA_SERVER_SERVICE_CONVERSION_HEADER

#include <string_view>

std::wstring ConvFromNetwork(std::string_view const& str);
std::string ConvToLocal(std::wstring_view const& str);
std::wstring ConvFromLocal(std::string_view const& str);

#endif
