#pragma once
#include <string>
#include <vector>
#include <stack>
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
		bool ReadUntil(const unsigned char* tail, int nLen,
			const std::function<void(const unsigned char* data, int nLen,bool bEndPart)>& cb);
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
	class RESPSocketClient:public TinySocketClient
	{
	public:
		RESPSocketClient(const char* addres, int port);
		bool SendError(const char* error, int nLen = -1);
		bool SendSimpleString(const char* str, int nLen = -1);
		bool SendSimpleString(const std::string& str);

		bool SendInteger(long long nInt, RESPCommand cmd = RESPCommand::eInteger);
		
		bool SendBulkString(const std::string& data);
		bool SendBulkString(const unsigned char* data, int nLen);
		bool SendBulkString(const  char* data, int nLen);
		bool SendArray(int nCount);
		bool SendLine(RESPCommand cmd, const unsigned char* data, int nLen);
	};
	//RESP (REdis Serialization Protocol)解析器
	class RESPParser
	{
		long long ParseInteger(TinySocketClient* socket);
		bool ParseLine(TinySocketClient* client);
		bool ParseFixLength(TinySocketClient* client);
		bool ParseArray(TinySocketClient* client);
	protected:
		RESPParser() {}
		virtual bool OnBegin(RESPCommand cmd,int nArrayLen = 0) = 0;
		virtual bool OnFinish(RESPCommand cmd) = 0;
		virtual unsigned char* OnFixLengthContent(int nLen) = 0;
		virtual bool OnContentPart(const unsigned char* data, int nPartLen, bool bEndPart) = 0;

	public:
		virtual ~RESPParser() {}
		
		virtual bool Parse(TinySocketClient* client);
	};
	class Reply
	{
	public:

		RESPCommand			Type = RESPCommand::eEmpty;
		std::string			Content;
		std::vector< Reply>	Children;

	public:
		Reply(RESPCommand eType = RESPCommand::eEmpty);
		Reply(const Reply& r);
		Reply(Reply&& r);
		operator bool()const;
		bool IsInteger()const;
		long long Integer(bool bForce = false)const;
		Reply& operator = (const Reply& r);
		Reply& operator = (Reply&& r);
		Reply& Swap(Reply& r);
		void Reset();

	};
	class ReplyParser :public Reply, public RESPParser
	{
		virtual bool OnBegin(RESPCommand cmd, int nArrayLen);
		virtual bool OnFinish(RESPCommand cmd);
		virtual unsigned char* OnFixLengthContent(int nLen);
		virtual bool OnContentPart(const unsigned char* data, int nPartLen, bool bLastPart);
		std::stack<Reply*> m_Recent;
	public:
		ReplyParser();
		ReplyParser(TinySocketClient* socket);
	};

	class ScanCursor:public ReplyParser
	{
	public:
		ScanCursor();
		//游标值
		int Cursor()const;
		//是否为最后一个游标
		bool IsFinished()const;

		//游标中Key的数量
		int Count()const;

		//根据索引获取Key的值
		const Reply* Key(int n)const;
	};

	class ScriptEval :public ReplyParser
	{
		RESPSocketClient* m_client;
	public:
		ScriptEval(RESPSocketClient* client, const char* script, int numKeys = 0,int nNumArgs = 0);
		bool SendKey(const char* key);
		bool SendKey(const unsigned char* key,int nLen);
		bool SendKey(const std::string& key);

		bool SendArg(const char* key);
		bool SendArg(const unsigned char* key, int nLen);
		bool SendArg(const std::string& key);
		bool Execute();

	};
	class TinyRedisClient :public RESPSocketClient
	{
	public:
		TinyRedisClient(const char* addres, int port);
		~TinyRedisClient();
		
		//http://redisdoc.com/database/select.html
		bool Select(int i);

		//http://redisdoc.com/string/set.html
		bool Set(const std::string& key, std::string& value);
		bool Set(const char* key, const char* value);
		bool Set(const unsigned char* key, int nKeyLen, const unsigned char* value, int nValueLen);
		//http://redisdoc.com/database/del.html
		bool Del(const unsigned char* key, int nKeyLen);
		bool Del(const char* key);
		bool Del(const std::string& key);
		//http://redisdoc.com/database/exists.html
		bool Exists(const unsigned char* key, int nKeyLen);
		bool Exists(const char* key);
		bool Exists(const std::string& key);

		//http://redisdoc.com/string/get.html
		bool Get(const std::string& key, RESPParser* result);
		bool Get(const char* key, RESPParser* result);
		bool Get(const unsigned char* key,int nKeyLen, RESPParser* result);

		//http://redisdoc.com/database/scan.html
		bool Scan(int cursor, RESPParser* result, const std::string& pattern, int nCount = -1);
		bool Scan(int cursor, RESPParser* result, const char* pattern = NULL, int nCount  = -1);
		bool Scan(int cursor, RESPParser* result, const unsigned char* pattern = NULL,int nPatternLen =-1, int nCount = -1);

		//http://redisdoc.com/database/flushdb.html
		bool FlushDB();

		//使用Lua脚本批量删除
		bool BatchDel(const std::string& pattern);
		bool BatchDel(const char* pattern);
		bool BatchDel(const unsigned char* pattern,int nLen);

	};

}