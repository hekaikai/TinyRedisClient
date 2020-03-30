// TinyRedisClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "trc.h"
#include <algorithm>
int main()
{
	WORD version = MAKEWORD(2, 2);
	WSADATA data;

	int nRet = WSAStartup(version, &data);

	TRC::TinyRedisClient client("127.0.0.1", 6379);
	client.Connect();
	if (client.Set("hello", "world"))
	{
		std::cout << "ok!\n";
	}
	if (client.Exists("hello"))
	{
		std::cout << "ok!\n";
	}
	{
		TRC::ReplyParser replyParser;
		if (client.Get("hello", &replyParser))
		{
			std::cout << "ok!\n" << replyParser.Content << std::endl;;
		}
	}

	if (client.Del("hello"))
	{
		std::cout << "ok!\n";
	}
	if (client.Exists("hello"))
	{
		std::cout << "ok!\n";
	}
	{
		TRC::ReplyParser replyParser;
		if (client.Get("hello", &replyParser))
		{
			std::cout << "ok!\n" << replyParser.Content << std::endl;;
		}
	}
	for (int i = 0; i < 1000; i++)
	{
		std::string strKey = "a";
		strKey += std::to_string(i);
		std::string strVal = std::to_string(i);
		client.Set(strKey.c_str(), strVal.c_str());
	}
	{
		TRC::ScanCursor replyCursor;
		int n = 0;
		std::vector<std::string> vec;
		int nNextCursor = 0;
		do
		{
			replyCursor.Reset();
			if (client.Scan(nNextCursor, &replyCursor, "a*", 100))
			{
				n += replyCursor.Count();
				for (int i = 0; i < replyCursor.Count(); i++)
				{
					auto reply =  replyCursor.Key(i);
					vec.push_back(reply->Content);
				}
				std::cout << "ok!" << replyCursor.Count()<< std::endl;;
			}
			nNextCursor = replyCursor.Cursor();
		} while (!replyCursor.IsFinished());

		std::sort(vec.begin(), vec.end());
		std::cout << "total " << vec.size() << std::endl;
	}
    std::cout << "Hello World!\n";
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
