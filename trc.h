#pragma once
#include <string>
#include <vector>
#include <functional>
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET socket_t;
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
typedef int socket_t;
#endif
//Tiny Redis Client
namespace TRC
{
	class TinySocketClient
	{
		std::string m_Address;
		int m_Port;
		socket_t m_Socket = 0;
		//读超时，缺省100秒
		int m_ReadOvertime = 100000;
		int CheckError(int n);
		bool CheckError(bool result);
	public:
		TinySocketClient(const char* addres, int port);
		virtual ~TinySocketClient();
		void Close();
		//连接
		bool Connect();
		bool Good()const;
		int Send(const unsigned char* buf, int n);
		bool Readable()const;

		//读取特定长度的数据
		bool Read(unsigned char* buffer, int nLen);
		template<class T>
		int SendT(const T& val)
		{
			return Send((const unsigned char*)&val, sizeof(T));
		}

		template<class T>
		T ReadT()
		{
			T  v;
			Read((unsigned char*)&v, sizeof(T));
			return v;
		}

		//读取数据直到读取到特定长度的尾
		bool ReadTo(const unsigned char* tail, int nLen, 
			const std::function<void(const unsigned char* data, int nLen,bool bLastPart)>& cb);
	};

	enum class RESPCommand :char
	{
		eEmpty = 0,
		eError = '-',
		eSimpleString = '+',
		eInteger = ':',
		eBulkString = '$',
		eArray = '*',
	};
	
	//RESP (REdis Serialization Protocol)解析器
	class RESPParser
	{
		bool ParseLine(TinySocketClient* client);
		bool ParseFixLength(TinySocketClient* client);
		bool ParseArray(TinySocketClient* client);
	protected:
		RESPParser() {}
		virtual bool OnBegin(RESPCommand cmd) = 0;
		virtual bool OnFinish(RESPCommand cmd) = 0;
		virtual unsigned char* OnFixLengthContent(int nLen) = 0;
		virtual bool OnContentPart(const unsigned char* data, int nPartLen, bool bLastPart) = 0;

	public:
		virtual ~RESPParser() {}
		
		bool Parse(TinySocketClient* client);
	};

	class Reply
	{
	public:

		RESPCommand			Type = RESPCommand::eEmpty;
		std::string			Content;
		std::vector< Reply>	Children;

	public:
		Reply(RESPCommand eType = RESPCommand::eEmpty);
		Reply(TinySocketClient* socket);
		Reply(const Reply& r);
		Reply(Reply&& r);
		operator bool()const;
		bool IsInteger()const;
		long long Integer(bool bForce = false)const;
		Reply& operator = (const Reply& r);
		Reply& operator = (Reply&& r);
		Reply& Swap(Reply& r);
	};
	class TinyRedisClient :public TinySocketClient
	{
		bool SendError(const char* error, int nLen = -1);
		bool SendSimpleString(const char* str, int nLen = -1);
		bool SendInteger(long long nInt, RESPCommand cmd = RESPCommand::eInteger);
		bool SendBulkString(const unsigned char* data, int nLen);
		bool SendBulkString(const  char* data, int nLen);
		bool SendArray(int nCount);
		bool SendLine(RESPCommand cmd, const unsigned char* data, int nLen);

	public:
		TinyRedisClient(const char* addres, int port);
		~TinyRedisClient();

		bool Set(const char* key, const char* value);
		bool Set(const unsigned char* key, int nKeyLen, const unsigned char* value, int nValueLen);
		bool Erase(const unsigned char* key, int nKeyLen);
		bool Erase(const char* key);

		bool Exists(const unsigned char* key, int nKeyLen);
		bool Exists(const char* key);
	};


}