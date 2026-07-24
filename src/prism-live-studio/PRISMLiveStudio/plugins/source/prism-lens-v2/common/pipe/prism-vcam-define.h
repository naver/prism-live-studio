#pragma once
#include <Windows.h>
#include <TCHAR.H>
#include <chrono>

#define PRISM_VCAM_ENABLE_AUTO_START_AND_STOP true

#define PRISM_VCAM_WHITE_LIST_PATH "\\PRISMLiveStudio\\user\\vcam.json"
#define PRISM_VCAM_WHITE_LIST_PATH_W TEXT(PRISM_VCAM_WHITE_LIST_PATH)

#define PRISM_VCAM_CONFIG_FILE_PATH "\\PRISMLiveStudio\\user\\prism-virtualcam.txt"
#define PRISM_VCAM_CONFIG_FILE_PATH_W TEXT(PRISM_VCAM_CONFIG_FILE_PATH)

#define PRISM_CAM_RUNNING_FLAG "com.prism.cam.running.flag"

#define PRISM_VCAM_PIPE_NAME L"\\\\.\\pipe\\PRISMCamVirtualDevicePipeName"
#define PRISM_VCAM_PIPE_BUF_SIZE 2048
#define PRISM_VCAM_MESSAGE_BUF_SIZE 1024
#define PRISM_VCAM_PIPE_TIME_OUT_MS 1000
#define PRISM_VCAM_MAX_PATH (2 * MAX_PATH)
#define PRISM_VCAM_GUID_SIZE 64
#define PRISM_VCAM_HEARTBEAT_INTERVAL_MS 1000
#define PRISM_VCAM_HEARTBEAT_TIMEOUT_MS 10 * 1000
#define PRISM_VCAM_MAX_MSG_QUEUE_SIZE 10

enum named_pipe_pkg_type { PKG_CONNECT, PKG_START, PKG_STOP, PKT_HEARTBEAT, PKG_MESSAGE };
enum prism_vcam_request { PRISM_VCAM_REQ_START = 1, PRISM_VCAM_REQ_STOP };

enum prism_vcam_cb_log_level {
	PRISM_VCAM_CB_LOG_INFO,
	PRISM_VCAM_CB_LOG_WARN,
	PRISM_VCAM_CB_LOG_ERROR,
};

enum prism_vcam_cb_msg_type { PRISM_VCAM_CB_MSG_VIP, PRISM_VCAM_CB_MSG_WHITELIST, PRISM_VCAM_CB_MSG_START, PRISM_VCAM_CB_MSG_STOP, PRISM_VCAM_CB_MSG_HEARTBEAT };

struct prism_vcam_cb_msg {
	char guid[PRISM_VCAM_GUID_SIZE];
	int cam_idx;
	union {
		char str_value[PRISM_VCAM_MAX_PATH]; // with utf8 format
		bool bool_value;
		long long_value;
		long long large_value;
		double double_value;
	};
};

struct prism_vcam_client {
	std::string proc;
	std::string guid;
	int cam_idx;
	std::chrono::system_clock::time_point last_heartbeat;
};

typedef void (*prism_vcam_log_cb)(void *opaque, int level, const char *log);
typedef void (*prism_vcam_msg_cb)(void *opaque, enum prism_vcam_cb_msg_type type, struct prism_vcam_cb_msg *msg);
