#pragma once
#include "trc.h"
#include <memory>
#include <string>
#include <map>
//tiny fastdfs client
// https://github.com/happyfish100/fastdfs
namespace TFC
{
	enum class PacketSize
	{
		eIP_ADDRESS_SIZE				=	16,
		eFDFS_GROUP_NAME_MAX_LEN		=	16,
		eFDFS_FILE_PREFIX_MAX_LEN		=	16,
		eFDFS_STORAGE_ID_MAX_SIZE		=	16,
		eFDFS_REMOTE_FILE_NAME_MAX_LEN	=	128,
		eFDFS_FILE_EXT_NAME_MAX_LEN		=	6,
		eFDFS_MAX_SERVERS_EACH_GROUP	=	32,
		eFDFS_MAX_GROUPS				=	512,
		eFDFS_PROTO_PKG_LEN_SIZE		=	8,
		eFDFS_VERSION_SIZE				=	6,
		eFDFS_DOMAIN_NAME_MAX_SIZE		=	128,
	};
	enum class Command:unsigned char
	{
		eEMPTY														=	0,
		eTRACKER_PROTO_CMD_SERVER_LIST_ALL_GROUPS					=	91,
		eTRACKER_PROTO_CMD_SERVER_LIST_STORAGE						=	92,
		eTRACKER_PROTO_CMD_RESP										=	100,
		eTRACKER_PROTO_CMD_SERVICE_QUERY_STORE_WITHOUT_GROUP_ONE	=	101,
		eTRACKER_PROTO_CMD_SERVICE_QUERY_FETCH_ONE					=	102,
		eTRACKER_PROTO_CMD_SERVICE_QUERY_UPDATE  					=	103,
		eSTORAGE_PROTO_CMD_UPLOAD_FILE								=	11,
		eSTORAGE_PROTO_CMD_DELETE_FILE								=	12,
		eSTORAGE_PROTO_CMD_DOWNLOAD_FILE							=	14,
		eSTORAGE_PROTO_CMD_UPLOAD_SLAVE_FILE						=	21,
	};
#pragma pack(push)
#pragma pack(1)
	struct  PacketHeader
	{
		unsigned char byPkgLen[static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE)];
		Command byCmd = Command::eEMPTY;
		unsigned char byStatus = 0;
		PacketHeader();
		//获取包的长度。
		long long PacketLength()const;
		//设置包的长度
		void PacketLength(unsigned long long nLen);
	};
	
	struct UploadFileHeader :public PacketHeader
	{
		unsigned char StorePathIndex = 0;
		//文件长度
		unsigned char Length[static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE)];
		//文件扩展名
		unsigned char Extension[static_cast<int>(PacketSize::eFDFS_FILE_EXT_NAME_MAX_LEN)];
		UploadFileHeader();
		//设置文件长度
		void FileLenth(unsigned long long nLen);
		//设置文件的扩展名
		void FileExtension(const char* ext);
	};
	struct DeleteFileHeader :public PacketHeader
	{
		//分组名
		unsigned char Group[static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN)];
		//文件名为变长，独立写入
		DeleteFileHeader();
		//设置分组名称
		void GroupName(const char* group);
	};
	struct DownloadFileHeader :public PacketHeader
	{
		//两个整数，暂时都为0。
		unsigned char Length1[static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE)];
		unsigned char Length2[static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE)];
		//分组名
		unsigned char Group[static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN)];
		//文件名为变长，独立写入
		DownloadFileHeader();
		//设置分组名称
		void GroupName(const char* group);
	};
#pragma pack(pop)

	struct StorageInfo
	{
		unsigned char Packet[static_cast<int>(PacketSize::eFDFS_GROUP_NAME_MAX_LEN) +
			static_cast<int>(PacketSize::eIP_ADDRESS_SIZE) + 
			static_cast<int>(PacketSize::eFDFS_PROTO_PKG_LEN_SIZE)];
		StorageInfo(); 
		StorageInfo(const StorageInfo& r);
		StorageInfo(StorageInfo& rr);
		StorageInfo& operator =(const StorageInfo& r);
		StorageInfo& operator =(StorageInfo&& r);
		void Swap(StorageInfo& r);
		const char* GroupName()const;
		const char* IPAddress()const;
		unsigned int Port()const;
		unsigned char StorePathIndex();
		operator bool()const;
	};
	class TrackerServer :public TRC::TinySocketClient
	{
	public:
		TrackerServer(const char* address, int port);
		//查询存储信息
		StorageInfo QueryStorageStore();
	};
	typedef std::shared_ptr<TrackerServer> TrackerServerPtr;
	struct FileNameInfo
	{
		std::string Content;
		const char* GroupName()const;
		const char* FileName()const;
		operator bool()const;
	};
	class StorageServer :public  TRC::TinySocketClient
	{
	public:
		StorageServer(const char* address, int port);
		//上传文件，返回文件的的名称
		FileNameInfo Upload(const unsigned char* fileContent, int nLen, const char* ext);

		//下载文件
		bool Download(const char* group, const char* fileName,
			const std::function<unsigned char* (int nLen)>& cb);

		//删除文件
		bool Delete(const char* group, const char* fileName);

	};
	typedef std::shared_ptr<StorageServer> StorageServerPtr;

	
	class FastClient 
	{
		TrackerServerPtr m_ptrTrackerServer;
		std::map<std::string, StorageServerPtr> m_StorageServer;
		StorageServerPtr OpenStorage(const StorageInfo& info);
	public:
		FastClient(const char* address, int port);
		bool Good()const;
		//上传文件，返回文件的的名称
		FileNameInfo Upload(const unsigned char* fileContent, int nLen, const char* ext);
		bool Download(const char* group, const char* fileName,
			const std::function<unsigned char* (int nLen)>& cb);
		//下载文件
		bool Download(FileNameInfo& info,
			const std::function<unsigned char* (int nLen)>& cb);
		//删除文件
		bool Delete(FileNameInfo& info);
		//删除文件
		bool Delete(const char* group, const char* fileName);
	};
}

