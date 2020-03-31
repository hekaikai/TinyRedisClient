#include "trc.h"
#include <vector>
#include <thread>
#include <stack>
#ifdef _WIN32
#define  strcasecmp _stricmp
#endif

using namespace TRC;

//http://c.biancheng.net/socket/
#ifdef _WIN32
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
#else

#endif
static const char* g_strCRLF = "\r\n";
int  socket_write(socket_t s, const void* buf, size_t nbytes)
{
#ifdef _WIN32
    return send(s, (const char*)buf, nbytes, 0);
#else
    return write(s, buf, nbytes);
#endif
}
int sockt_read(socket_t  s, void* buf, size_t nbytes)
{
#ifdef _WIN32
    return recv(s, (char*)buf, nbytes, 0);
#else
    return read(s, buf, nbytes);
#endif
}
bool socket_close(socket_t s)
{
#ifdef _WIN32
    return closesocket(s) == 0;
#else
    return close(s) == 0;
#endif
}


TinySocketClient::TinySocketClient(const char* addres, int port)
{
    m_Socket = INVALID_SOCKET;
	m_Address = addres;
	m_Port = port; 
}
TinySocketClient::~TinySocketClient()
{
    Close();
}

void TinySocketClient::Close()
{
    if (m_Socket == INVALID_SOCKET)
        return ;
    socket_close(m_Socket);
    m_Socket = INVALID_SOCKET;
}
bool TinySocketClient::CheckError(bool result)
{
    if(!result)
        Close();
    return result;
}
int TinySocketClient::CheckError(int n)
{
    if (n < 0)
        Close();
    return n;
}
bool TinySocketClient::Good()const
{
    return m_Socket != INVALID_SOCKET;
}
//连接
bool TinySocketClient::Connect()
{
    if (m_Socket != INVALID_SOCKET)
        Close();

    //创建套接字
    m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == m_Socket)
        return false;

    //向服务器发起请求
    struct sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(m_Address.c_str());
    sockAddr.sin_port = htons(m_Port);
    int n = connect(m_Socket, (struct sockaddr*) & sockAddr, sizeof(sockAddr));
    if (n != 0)
        return false;
    return true;

}
int TinySocketClient::Send(const unsigned char* buf, int n)
{
    if (!Good())
        return 0;
    return CheckError(socket_write(m_Socket, buf, n));
}
bool SelectRead(socket_t fd,int timeout)
{
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    return select(fd + 1, &fds, NULL, NULL, &tv) >0;
}
template<const int SIZE=8192>
struct TempBuffer
{
    unsigned char Buffer[SIZE];
    int Offset = 0;
    const unsigned char& Current()const
    {
        return Buffer[Offset-1];
    }
    unsigned char& AllocByte()
    {
        return Buffer[Offset++];
    }
    bool IsFull()const
    {
        return Offset >= SIZE;
    }
};
//读取数据直到读取到特定长度的尾
bool TinySocketClient::ReadUntil(const unsigned char* tail, int nLen,
    const std::function<void(const unsigned char* data, int nLen, bool bEndPart)>& cb)
{
    TempBuffer<> tempBuff;

    int nLeft = nLen;
    const unsigned char* it = tail;
    while (nLeft > 0)
    {
        if (!Read(&tempBuff.AllocByte(), 1))
            return false;
        if (it[0] != tempBuff.Current())
        {
            it = tail;
            nLeft = nLen;
        }
        else
        {
            it++;
            nLeft--;
        }
        if (tempBuff.IsFull())
        {
            cb(tempBuff.Buffer, tempBuff.Offset, nLeft ==0);
            tempBuff.Offset = 0;
        }
    }
    if(tempBuff.Offset >0)
        cb(tempBuff.Buffer, tempBuff.Offset, nLeft == 0);
    return true;
}
bool TinySocketClient::Readable()const
{
    return SelectRead(m_Socket, 1);
}
//读取特定长度的数据
bool TinySocketClient::Read(unsigned char* buffer, int nLen)
{
    while (nLen > 0)
    {
        if (!CheckError(SelectRead(m_Socket, m_ReadOvertime)))
            return false;
        int n = sockt_read(m_Socket, buffer, nLen);
        buffer += n;
        nLen -= n;
    }  
    
    return true;
} 


RESPSocketClient::RESPSocketClient(const char* addres, int port) :TinySocketClient(addres, port)
{

}
bool RESPSocketClient::SendLine(RESPCommand cmd, const unsigned char* data, int nLen)
{
    if (Send((const unsigned char*)&cmd, 1) != 1)
        return false;
    if (nLen > 0)
    {
        if (Send(data, nLen) != nLen)
            return false;
    }

    return  Send((const unsigned char*)g_strCRLF, 2) == 2;
}
bool RESPSocketClient::SendError(const char* error, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (error)
            nLen = strlen(error);
    }
    return SendLine(RESPCommand::eError, (const unsigned char*)error, nLen);
}
bool RESPSocketClient::SendSimpleString(const std::string& str)
{
    return SendSimpleString(str.data(), str.size());
}
bool RESPSocketClient::SendSimpleString(const char* str, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (str)
            nLen = strlen(str);
    }
    return SendLine(RESPCommand::eSimpleString, (const unsigned char*)str, nLen);
}
bool RESPSocketClient::SendInteger(long long nInt, RESPCommand cmd)
{
    std::string str = std::to_string(nInt);
    return SendLine(cmd, (const unsigned char*)str.data(), str.size());
}
bool RESPSocketClient::SendBulkString(const  char* data, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (data)
            nLen = strlen(data);
    }
    return SendBulkString((const unsigned char*)data, nLen);
}
bool RESPSocketClient::SendBulkString(const std::string& data)
{
    return SendBulkString((const unsigned char*)data.data(), data.size());
}
bool RESPSocketClient::SendBulkString(const unsigned char* data, int nLen)
{
    if (!SendInteger(nLen, RESPCommand::eBulkString))
        return false;
    if (nLen > 0)
    {
        if (Send(data, nLen) <= 0)
            return false;
    }
    return  Send((const unsigned char*)g_strCRLF, 2) == 2;
}
bool RESPSocketClient::SendArray(int nCount)
{
    return SendInteger(nCount, RESPCommand::eArray);
}


long long RESPParser::ParseInteger(TinySocketClient* socket)
{
    std::string strContent;
    if (!socket->ReadUntil((const unsigned char*)g_strCRLF, 2, [&](const unsigned char* data, int nLen, bool bEndPart) {
        if (bEndPart)
            nLen++;
        strContent.insert(strContent.end(), data, data + nLen);
        }))
    {
        return 0;
    }
        if (strContent.empty())
            return 0;
    return std::stoll(strContent);
}

bool RESPParser::ParseLine(TinySocketClient* socket)
{
    return socket->ReadUntil((const unsigned char*)g_strCRLF, 2, [&](const unsigned char* data, int nLen, bool bLast) {
        //如果是最后一块数据则剔除末尾的\r\n
        if (bLast)
            nLen -= 2;
        OnContentPart(data, nLen, bLast);
        });
}

bool RESPParser::ParseFixLength(TinySocketClient* socket)
{
    //转换字符串的长度为整数
    long long nLen = ParseInteger(socket);
    
    //获取固定长度内容的指针
    auto pointer = OnFixLengthContent(nLen);
    if (!pointer)
        return false;

    if (!socket->Read(pointer, nLen))
        return false;

    //将\r\n分割符读取后丢弃
    unsigned char buff[2];
    return socket->Read(buff, 2);
}
bool RESPParser::ParseArray(TinySocketClient* socket)
{ 
//转换字符串的长度为整数
    long long nLen = ParseInteger(socket);
    OnBegin(RESPCommand::eArray, nLen);
    for (int i = 0; i < nLen; i++)
    {
        if (!Parse(socket))
            return false;
    }
    return true;
}
bool RESPParser::Parse(TinySocketClient* socket)
{
    RESPCommand cmd = socket->ReadT<RESPCommand>();
    bool bOk = false;

    //对于数组而言只有在解析了数组的数量之后才调用OnBegin，这样便于预先准备好存储空间。
    if (cmd == RESPCommand::eArray)
        bOk = ParseArray(socket);
    else
    {
        OnBegin(cmd);
        if (cmd == RESPCommand::eError)
            bOk = ParseLine(socket);
        else if (cmd == RESPCommand::eSimpleString)
            bOk = ParseLine(socket);
        else if (cmd == RESPCommand::eInteger)
            bOk = ParseLine(socket);
        else if (cmd == RESPCommand::eBulkString)
            bOk = ParseFixLength(socket);
    }
    if (bOk)
        OnFinish(cmd);
    return bOk;
}
bool ReplyParser::OnBegin(RESPCommand cmd, int nArrayLen)
{
    if (m_Recent.empty())
    {
        if (cmd == RESPCommand::eArray && nArrayLen > 0)
            Children.reserve(nArrayLen);
        Type = cmd;
        m_Recent.push(this);
        return true;
    }
    auto top = m_Recent.top();
    top->Children.emplace_back(cmd);
    auto &last = m_Recent.top()->Children.back();
    if (cmd == RESPCommand::eArray && nArrayLen > 0)
        last.Children.reserve(nArrayLen);

    m_Recent.push(&last);
    return true;
}
bool ReplyParser::OnFinish(RESPCommand cmd)
{
    if (m_Recent.empty())
        return false;
    m_Recent.pop();
    return true;
}
unsigned char* ReplyParser::OnFixLengthContent(int nLen)
{
    auto top = m_Recent.top();
    if (nLen > 0)
    {
        top->Content.resize(nLen);
        if (top->Content.size() != nLen)
            return NULL;
    }
    else
    {
        top->Content.clear();
        return NULL;
    }

    return (unsigned char* )top->Content.data();
}
bool ReplyParser::OnContentPart(const unsigned char* data, int nPartLen, bool bLastPart)
{
    auto top = m_Recent.top();
    top->Content.insert(top->Content.end(), data, data + nPartLen);
    return true;
}
ReplyParser::ReplyParser()
{
}
ReplyParser::ReplyParser(TinySocketClient* socket)
{
    Parse(socket);
}

ScanCursor::ScanCursor()
{

}
//游标值
int ScanCursor::Cursor()const 
{
    if (Children.empty())
        return 0;

    return Children.front().Integer();
}
//是否为最后一个游标
bool ScanCursor::IsFinished()const
{
    return Cursor() == 0;
}

//游标中Key的数量
int ScanCursor::Count()const
{
    if (Children.size() < 2)
        return 0;
    return Children.back().Children.size();
}

//根据索引获取Key的值
const Reply* ScanCursor::Key(int n)const
{
    if (n < 0 || n >= Count())
        return NULL;
    return &Children.back().Children.at(n);
}

Reply::Reply(RESPCommand eType )
{
    Type = eType;
} 
Reply::Reply(const Reply& r)
{
    operator=(r);
}
Reply::Reply(Reply&& r)
{
    Swap(r);
}
bool Reply::IsInteger()const
{
    return (Type == RESPCommand::eInteger);
}
long long Reply::Integer(bool bForce )const
{
    if (bForce && Type != RESPCommand::eInteger)
        return 0;
    if (Content.empty())
        return 0;
    return std::stoll(Content);
}
Reply::operator bool()const
{
    return Type != RESPCommand::eEmpty;
}
Reply& Reply::operator = (const Reply& r)
{
    Type = r.Type;
    Content = r.Content;
    Children = r.Children;
    return *this;
}
Reply& Reply::operator = (Reply&& r)
{
    return Swap(r);
}
void Reply::Reset()
{
    Reply().Swap(*this);
}
Reply& Reply::Swap(Reply& r)
{
    std::swap(Type, r.Type);
    Content.swap(r.Content);
    Children.swap(r.Children);
    return *this;
}
ScriptEval::ScriptEval(RESPSocketClient* client, const char* script, int numKeys, int nNumArgs)
{
    m_client = client;
    
    m_client->SendArray(3 + numKeys + nNumArgs);
    m_client->SendBulkString("EVAL", 4);
    m_client->SendBulkString(script);
    m_client->SendBulkString(std::to_string(numKeys));
}
bool ScriptEval::SendKey(const char* key)
{
    return m_client->SendBulkString(key);
}
bool ScriptEval::SendKey(const unsigned char* key, int nLen)
{
    return m_client->SendBulkString(key,nLen);
}
bool ScriptEval::SendKey(const std::string& key)
{
    return m_client->SendBulkString(key);
}

bool ScriptEval::SendArg(const char* key)
{
    return m_client->SendBulkString(key);
}
bool ScriptEval::SendArg(const unsigned char* key, int nLen)
{
    return m_client->SendBulkString(key, nLen);
}
bool ScriptEval::SendArg(const std::string& key)
{
    return m_client->SendBulkString(key);
}
bool ScriptEval::Execute()
{
    return Parse(m_client);
}

TinyRedisClient::TinyRedisClient(const char* addres, int port):RESPSocketClient(addres,port)
{

}
TinyRedisClient::~TinyRedisClient()
{

}
bool TinyRedisClient::Set(const std::string& key, std::string& value)
{
    return Set((const unsigned char*)key.data(), key.size(),
        (const unsigned char*)value.data(), value.size());
}
bool TinyRedisClient::Set(const char* key, const char* value)
{
    return Set((const unsigned char*)key, key?strlen(key):0,
        (const unsigned char*)value, value?strlen(value):0);
}
bool TinyRedisClient::Set(const unsigned char* key, int nKeyLen, const unsigned char* value, int nValueLen)
{
    if (!SendArray(3)) return false;

    if (!SendBulkString("SET",3)) return false;
    if (!SendBulkString(key, nKeyLen)) return false;
    if (!SendBulkString(value, nValueLen)) return false;

    //获取结果
    ReplyParser ret(this);
    if (!ret)
        return false;
    
    if (ret.Type == RESPCommand::eSimpleString)
        return strcasecmp(ret.Content.c_str(), "OK") == 0;
    return false;
}
bool TinyRedisClient::Del(const std::string& key)
{
    return Del((const unsigned char*)key.data(), key.size());
}
bool TinyRedisClient::Del(const char* key)
{
    return Del((const unsigned char*)key, key ? strlen(key) : 0);
}
bool TinyRedisClient::Del(const unsigned char* key, int nKeyLen)
{
    if (!SendArray(2)) return false;

    if (!SendBulkString("DEL", 3)) return false;
    if (!SendBulkString(key, nKeyLen)) return false;

    //获取结果
    ReplyParser ret(this);
    if (!ret)
        return false;
    if (ret.IsInteger())
        return ret.Integer() == 1;
    return false;
}
bool TinyRedisClient::Exists(const unsigned char* key, int nKeyLen)
{
    if (!SendArray(2)) return false;

    if (!SendBulkString("EXISTS", 6)) return false;
    if (!SendBulkString(key, nKeyLen)) return false;

    //获取结果
    ReplyParser ret(this);
    if (!ret)
        return false;
    if (ret.IsInteger())
        return ret.Integer() == 1;
    return false;
}
bool TinyRedisClient::Exists(const std::string& key)
{
    return Exists((const unsigned char*)key.data(), key.size());
}
bool TinyRedisClient::Exists(const char* key)
{
    return Exists((const unsigned char*)key, key ? strlen(key) : 0);
}
bool TinyRedisClient::Get(const std::string& key, RESPParser* result)
{
    return Get((const unsigned char*)key.data(), key.size(), result);
}
bool TinyRedisClient::Get(const char* key, RESPParser* result)
{
    return Get((const unsigned char*)key, key?strlen(key):0, result);
}
bool TinyRedisClient::Get(const unsigned char* key, int nKeyLen, RESPParser* result)
{
    if (!SendArray(2)) return false;
    if (!SendBulkString("GET", 3)) return false;
    if (!SendBulkString(key, nKeyLen)) return false;
    return result->Parse(this);
}
bool TinyRedisClient::Scan(int cursor, RESPParser* result, const std::string& pattern, int nCount)
{
    return Scan(cursor, result, (const unsigned char*)pattern.data(), pattern.size());
}
bool TinyRedisClient::Scan(int cursor, RESPParser* result, const char* pattern, int nCount)
{
    return Scan(cursor, result, (const unsigned char*)pattern, pattern ? strlen(pattern) : 0, nCount);
}
bool TinyRedisClient::Scan(int cursor, RESPParser* result, const unsigned char* pattern, int nPatternLen, int nCount)
{
    int nSendCount = 2;
    if (pattern && nPatternLen > 0)
        nSendCount += 2;
    if (nCount > 0) nSendCount += 2;

    if (!SendArray(nSendCount)) return false;
    if (!SendBulkString("SCAN", 4)) return false;
    std::string str = std::to_string(cursor);
    if (!SendBulkString(str.data(), str.size())) return false;

    if (pattern && nPatternLen > 0)
    {
        if (!SendBulkString("MATCH", 5)) return false;
        if (!SendBulkString(pattern, nPatternLen)) return false;
    }
    if(nCount >0)
    {
        if (!SendBulkString("COUNT", 5)) return false;
        str = std::to_string(nCount);
        if (!SendBulkString(str.data(), str.size())) return false;
    }
    return result->Parse(this);
}

bool TinyRedisClient::FlushDB()
{
    if(!SendArray(1)) return false;
    if (!SendBulkString("FLUSHDB", 7)) return false;
    ReplyParser reply;
    return reply.Parse(this);
}
//使用Lua脚本批量删除
bool TinyRedisClient::BatchDel(const std::string& pattern)
{
    return BatchDel((const unsigned char*)pattern.data(), pattern.size());
}
bool TinyRedisClient::BatchDel(const char* pattern)
{
    return BatchDel((const unsigned char*)pattern, pattern?strlen(pattern):0);
}
bool TinyRedisClient::BatchDel(const unsigned char* pattern, int nLen)
{
    //如果没有给条件则删除全部的数据。
    if (!pattern || nLen <= 0)
        return FlushDB();

    static std::string script = "return redis.call('del',unpack(redis.call('keys',KEYS[1])))";
    ScriptEval del(this, script.c_str(), 1);
    del.SendKey(pattern, nLen);
    if (!del.Execute())
        return false;
    return true;
}


