#include "tfc.h"
#include <algorithm>
using namespace TRC;
using namespace TFC;
//判断是否是小端字节
bool IsLittleEndian()
{
	unsigned short n = 1;
	return((unsigned char*)&n)[0] ? true : false;
}
template<class T>
T ByteToInteger(const unsigned char* buff)
{
	T num;
	memcpy(&num, buff, sizeof(T));
	if (IsLittleEndian())
	{
		unsigned char* h = (unsigned char*)&num;
		std::reverse(h, h + sizeof(T));
	}
	return num;
}
template<class T>
void ToBigEndian(T n, unsigned char* buff)
{
	memcpy(buff, &n, sizeof(T));
	if (IsLittleEndian())
		std::reverse(buff, buff + sizeof(T));
}
PacketHeader::PacketHeader()
{
	memset(byPkgLen, 0, static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE));
}
//将数据包中的数据转换为64位整数
long long PacketHeader::PacketLength()const
{
	return ByteToInteger<long long>(byPkgLen);
}
void PacketHeader::PacketLength(unsigned long long val)
{
	ToBigEndian(val, byPkgLen);
}

UploadFileHeader::UploadFileHeader()
{
	memset(Length, 0, static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE));
	memset(Extension, 0, static_cast<int>(PacketSize::eFDFS_FILE_EXT_NAME_MAX_LEN));
	byCmd = Command::eSTORAGE_PROTO_CMD_UPLOAD_FILE;
}
//设置文件长度
void UploadFileHeader::FileLenth(unsigned long long nLen)
{
	ToBigEndian(nLen, Length);
}
//设置文件的扩展名
void UploadFileHeader::FileExtension(const char* ext)
{
	if (!ext) return;
	int n = strlen(ext);
	memcpy(Extension, ext, min(n, static_cast<int>(PacketSize::eFDFS_FILE_EXT_NAME_MAX_LEN)));
}
//文件名为变长，独立写入
DeleteFileHeader::DeleteFileHeader()
{
	memset(Group, 0, static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN));
	byCmd = Command::eSTORAGE_PROTO_CMD_DELETE_FILE;
}
//设置分组名称
void DeleteFileHeader::GroupName(const char* g)
{
	if (!g)
		return;
	int nLen = strlen(g);
	memcpy(Group, g, min(nLen, static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN)));
}
//文件名为边长，独立写入
DownloadFileHeader::DownloadFileHeader()
{
	memset(Length1, 0, static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE));
	memset(Length2, 0, static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE));
	memset(Group, 0, static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN));
	byCmd = Command::eSTORAGE_PROTO_CMD_DOWNLOAD_FILE;

}
void DownloadFileHeader::GroupName(const char* g)
{
	if (!g)
		return;
	int nLen = strlen(g);
	memcpy(Group, g, min(nLen, static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN)));
}


StorageInfo::StorageInfo()
{
	memset(Packet, 0, 40);
}

StorageInfo::StorageInfo(const StorageInfo& r)
{
	memcpy(Packet, r.Packet, sizeof(Packet));
}
StorageInfo::StorageInfo(StorageInfo&& rr)
{
	Swap(rr);
}
StorageInfo& StorageInfo::operator =(const StorageInfo& r)
{
	memcpy(Packet, r.Packet, sizeof(Packet));
	return *this;
}
StorageInfo& StorageInfo::operator =(StorageInfo&& r)
{
	Swap(r);
	return *this;
}
void StorageInfo::Swap(StorageInfo& r)
{
	StorageInfo  t = *this;
	memcpy(Packet, r.Packet, sizeof(Packet));
	memcpy(r.Packet, t.Packet, sizeof(Packet));
}
StorageInfo::operator bool()const
{
	return strlen(GroupName()) > 0 && strlen(IPAddress()) > 0 && Port() > 0;
}
const char* StorageInfo::GroupName()const
{
	return (const char*)Packet;
}
const char* StorageInfo::IPAddress()const 
{
	return (const char*)(Packet + static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN));
}
unsigned int StorageInfo::Port()const
{
	return ByteToInteger<unsigned long long>(Packet + static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN) +
		static_cast<int>(PacketSize::eIP_ADDRESS_SIZE) -1);
}
unsigned char StorageInfo::StorePathIndex()
{
	return Packet[sizeof(Packet)-1];
}
TrackerServer::TrackerServer(const char* address, int port):TinySocketClient(address, port)
{
	Connect();
}
StorageInfo TrackerServer::QueryStorageStore()
{
	StorageInfo info;
	PacketHeader header;
	header.byCmd = Command::eTRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE;
	int nSize = sizeof(PacketHeader);
	if (Send((const unsigned char*)&header, nSize) != nSize)
		return info;

	PacketHeader resp;
	if (!Read((unsigned char*)&resp, nSize))
		return info;
	
	if (resp.byStatus != 0)									return info;
	if (resp.byCmd != Command::eTRACKER_PROTO_CMD_RESP)		return info;
	nSize = resp.PacketLength();

	if (!Read((unsigned char*)&info, nSize))
		return info;

	return info;
}
FileFullName::operator bool()const
{
	return Content.size() > static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN);
}
const char* FileFullName::GroupName()const
{
	return Content.c_str();
}
const char* FileFullName::FileName()const
{
	if (Content.size() < static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN))
		return NULL;
	return Content.data() + static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN);
}
StorageServer::StorageServer(const char* address, int port) :TinySocketClient(address, port)
{
	Connect();
}
//上传文件，返回文件的的名称
struct FileFullName StorageServer::Upload(const unsigned char* fileContent, int nLen, const char* ext)
{
	struct FileFullName name;
	UploadFileHeader upload;
	upload.byCmd = Command::eSTORAGE_PROTO_CMD_UPLOAD_FILE;
	upload.FileExtension(ext);
	upload.FileLenth(nLen);
	//整个包的长度，长度需要减去包头的长度。
	upload.PacketLength(sizeof(UploadFileHeader) + nLen - sizeof(PacketHeader));
	//发送头
	SendT(upload);

	//发送文件内容
	Send(fileContent,nLen);

	//接收结果
	PacketHeader resp;
	int nSize = sizeof(PacketHeader);
	if (!Read((unsigned char*)&resp, nSize))
		return name;

	if (resp.byStatus != 0)									return name;
	if (resp.byCmd != Command::eTRACKER_PROTO_CMD_RESP)		return name;
	nSize = resp.PacketLength();

	name.Content.resize(nSize);
	if (!Read((unsigned char*)name.Content.data(), nSize))
	{
		name.Content.clear();
		return name;
	}
	return name;
}
//删除文件
bool StorageServer::Delete(const char* group, const char* fileName)
{
	if (!group || !fileName)
		return false;
	int nGroupLen = strlen(group);
	int nFileNameLen = strlen(fileName);
	if (nGroupLen <= 0 || nFileNameLen <= 0)
		return false;
	
	DeleteFileHeader header;
	int nTotalSize = static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN) + nFileNameLen;
	header.byCmd = Command::eSTORAGE_PROTO_CMD_DELETE_FILE;
	header.PacketLength(nTotalSize);
	header.GroupName(group);

	if (SendT(header) != sizeof(DeleteFileHeader))
		return false;
	if (Send((const unsigned char*)fileName, nFileNameLen) != nFileNameLen)
		return false;

	PacketHeader resp;
	if (!Read((unsigned char*)&resp, sizeof(PacketHeader)))
		return false;

	if (resp.byStatus != 0)									return false;
	if (resp.byCmd != Command::eTRACKER_PROTO_CMD_RESP)		return false;
	if (resp.PacketLength() < 0)
		return false;

	return true;
}
//下载文件
bool StorageServer::Download(const char* group, const char* fileName,
	const std::function<unsigned char* (int nLen)>& cb)
{
	if (!group || !fileName)
		return false;
	int nGroupLen = strlen(group);
	int nFileNameLen = strlen(fileName);
	if (nGroupLen <= 0 || nFileNameLen <= 0)
		return false;

	DownloadFileHeader header;
	header.GroupName(group);
	int nSize = sizeof(DownloadFileHeader) - sizeof(PacketHeader) + nFileNameLen;
	header.PacketLength(nSize);
	if (Send((const unsigned char*)&header, sizeof(DownloadFileHeader)) != sizeof(DownloadFileHeader))
		return false;
	if (Send((const unsigned char*)fileName, nFileNameLen) != nFileNameLen)
		return false;
	

	//接收结果
	PacketHeader resp;
	nSize = sizeof(PacketHeader);
	if (!Read((unsigned char*)&resp, nSize))
		return false;

	if (resp.byStatus != 0)									return false;
	if (resp.byCmd != Command::eTRACKER_PROTO_CMD_RESP)		return false;
	nSize = resp.PacketLength();
	unsigned char* buff = cb(nSize);
	if (!buff)
		return false;
	
	//读取文件体
	return Read(buff, nSize);
}
FastClient::FastClient(const char* address, int port)
{
	m_ptrTrackerServer.reset(new TrackerServer(address, port));
	if (!m_ptrTrackerServer->Good())
		m_ptrTrackerServer.reset();
}
bool FastClient::Good()const
{
	if (!m_ptrTrackerServer)
		return false;
	return m_ptrTrackerServer->Good();
}

StorageServerPtr FastClient::OpenStorage(const StorageInfo& info)
{
	std::string str = info.IPAddress();
	str += ":";
	str += std::to_string(info.Port());

	auto it = m_StorageServer.find(str);
	if (it != m_StorageServer.end())
		return it->second;
	
	StorageServerPtr ptr(new StorageServer(info.IPAddress(), info.Port()));
	if (!ptr->Good())
		return 0;
	
	m_StorageServer[str] = ptr;
	return ptr;
}
//上传文件，返回文件的的名称
struct FileFullName FastClient::Upload(const unsigned char* fileContent, int nLen,const char* ext)
{
	FileFullName name;
	auto info = m_ptrTrackerServer->QueryStorageStore();
	if (!info)
		return name;
	auto ptrStorage = OpenStorage(info);
	if (!ptrStorage)
		return name;
	return ptrStorage->Upload(fileContent, nLen, ext);
}
bool FastClient::Download(const char* group, const char* fileName,
	const std::function<unsigned char* (int nLen)>& cb)
{
	auto info = m_ptrTrackerServer->QueryStorageStore();
	if (!info)
		return false;
	auto ptrStorage = OpenStorage(info);
	if (!ptrStorage)
		return false;
	return ptrStorage->Download(group,fileName,cb);
}

bool FastClient::Download(FileFullName& info,
	const std::function<unsigned char* (int nLen)>& cb)
{
	if (!info || !cb)
		return false;
	return Download(info.GroupName(),info.FileName(),cb);
}
//删除文件
bool FastClient::Delete(FileFullName& info)
{
	if (!info)
		return false;
	return Delete(info.GroupName(), info.FileName());
}
//删除文件
bool FastClient::Delete(const char* group, const char* fileName)
{
	auto info = m_ptrTrackerServer->QueryStorageStore();
	if (!info)
		return false;
	auto ptrStorage = OpenStorage(info);
	if (!ptrStorage)
		return false;
	return ptrStorage->Delete(group, fileName);
}



