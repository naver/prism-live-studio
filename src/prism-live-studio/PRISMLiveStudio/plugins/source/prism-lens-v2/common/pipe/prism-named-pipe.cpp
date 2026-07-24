#include "prism-named-pipe.h"
//#include "prism-str-helper.h"
#include "prism-vcam-define.h"
//#include "base64.hpp"
//#include "cJSON.h"
#include <Windows.h>
#include <namedpipeapi.h>
#include <string>
#include <thread>
#include <utility>
#include <list>
#include <atomic>
#include <vector>
#include <array>
#include <stdint.h>

void DebugLog(const wchar_t *msg)
{
#ifdef _DEBUG
	OutputDebugString(msg);
#endif
}

static std::wstring SplitFileName(std::wstring path)
{
	size_t suffixPos = path.find_last_of('\\');
	std::wstring subStr = path.substr(suffixPos + 1);
	return subStr;
}

std::string ToUtf8(const wchar_t *str)
{
	if (!str)
		return "";
	int n = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
	std::vector<char> vecBuffer(n + 1);
	auto pBuffer = vecBuffer.data();
	n = WideCharToMultiByte(CP_UTF8, 0, str, -1, pBuffer, n, nullptr, nullptr);
	pBuffer[n] = 0;
	std::string ret(pBuffer);
	return ret;
}

std::wstring utf8_to_unicode(const char *utf8_str)
{
	int unicode_length = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, nullptr, 0);
	if (unicode_length == 0) {
		return L"";
	}

	std::wstring unicode_str(unicode_length, L'\0');
	if (MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, &unicode_str[0], unicode_length) == 0) {
		return L"";
	}

	return unicode_str;
}

typedef std::pair<HANDLE, HANDLE> pipe_instance;

struct named_pipe_server {
	std::wstring name;
	void *user = nullptr;
	prism_vcam_log_cb log_cb = nullptr;
	prism_vcam_msg_cb msg_cb = nullptr;
	std::thread main_thread;
	HANDLE main_handle = nullptr;
	std::list<pipe_instance> instances;
	std::atomic_bool exit;
};

struct named_pipe_client {
	std::wstring name;
	HANDLE handle = nullptr;
};

static bool check_proc_available(named_pipe_server_t *svr, const wchar_t *proc)
{
	bool available = false;

	if (!svr->msg_cb)
		return false;

	prism_vcam_cb_msg msg = {0};
	svr->msg_cb(svr->user, PRISM_VCAM_CB_MSG_VIP, &msg);
	if (msg.bool_value)
		return true;

	memset(&msg, 0, sizeof(prism_vcam_cb_msg));
	svr->msg_cb(svr->user, PRISM_VCAM_CB_MSG_WHITELIST, &msg);

	HANDLE json_file = CreateFileW(utf8_to_unicode(msg.str_value).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (json_file) {
		DWORD size = GetFileSize(json_file, NULL);
		if (size == INVALID_FILE_SIZE) {
			CloseHandle(json_file);
			return available;
		}
		char *res = new char[size + 1];
		if (res == nullptr) {
			CloseHandle(json_file);
			return available;
		}
		memset(res, 0, size + 1);
		DWORD len = 0;

		if (ReadFile(json_file, res, size, &len, nullptr)) {
			res[len] = 0;
			/*cJSON *vcam_json = cJSON_Parse(res);
			cJSON *items_json = cJSON_GetObjectItem(
				vcam_json, base64_encode("whitelist").c_str());
			int count = cJSON_GetArraySize(items_json);
			for (int i = 0; i < count; i++) {
				cJSON *item = cJSON_GetArrayItem(items_json, i);
				std::string wl_name =
					base64_decode(item->valuestring);
				if (!!wcsstr(proc, U2W(wl_name))) {
					available = true;
					break;
				}
			}

			cJSON_Delete(vcam_json);*/
		}
		delete[] res;
		CloseHandle(json_file);
	}
	return available;
}

static bool handle_connection(HANDLE hdl, named_pipe_server_t *svr, named_pipe_pkg &pkg)
{
	BOOL fSuccess = FALSE;
	HANDLE hPipe = hdl;
	named_pipe_server_t *server = svr;
	DWORD cbWritten = 0;

	std::wstring proc_name = SplitFileName(pkg.proc_name);
	std::string s_proc_name = ToUtf8(proc_name.c_str());
	std::string s_proc_path = ToUtf8(pkg.proc_name);
	if (server->log_cb) {
		char msg[512] = {0};
		sprintf(msg, "[vcam] Process[%lu] %s is connecting virtual cam, seq_nb=%d.", pkg.pid, s_proc_name.c_str(), pkg.seq_nb);
		server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
	}

	bool in_whitelist = check_proc_available(server, pkg.proc_name);
	if (in_whitelist) {
		pkg.seq_nb++;
		if (server->log_cb) {
			char msg[512] = {0};
			sprintf(msg, "[vcam] Process[%lu] %s is in white list, allow connecting. Reply seq_nb=%d.", pkg.pid, s_proc_name.c_str(), pkg.seq_nb);
			server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
		}

		fSuccess = WriteFile(hPipe, &pkg, sizeof(named_pipe_pkg), &cbWritten, NULL);
		if (!fSuccess || sizeof(named_pipe_pkg) != cbWritten) {
			if (server->log_cb) {
				char msg[256] = {0};
				sprintf(msg, "[vcam] InstanceThread WriteFile failed, ERROR=%lu.", GetLastError());
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg);
			}
			return false;
		}
	} else {
		pkg.seq_nb = -1;
		if (server->log_cb) {
			char msg[512] = {0};
			sprintf(msg, "[vcam] Process[%lu] %s is NOT in white list, refuse connecting. Reply seq_nb=%d.", pkg.pid, s_proc_name.c_str(), pkg.seq_nb);
			server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
		}

		fSuccess = WriteFile(hPipe, &pkg, sizeof(named_pipe_pkg), &cbWritten, NULL);
		if (!fSuccess || sizeof(named_pipe_pkg) != cbWritten) {
			if (server->log_cb) {
				char msg[256] = {0};
				sprintf(msg, "[vcam] InstanceThread WriteFile failed, ERROR=%lu.", GetLastError());
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg);
			}
			return false;
		}
	}

	return true;
}

struct Params {
	HANDLE hdl;
	named_pipe_server_t *svr;
};

static unsigned __stdcall named_pipe_instance_thread(void *pParam)
{
	auto param = (Params *)pParam;

	HANDLE hPipe = param->hdl;
	named_pipe_server_t *server = param->svr;
	DWORD cbBytesRead = 0;
	BOOL fSuccess = FALSE;

	delete param;
	param = nullptr;

	if (server->log_cb)
		server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, "[vcam] named_pipe_instance_thread start");

	do {
		named_pipe_pkg pkg = {0};
		fSuccess = ReadFile(hPipe, &pkg, sizeof(named_pipe_pkg), &cbBytesRead, NULL);
		if (!fSuccess || cbBytesRead == 0) {
			DWORD lastError = GetLastError();
			if (lastError == ERROR_BROKEN_PIPE) {
				if (server->log_cb)
					server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, "[vcam] InstanceThread: client disconnected.");
			} else {
				if (server->log_cb) {
					char msg[256] = {0};
					sprintf(msg, "[vcam] InstanceThread ReadFile failed, ERROR=%lu.", lastError);
					server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg);
				}
			}
			break;
		}

		if (pkg.type == PKG_MESSAGE) {
			if (server->log_cb) {
				char msg[PRISM_VCAM_MESSAGE_BUF_SIZE + 1] = {0};
				memmove(msg, pkg.message, PRISM_VCAM_MESSAGE_BUF_SIZE);
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
			}
		} else if (pkg.type == PKG_CONNECT) {
			if (!handle_connection(hPipe, server, pkg))
				break;
		} else {
			std::wstring proc_name = SplitFileName(pkg.proc_name);
			std::string s_proc_name = ToUtf8(proc_name.c_str());
			char guid[PRISM_VCAM_GUID_SIZE + 1] = {0};
			memmove(guid, pkg.guid, PRISM_VCAM_GUID_SIZE);
			prism_vcam_cb_msg msg = {0};
			msg.cam_idx = pkg.cam_idx;
			memmove(msg.guid, pkg.guid, PRISM_VCAM_GUID_SIZE);
			memmove(msg.str_value, s_proc_name.c_str(), s_proc_name.size());
			switch (pkg.type) {
			case PKT_HEARTBEAT:
				if (server->msg_cb)
					server->msg_cb(server->user, PRISM_VCAM_CB_MSG_HEARTBEAT, &msg);
				break;
			case PKG_START:
				if (server->log_cb) {
					std::string s_proc_path = ToUtf8(pkg.proc_name);
					char msg[256] = {0};
					sprintf(msg, "[vcam] VCAM [GUID %s] in process[%lu] %s is requesting start.", guid, pkg.pid, s_proc_name.c_str());
					server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
				}

				if (server->msg_cb)
					server->msg_cb(server->user, PRISM_VCAM_CB_MSG_START, &msg);
				break;
			case PKG_STOP:
				if (server->log_cb) {
					std::string s_proc_path = ToUtf8(pkg.proc_name);
					char msg[256] = {0};
					sprintf(msg, "[vcam] VCAM [GUID %s] in process[%lu] %s is requesting stop.", guid, pkg.pid, s_proc_name.c_str());
					server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, msg);
				}

				if (server->msg_cb)
					server->msg_cb(server->user, PRISM_VCAM_CB_MSG_STOP, &msg);
				break;
			default:
				break;
			}
		}
	} while (!server->exit);

	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	if (server->log_cb)
		server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, "[vcam] named_pipe_instance_thread finished");

	return 0;
}

static inline void *create_full_access_security_descriptor()
{
	void *sd = malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (!sd) {
		return NULL;
	}

	if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)) {
		goto error;
	}

	if (!SetSecurityDescriptorDacl(sd, true, NULL, false)) {
		goto error;
	}

	return sd;

error:
	free(sd);
	return NULL;
}

static void named_pipe_main_thread(named_pipe_server_t *svr)
{
	named_pipe_server_t *server = svr;

	void *sd = create_full_access_security_descriptor();
	if (!sd) {
		if (server->log_cb) {
			char msg[256] = {0};
			sprintf(msg, "[vcam] Fail to create full access security descriptor.");
			server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg);
		}
		return;
	}
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = sd;
	sa.bInheritHandle = false;

	do {
		server->main_handle = INVALID_HANDLE_VALUE;
		HANDLE hPipe = CreateNamedPipe(server->name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, PRISM_VCAM_PIPE_BUF_SIZE,
					       PRISM_VCAM_PIPE_BUF_SIZE, 0, &sa);
		if (hPipe == INVALID_HANDLE_VALUE) {
			if (server->log_cb) {
				char msg[256] = {0};
				sprintf(msg, "[vcam] CreateNamedPipe failed, ERROR=%lu.", GetLastError());
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg);
			}

			break;
		}

		server->main_handle = hPipe;
		if (server->exit) {
			CloseHandle(hPipe);
			break;
		}

		bool Connected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (Connected) {
			if (server->log_cb)
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, "[vcam] Client connected, creating a processing thread.");

			auto param = new Params;
			param->hdl = hPipe;
			param->svr = server;
			auto hThread = (HANDLE)_beginthreadex(nullptr, 0, named_pipe_instance_thread, param, 0, nullptr);
			if (!hThread) {
				errno_t err;
				_get_errno(&err);
				if (server->log_cb) {
					std::array<char, 512> msg = {};
					snprintf(msg.data(), msg.size(), "[vcam] Failed to create thread. errno: %d", err);
					server->log_cb(server->user, PRISM_VCAM_CB_LOG_ERROR, msg.data());
				}

				delete param;
				param = nullptr;
				CloseHandle(hPipe);
				break;
			}

			server->instances.emplace_back(std::make_pair(hPipe, hThread));
		} else {
			if (server->log_cb)
				server->log_cb(server->user, PRISM_VCAM_CB_LOG_WARN, "[vcam] The client could not connect, so close the pipe.");
			CloseHandle(hPipe);
		}
	} while (!server->exit);

	free(sd);
}

named_pipe_server_t *start_pipe_server(named_pipe_info_t *info)
{
	named_pipe_server_t *server = nullptr;
	server = new (std::nothrow) named_pipe_server_t;
	if (!server)
		return nullptr;
	server->name = info->name;
	server->user = info->opaque;
	server->log_cb = info->log_cb;
	server->msg_cb = info->msg_cb;
	server->exit = false;
	std::thread t(named_pipe_main_thread, server);
	server->main_thread.swap(t);
	if (server->log_cb)
		server->log_cb(server->user, PRISM_VCAM_CB_LOG_INFO, "[vcam] Pipe server created.");
	return server;
}

void stop_pipe_server(named_pipe_server_t **svr)
{
	if (svr && *svr) {
		named_pipe_server_t *server = *svr;

		server->exit = true;

		if (server->main_handle != INVALID_HANDLE_VALUE)
			CancelIoEx(server->main_handle, NULL);

		if (server->main_thread.joinable())
			server->main_thread.join();

		for (auto iter = server->instances.begin(); iter != server->instances.end(); iter++) {
			CancelIoEx(((*iter).first), NULL);
			if ((*iter).second) {
				WaitForSingleObject((*iter).second, INFINITE);
				CloseHandle((*iter).second);
			}
		}

		if (server->log_cb)
			server->log_cb(server->user, PRISM_VCAM_CB_LOG_WARN, "[vcam] Goodbye!.");

		delete server;
		*svr = NULL;
	}
}

named_pipe_client_t *create_pipe_client(const wchar_t *server_name)
{
	named_pipe_client_t *client = new named_pipe_client_t;
	if (!client)
		return NULL;

	HANDLE hPipe;
	do {
		hPipe = CreateFile(server_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		DWORD err = GetLastError();
		if (err != ERROR_PIPE_BUSY) {
			wchar_t log[256] = {0};
			swprintf_s(log, L"CreateFile FAILED(%d).\n", err);
			DebugLog(log);
			delete client;
			return NULL;
		}

		if (!WaitNamedPipe(server_name, PRISM_VCAM_PIPE_TIME_OUT_MS)) {
			DebugLog(L"WaitNamedPipe timeout.\n");
			delete client;
			return NULL;
		}

	} while (true);

	DWORD dwMode;
	dwMode = PIPE_READMODE_MESSAGE;
	BOOL fSuccess = SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL);
	if (!fSuccess) {
		wchar_t log[256] = {0};
		swprintf_s(log, L"SetNamedPipeHandleState FAILED(%d).\n", GetLastError());
		DebugLog(log);

		CloseHandle(hPipe);
		delete client;
		return NULL;
	}

	client->name = server_name;
	client->handle = hPipe;

	return client;
}

bool pipe_send_pkg(named_pipe_client_t *client, named_pipe_pkg_t *pkg)
{
	if (client && client->handle && pkg) {
		DWORD cbWritten;
		BOOL fSuccess = WriteFile(client->handle, pkg, sizeof(*pkg), &cbWritten, NULL);
		if (!fSuccess) {
			return false;
		}
		return true;
	}

	return false;
}

named_pipe_pkg_t *pipe_recv_pkg(named_pipe_client_t *client)
{
	if (client && client->handle) {
		BOOL fSuccess = FALSE;
		DWORD cbRead = 0;
		named_pipe_pkg *pkg = new named_pipe_pkg;
		if (!pkg)
			return NULL;
		memset(pkg, 0, sizeof(named_pipe_pkg));
		int retry_times = 3;
		do {
			fSuccess = ReadFile(client->handle, pkg, sizeof(named_pipe_pkg), &cbRead, NULL);

			if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
				break;
		} while (!fSuccess && retry_times--);

		if (fSuccess)
			return pkg;
		else {
			delete pkg;
			return NULL;
		}
	}
	return NULL;
}

void destroy_pipe_client(named_pipe_client_t **client)
{
	if (client && *client) {
		if ((*client)->handle)
			CloseHandle((*client)->handle);
		delete *client;
		*client = NULL;
	}
}

void cancel_pipe_client(named_pipe_client_t *client)
{
	if (client && client->handle)
		CancelIoEx(client->handle, NULL);
}
