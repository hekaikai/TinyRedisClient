#include "trc.h"
#include <vector>
#include <thread>
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
bool TinySocketClient::ReadTo(const unsigned char* tail, int nLen, const std::function<void(const unsigned char* data, int nLen)>& cb)
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
            cb(tempBuff.Buffer, tempBuff.Offset);
            tempBuff.Offset = 0;
        }
    }
    if(tempBuff.Offset >0)
        cb(tempBuff.Buffer, tempBuff.Offset);
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


bool ReadLine(TinySocketClient* socket, Reply& reply)
{
    if (!socket->ReadTo((const unsigned char*)g_strCRLF, 2, [&](const unsigned char* data, int nLen) {
        reply.Content.insert(reply.Content.end(), data, data + nLen);
        }))
    {
        return false;
    }

    if (reply.Content.empty())
        return false;

    //将最后的\r\n删除掉
    reply.Content.pop_back();
    reply.Content.pop_back();
    return true;
}
bool ReadFixLength(TinySocketClient* socket, Reply& reply)
{
    //首先读一行获取长度
    if (!ReadLine(socket, reply))
        return false;
    
    //转换字符串的长度为整数
    long long nLen = reply.Integer();
    reply.Content.resize(nLen);
    return socket->Read((unsigned char*)reply.Content.data(), nLen);
}
bool ReadArray(TinySocketClient* socket, Reply& reply)
{
    //首先读一行获取长度
    if (!ReadLine(socket, reply))
        return false;

    //转换字符串的长度为整数
    long long nLen = reply.Integer();
    reply.Children.reserve(nLen);
    if (reply.Children.capacity() < nLen)
        return false;

    for (int i = 0; i < nLen; i++)
    {
        //構造的時候讀取
        reply.Children.emplace_back(socket);
        //檢查讀取是否正確，不正確則放棄。
        if (!reply.Children.back())
            return false;
    }
    return true;
}


Reply::Reply(TinySocketClient* socket)
{
    Type = socket->ReadT<RESPCommand>();
    bool bOk = false;
    if (Type == RESPCommand::eError)
        bOk = ReadLine(socket, *this);
    else if (Type == RESPCommand::eSimpleString)
        bOk = ReadLine(socket, *this);
    else if (Type == RESPCommand::eInteger)
        bOk = ReadLine(socket, *this);
    else if (Type == RESPCommand::eBulkString)
        bOk = ReadFixLength(socket, *this);
    else if (Type == RESPCommand::eArray)
        bOk = ReadArray(socket, *this);

    if (!bOk)
        Type = RESPCommand::eEmpty;
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
    return _atoi64(Content.data());
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
Reply& Reply::Swap(Reply& r)
{
    std::swap(Type, r.Type);
    Content.swap(r.Content);
    Children.swap(r.Children);
    return *this;
}
TinyRedisClient::TinyRedisClient(const char* addres, int port):TinySocketClient(addres,port)
{

}
TinyRedisClient::~TinyRedisClient()
{

}
bool TinyRedisClient::SendLine(RESPCommand cmd, const unsigned char* data, int nLen)
{
    if (Send((const unsigned char*)&cmd, 1)!= 1)
        return false;
    if (nLen > 0)
    {
        if (Send(data, nLen) != nLen)
            return false;
    }

    return  Send((const unsigned char*)g_strCRLF, 2) == 2;
}
bool TinyRedisClient::SendError(const char* error, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (error)
            nLen = strlen(error);
    }
    return SendLine(RESPCommand::eError,(const unsigned char*)error, nLen);
}
bool TinyRedisClient::SendSimpleString(const char* str, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (str)
            nLen = strlen(str);
    }
    return SendLine(RESPCommand::eSimpleString, (const unsigned char*)str, nLen);
}
bool TinyRedisClient::SendInteger(long long nInt, RESPCommand cmd)
{
    std::string str = std::to_string(nInt);
    return SendLine(cmd, (const unsigned char*)str.data(), str.size());
}
bool TinyRedisClient::SendBulkString(const  char* data, int nLen)
{
    if (nLen < 0)
    {
        nLen = 0;
        if (data)
            nLen = strlen(data);
    }
    return SendBulkString((const unsigned char*)data, nLen);
}
bool TinyRedisClient::SendBulkString(const unsigned char* data, int nLen)
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
bool TinyRedisClient::SendArray(int nCount)
{
    return SendInteger(nCount, RESPCommand::eArray);
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
    Reply ret(this);
    if (!ret)
        return false;
    if (ret.Type == RESPCommand::eSimpleString)
        return _stricmp(ret.Content.c_str(), "OK") == 0;
    return false;
}
bool TinyRedisClient::Erase(const char* key)
{
    return Erase((const unsigned char*)key, key ? strlen(key) : 0);
}
bool TinyRedisClient::Erase(const unsigned char* key, int nKeyLen)
{
    if (!SendArray(2)) return false;

    if (!SendBulkString("DEL", 3)) return false;
    if (!SendBulkString(key, nKeyLen)) return false;

    //获取结果
    Reply ret(this);
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
    Reply ret(this);
    if (!ret)
        return false;
    if (ret.IsInteger())
        return ret.Integer() == 1;
    return false;
}
bool TinyRedisClient::Exists(const char* key)
{
    return Exists((const unsigned char*)key, key ? strlen(key) : 0);
}

