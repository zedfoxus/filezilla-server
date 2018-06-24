#ifndef __XML_UTILS_H__
#define __XML_UTILS_H__

class TiXmlDocument;
class TiXmlElement;
class TiXmlNode;

namespace XML
{

std::wstring ReadText(TiXmlElement* pElement);
void SetText(TiXmlElement* pElement, std::wstring const& text);

bool Load(TiXmlDocument & document, CStdString const& file);
bool Save(TiXmlNode & element, CStdString const& file);

}

#endif