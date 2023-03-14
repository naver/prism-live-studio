#include "pipe-io.h"
#include <assert.h>
#include <io.h>
#include <fcntl.h>

#define MAX_MESSAGE_COUNT 200

bool CreatePipeIO(HANDLE *hReadPipe, HANDLE *hWritePipe)
{
	SECURITY_ATTRIBUTES sa = {0};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = true;

	if (!CreatePipe(hReadPipe, hWritePipe, &sa, 0)) {
		*hReadPipe = 0;
		*hWritePipe = 0;
		assert(false);
		return false;
	}

	return true;
}

bool IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond)
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

void SafeClosePipe(HANDLE &hdl)
{
	if (hdl) {
		CancelIoEx(hdl, NULL);
		CloseHandle(hdl);
		hdl = 0;
	}
}

void SafeCloseThread(HANDLE &thread_handle)
{
	if (thread_handle && (thread_handle != INVALID_HANDLE_VALUE)) {
		::WaitForSingleObject(thread_handle, INFINITE);
		::CloseHandle(thread_handle);
		thread_handle = 0;
	}
}

//-----------------------------------------------------------------------
PipeIO::PipeIO()
{
	thread_exit_event = ::CreateEvent(NULL, TRUE, FALSE, NULL);
}

PipeIO::~PipeIO()
{
	Stop();
	CloseHandle(thread_exit_event);
}

void PipeIO::StartServer(PipeServer *svr, PipeCallback *cb)
{
	HANDLE svr_write_handle;
	HANDLE svr_read_handle;
	svr->GetServerHandle(svr_write_handle, svr_read_handle);

	DuplicateHandle(GetCurrentProcess(), svr_read_handle,
			GetCurrentProcess(), &read_handle, 0, false,
			DUPLICATE_SAME_ACCESS);
	DuplicateHandle(GetCurrentProcess(), svr_write_handle,
			GetCurrentProcess(), &write_handle, 0, false,
			DUPLICATE_SAME_ACCESS);

	callback = cb;

	Start();
}

void PipeIO::StartClient(HANDLE prism_process, HANDLE read_pipe,
			 HANDLE write_pipe, PipeCallback *cb)
{
	DuplicateHandle(prism_process, read_pipe, GetCurrentProcess(),
			&read_handle, 0, false, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(prism_process, write_pipe, GetCurrentProcess(),
			&write_handle, 0, false, DUPLICATE_SAME_ACCESS);

	callback = cb;

	_setmode(_fileno(stdin), O_BINARY);
	_setmode(_fileno(stdin), O_BINARY);

	Start();
}

void PipeIO::Stop()
{
	SafeClosePipe(read_handle);
	SafeClosePipe(write_handle);

	::SetEvent(thread_exit_event);
	SafeCloseThread(read_thread);
	SafeCloseThread(write_thread);
}

bool PipeIO::PushSendMsg(const PipeMessageHeader &hdr, const char *body,
			 bool insert_front)
{
	assert(hdr.magic_code == PIPE_MAGIC_CODE);

	size_t msg_count = send_list.size();
	if (msg_count >= MAX_MESSAGE_COUNT) {
		callback->OnError(PIPE_LARGE_BUFFER, (unsigned)msg_count);
		assert(false);
		return false;
	}

	SendInfo info;
	info.hdr = hdr;
	info.body = (hdr.body_len > 0 && body) ? body : "";

	std::lock_guard<std::mutex> auto_lock(msg_lock);
	if (insert_front) {
		send_list.insert(send_list.begin(), info);
	} else {
		send_list.push_back(info);
	}

	return true;
}

void PipeIO::Start()
{
	if (read_thread && (read_thread != INVALID_HANDLE_VALUE)) {
		assert(false);
		return;
	}

	::ResetEvent(thread_exit_event);
	read_thread = (HANDLE)_beginthreadex(0, 0, ReadThreadFunc, this, 0, 0);
	write_thread =
		(HANDLE)_beginthreadex(0, 0, WriteThreadFunc, this, 0, 0);
}

bool PipeIO::PopMsg(SendInfo &info)
{
	std::lock_guard<std::mutex> auto_lock(msg_lock);

	if (send_list.empty())
		return false;

	info = send_list[0];
	send_list.erase(send_list.begin());

	return true;
}

unsigned __stdcall PipeIO::ReadThreadFunc(void *pParam)
{
	PipeIO *self = reinterpret_cast<PipeIO *>(pParam);

	BOOL success;
	DWORD bytes_read;
	while (!IsHandleSigned(self->thread_exit_event, 10)) {
		PipeMessageHeader hdr;
		success = ReadFile(self->read_handle, &hdr,
				   sizeof(PipeMessageHeader), &bytes_read,
				   NULL);
		if (!success || bytes_read != sizeof(PipeMessageHeader)) {
			self->callback->OnError(PIPE_READ_ERROR,
						GetLastError());
			break;
		}

		assert(hdr.magic_code == PIPE_MAGIC_CODE);
		if (hdr.magic_code != PIPE_MAGIC_CODE) {
			self->callback->OnError(PIPE_INVALID_HEADER,
						hdr.msg_id);
			break;
		}

		std::shared_ptr<char> body;
		if (hdr.body_len > 0) {
			char *ptr = new char[hdr.body_len];
			if (!ptr) {
				self->callback->OnError(PIPE_ALLOC_MEMORY,
							hdr.body_len);
				break;
			}

			body = std::shared_ptr<char>(ptr);

			success = ReadFile(self->read_handle, ptr, hdr.body_len,
					   &bytes_read, NULL);
			if (!success || bytes_read != hdr.body_len) {
				self->callback->OnError(PIPE_READ_ERROR,
							GetLastError());
				break;
			}
		}

		self->callback->OnMessage(hdr, body);
	}

	return 0;
}

unsigned __stdcall PipeIO::WriteThreadFunc(void *pParam)
{
	PipeIO *self = reinterpret_cast<PipeIO *>(pParam);

	SendInfo info;
	BOOL success;
	DWORD bytes_written;
	while (!IsHandleSigned(self->thread_exit_event, 0)) {
		if (!self->PopMsg(info)) {
			IsHandleSigned(self->thread_exit_event, 10);
			continue;
		}

		success = WriteFile(self->write_handle, &info.hdr,
				    sizeof(PipeMessageHeader), &bytes_written,
				    NULL);
		if (!success || bytes_written != sizeof(PipeMessageHeader)) {
			self->callback->OnError(PIPE_WRITE_ERROR,
						GetLastError());
			break;
		}

		if (info.hdr.body_len > 0) {
			success = WriteFile(self->write_handle,
					    info.body.c_str(),
					    info.hdr.body_len, &bytes_written,
					    NULL);
			if (!success || bytes_written != info.hdr.body_len) {
				self->callback->OnError(PIPE_WRITE_ERROR,
							GetLastError());
				break;
			}
		}
	}

	return 0;
}

//-----------------------------------------------------------------------
PipeServer::PipeServer() {}

PipeServer::~PipeServer()
{
	UninitPipeIO();
}

bool PipeServer::InitPipeIO()
{
	if (!CreatePipeIO(&client_read, &server_write))
		return false;

	if (!CreatePipeIO(&server_read, &client_write))
		return false;

	SetHandleInformation(server_write, HANDLE_FLAG_INHERIT, false);
	SetHandleInformation(server_read, HANDLE_FLAG_INHERIT, false);

	return true;
}

void PipeServer::UninitPipeIO()
{
	SafeClosePipe(server_write);
	SafeClosePipe(client_read);
	SafeClosePipe(server_read);
	SafeClosePipe(client_write);
}

void PipeServer::GetServerHandle(HANDLE &write_out, HANDLE &read_out)
{
	write_out = server_write;
	read_out = server_read;
}

void PipeServer::GetClientHandle(HANDLE &write_out, HANDLE &read_out)
{
	write_out = client_write;
	read_out = client_read;
}
