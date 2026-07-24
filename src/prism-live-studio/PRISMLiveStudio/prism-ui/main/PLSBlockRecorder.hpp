#pragma once
#include <thread>
#include "liblog.h"
#include "log/module_names.h"
#include "pls-gpop-data.hpp"

static const auto HEARTBEAT_INTERVAL = 500; // in milliseconds
static const auto SRE_BLOCK_TIMEOUT = 2000; // in milliseconds

class PLSBlockRecorder {
public:
	PLSBlockRecorder() {}
	~PLSBlockRecorder() { UploadBlockDuration(); }

	void Reset()
	{
		if (current_blocked) {
			current_blocked = false;
			PLS_LOG(PLS_LOG_INFO, MAINFRAME_MODULE, "[PLSBlockDump] temp block reset");
		}
	}

	void UpdateBlockState(bool ui_blocked, std::string blockPos = std::string())
	{
		if (current_blocked == ui_blocked)
			return;

		auto now_time = GetCurrentTimeMs();
		if (ui_blocked) {
			++block_count;
			current_blocked = true;
			blocked_pos = blockPos;
			block_start_time_ms = now_time - SRE_BLOCK_TIMEOUT;
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"Position", blockPos.c_str()}}, "[PLSBlockDump] temp block occurred %lld times, position=%s", block_count, blockPos.c_str());
		} else {
			current_blocked = false;
			auto block_time = now_time - block_start_time_ms;
			block_dur_ms += block_time;
			auto tm = std::to_string(block_time) + std::string(" ms");
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"Position", blocked_pos.c_str()}, {"Duration", tm.c_str()}}, "[PLSBlockDump] temp block restored after %lld ms", block_time);
		}
	}

protected:
	int64_t GetCurrentTimeMs()
	{
		auto now = std::chrono::steady_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
		return ms;
	}

	void UploadBlockDuration()
	{
#ifndef QT_DEBUG
		auto now_time = GetCurrentTimeMs();
		auto run_dur_ms = now_time - start_time_ms;
		if (run_dur_ms <= 0)
			return;

		if (current_blocked) {
			block_dur_ms += (now_time - block_start_time_ms);
		}

		assert(block_dur_ms < run_dur_ms);
		if (block_dur_ms > run_dur_ms) {
			block_dur_ms = run_dur_ms;
		}

		double percent = (run_dur_ms > 0) ? ((double)block_dur_ms * 100.0 / (double)run_dur_ms) : 0.0;
		int run_dur_sec = static_cast<int>(run_dur_ms / 1000);
		int block_dur_sec = static_cast<int>(block_dur_ms / 1000);
		std::string block_percent_str = QLocale::c().toString(percent, 'f', 1).toStdString();
		int count = static_cast<int>(block_count);

		const std::vector<std::pair<const char *, const char *>> fields = {
			{"app_run_duration", std::to_string(run_dur_sec).c_str()},
			{"app_block_duration", std::to_string(block_dur_sec).c_str()},
			{"app_block_percent", block_percent_str.c_str()},
			{"app_block_count", std::to_string(count).c_str()},
		};
		PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, fields, "[PLSBlockDump] block percent is %s%%", block_percent_str.c_str());
#endif
	}

private:
	bool current_blocked = false;
	std::string blocked_pos;
	int64_t block_start_time_ms = 0;
	int64_t block_count = 0;

	int64_t start_time_ms = GetCurrentTimeMs();
	int64_t block_dur_ms = 0;
};
