#include "StdAfx.h"
#include "xml_utils.h"
#include "conversion.h"

#include <libfilezilla/local_filesys.hpp>

namespace XML
{

file Load(std::wstring const& fn)
{
	file ret;

	auto result = ret.document.load_file(fn.c_str());
	if (!result) {
		if (fz::local_filesys::get_size(fz::to_native(fn)) <= 0) {
			// Create new one
			ret.root = ret.document.append_child("FileZillaServer");
		}
		else {
			ret.document.reset();
		}
	}
	else {
		ret.root = ret.document.first_child();
		if (ret.root.name() != "FileZillaServer"s) {
			ret.root = pugi::xml_node();
			ret.document.reset();
		}
	}

	return ret;
}

bool Save(pugi::xml_document const& document, std::wstring const& fn)
{
	if (!document.first_child()) {
		return false;
	}
	return document.save_file(fn.c_str());
}
}
