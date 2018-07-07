#ifndef FILEZILLA_SERVER_XML_UTILS_HEADER
#define FILEZILLA_SERVER_XML_UTILS_HEADER

#include "pugixml/pugixml.hpp"

namespace XML
{

class file
{
public:
	pugi::xml_document document;
	pugi::xml_node root;

	explicit operator bool() const {
		return document.first_child();
	}
};

file Load(std::wstring const& fn);
bool Save(pugi::xml_document const& document, std::wstring const& fn);

}


#endif
