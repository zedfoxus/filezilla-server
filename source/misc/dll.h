#ifndef FZS_DLL_HEADER
#define FZS_DLL_HEADER

#ifndef _AFX
#define CString CStdString
#endif

class DLL final
{
public:
	DLL();
	explicit DLL(CString const& s);
	~DLL();

	DLL(DLL const&) = delete;
	DLL& operator=(DLL const&) = delete;

	bool load(CString const& s);

	void clear();
	
	explicit operator bool() const { return hModule != 0; }

	HMODULE get() { return hModule; }
protected:
	HMODULE hModule{};
};

#endif