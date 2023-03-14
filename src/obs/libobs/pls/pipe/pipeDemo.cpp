// pipeDemo.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
#include <shlwapi.h>
#include <string>
#include "pipe-io.h"

std::wstring GetFileDirectory(HINSTANCE hInstance = NULL)
{
	wchar_t dir[MAX_PATH] = {};
	GetModuleFileNameW(hInstance, dir, MAX_PATH);
	PathRemoveFileSpecW(dir); // no '\' at tail
	return std::wstring(dir) + std::wstring(L"\\");
}

class test : public PipeCallback {
public:
	virtual void OnMessage(PipeMessageHeader hdr,
			       std::shared_ptr<char> body)
	{
		printf("server> %s \n", body.get());
	}

	virtual void OnError(pipe_error_type type, unsigned code)
	{
		printf("server> error: %u \n", code);
	}
};

int main()
{
	test cb;

	PipeServer pipe;
	pipe.InitPipeIO();

	HANDLE client_write_handle;
	HANDLE client_read_handle;
	pipe.GetClientHandle(client_write_handle, client_read_handle);

	PipeIO io;
	io.StartServer(&pipe, &cb);

	{
		PROCESS_INFORMATION pi = {0};
		STARTUPINFOW si = {0};
		bool success = false;

		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
		si.hStdInput = client_read_handle;
		si.hStdOutput = client_write_handle;

		std::wstring path = GetFileDirectory();
		path += L"client.exe";

		success = !!CreateProcessW(NULL, (LPWSTR)path.c_str(), NULL,
					   NULL, true, CREATE_NEW_CONSOLE, NULL,
					   NULL, &si, &pi);

		if (success) {
			HANDLE process = pi.hProcess;
			CloseHandle(pi.hThread);
		}
	}

	bool failed = false;
	unsigned count = 0;
	while (1) {
		char body[1024];
		sprintf_s(body, "SERVER %u", ++count);

		PipeMessageHeader hdr;
		hdr.body_len = strlen(body) + 1;

		io.PushSendMsg(hdr, body);

		Sleep(20);

		if ((count % 30) == 0) {
			Sleep(2000);
		}
	}

	pipe.UninitPipeIO();
	io.Stop();

	printf("server end ! \n");
}
