// client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <vector>
#include "pipe-io.h"
#include <assert.h>

std::mutex lock;
std::vector<std::string> rcv_list;

bool exited = false;

class test : public PipeCallback {
public:
	virtual void OnMessage(PipeMessageHeader hdr,
			       std::shared_ptr<char> body)
	{
		std::lock_guard<std::mutex> auto_lock(lock);
		rcv_list.push_back(std::string(body.get()));
	}

	virtual void OnError(pipe_error_type type, unsigned code)
	{
		printf("client> error: %u \n", code);
		exited = true;
	}
};

int main()
{
	test cb;

	PipeIO io;
	io.StartClient(&cb);

	while (exited == false) {
		std::string rcv = "";

		{
			std::lock_guard<std::mutex> auto_lock(lock);
			if (rcv_list.empty())
				continue;

			rcv = rcv_list[0];
			rcv_list.erase(rcv_list.begin());
		}

		char body[1024];
		sprintf_s(body, "message from client, response for [%s]",
			  rcv.c_str());

		PipeMessageHeader hdr;
		hdr.body_len = strlen(body) + 1;
		io.PushSendMsg(hdr, body);

		Sleep(10);
	}

	io.Stop();
}
