#pragma once
#include <Windows.h>
#include <process.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>

#define PIPE_MAGIC_CODE 0X0f0f0f0f

enum pipe_error_type {
	PIPE_UNKNOWN_ERROR = 0,
	PIPE_INVALID_HEADER,
	PIPE_READ_ERROR,
	PIPE_WRITE_ERROR,
	PIPE_ALLOC_MEMORY,
	PIPE_LARGE_BUFFER,
};

struct PipeMessageHeader {
	int magic_code = PIPE_MAGIC_CODE;
	unsigned msg_id = 0;
	unsigned body_len = 0;
};

//-------------------------------------------------------------------------
class PipeCallback {
public:
	virtual ~PipeCallback() {}

	virtual void OnMessage(PipeMessageHeader hdr,
			       std::shared_ptr<char> body) = 0;
	virtual void OnError(pipe_error_type type, unsigned code) = 0;
};

class PipeServer;
class PipeIO {
	struct SendInfo {
		PipeMessageHeader hdr;
		std::string body;
	};

public:
	PipeIO();
	virtual ~PipeIO();

	void StartServer(PipeServer *svr, PipeCallback *cb);
	void StartClient(HANDLE prism_process, HANDLE read_pipe,
			 HANDLE write_pipe, PipeCallback *cb);
	void Stop();

	bool PushSendMsg(const PipeMessageHeader &hdr, const char *body,
			 bool insert_front = false);

protected:
	void Start();
	bool PopMsg(SendInfo &info);

	static unsigned __stdcall ReadThreadFunc(void *pParam);
	static unsigned __stdcall WriteThreadFunc(void *pParam);

private:
	PipeCallback *callback = NULL;

	HANDLE read_handle = 0;
	HANDLE write_handle = 0;

	HANDLE thread_exit_event = 0;
	HANDLE read_thread = 0;
	HANDLE write_thread = 0;

	std::mutex msg_lock;
	std::vector<SendInfo> send_list;
};

class PipeServer {
public:
	PipeServer();
	virtual ~PipeServer();

	bool InitPipeIO();
	void UninitPipeIO();

	void GetServerHandle(HANDLE &write_out, HANDLE &read_out);
	void GetClientHandle(HANDLE &write_out, HANDLE &read_out);

private:
	HANDLE server_write = 0;
	HANDLE client_read = 0;

	HANDLE server_read = 0;
	HANDLE client_write = 0;
};

//-------------------------------------------------------------------------
struct PipeErrorString {
	pipe_error_type type;
	std::string str;
};

static PipeErrorString pipe_error_str[] = {
	{PIPE_INVALID_HEADER, "invalid-header"},
	{PIPE_READ_ERROR, "read-pipe-error"},
	{PIPE_WRITE_ERROR, "write-pipe-error"},
	{PIPE_ALLOC_MEMORY, "fail-alloc-memory"},
	{PIPE_LARGE_BUFFER, "message-count-large"},
	{PIPE_UNKNOWN_ERROR, ""}}; // End flag

static std::string GetPipeError(pipe_error_type type)
{
	int index = 0;
	while (pipe_error_str[index].type != PIPE_UNKNOWN_ERROR) {
		if (type == pipe_error_str[index].type) {
			return pipe_error_str[index].str;
		}

		++index;
	}

	return "unknown";
};
