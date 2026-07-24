#pragma once
#include <Windows.h>
#include <string>
#include "prism-vcam-define.h"
#ifdef __cplusplus
extern "C" {
#endif

struct named_pipe_pkg {
	wchar_t proc_name[MAX_PATH];
	int seq_nb;
	int cam_idx;
	char guid[PRISM_VCAM_GUID_SIZE];
	unsigned long pid;
	char message[PRISM_VCAM_MESSAGE_BUF_SIZE];
	enum named_pipe_pkg_type type;
};

struct named_pipe_info {
	std::wstring name;
	void *opaque;
	prism_vcam_log_cb log_cb;
	prism_vcam_msg_cb msg_cb;
};

struct named_pipe_info;
struct named_pipe_server;
struct named_pipe_client;
typedef struct named_pipe_info named_pipe_info_t;
typedef struct named_pipe_server named_pipe_server_t;
typedef struct named_pipe_pkg named_pipe_pkg_t;
typedef struct named_pipe_client named_pipe_client_t;

extern named_pipe_server_t *start_pipe_server(named_pipe_info_t *info);
extern void stop_pipe_server(named_pipe_server_t **svr);

extern named_pipe_client_t *create_pipe_client(const wchar_t *server_name);
extern bool pipe_send_pkg(named_pipe_client_t *client, named_pipe_pkg_t *pkg);
extern named_pipe_pkg_t *pipe_recv_pkg(named_pipe_client_t *client);
extern void destroy_pipe_client(named_pipe_client_t **client);
extern void cancel_pipe_client(named_pipe_client_t *client);

#ifdef __cplusplus
}
#endif
