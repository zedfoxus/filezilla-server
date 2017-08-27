#ifndef FILEZILLA_SERVER_HASHTRHEAD_HEADER
#define FILEZILLA_SERVER_HASHTRHEAD_HEADER

#include <libfilezilla/mutex.hpp>

class CServerThread;
class CHashThread final
{
public:
	enum _result
	{
		OK = 0,
		PENDING = 1,
		BUSY = 2,
		FAILURE_OPEN = 4,
		FAILURE_READ = 8,
		FAILURE_MASK = 12
	};

	enum _algorithm
	{
		MD5,
		SHA1,
		SHA512
	};

	CHashThread();
	~CHashThread();

	enum _result Hash(std::wstring const& filename, enum _algorithm algorithm, int& id, CServerThread* server_thread);

	enum _result GetResult(int id, CHashThread::_algorithm& alg, CStdString& hash, CStdString& file);

	void Stop(CServerThread* server_thread);

private:
	void DoHash(fz::scoped_lock & lock);
	void Loop();

	static DWORD WINAPI ThreadFunc(LPVOID pThis);

	std::wstring filename_;
	CServerThread* m_server_thread{};

	fz::mutex mutex_{false};
	fz::condition cond_;

	bool m_quit{};

	int m_id{};
	int m_active_id{};
	enum _result m_result;
	char* m_hash{};
	enum _algorithm m_algorithm;

	HANDLE m_hThread;
};

#endif
