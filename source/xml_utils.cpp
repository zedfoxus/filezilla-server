#include "StdAfx.h"
#include "xml_utils.h"
#include "conversion.h"
#include "tinyxml/tinyxml.h"

#include <libfilezilla/string.hpp>

namespace XML
{

std::wstring ReadText(TiXmlElement* pElement)
{
	TiXmlNode* textNode = pElement->FirstChild();
	if (!textNode || !textNode->ToText()) {
		return std::wstring();
	}

	return ConvFromNetwork(textNode->Value());
}

void SetText(TiXmlElement* pElement, std::wstring const& text)
{
	pElement->Clear();
	pElement->LinkEndChild(new TiXmlText(fz::to_utf8(text).c_str()));
}

bool Load(TiXmlDocument & document, CStdString const& file)
{
	bool ret = false;

	FILE* f = _wfopen(file, L"rb");
	if (f) {
		ret = document.LoadFile(f);

		fclose(f);
	}

	return ret;
}

bool Save(TiXmlNode & node, CStdString const& file)
{
	bool ret = false;

	auto * document = node.GetDocument();
	if (document) {
		FILE* f = _wfopen(file, L"wb");
		if (f) {
			ret = document->SaveFile(f);
		}

		fclose(f);
	}

	return ret;
}

}