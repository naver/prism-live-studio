//
//  PLSBlockDump.mm
//  PRISMLiveStudio
//
//  Created by Keven on 4/25/23.
//
//

#import <Foundation/Foundation.h>
#import <mach/mach_time.h>
#import "PLSBlockDump.h"
#include "PLSUtil.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSApp.h"
#include "PLSBlockRecorder.hpp"
#include "pls/pls-obs-api.h"

extern std::atomic<bool> exception_happened;

uint64_t getTickCount()
{
	static mach_timebase_info_data_t s_timebase_info;
	if (s_timebase_info.denom == 0) {
		mach_timebase_info(&s_timebase_info);
	}
	uint64_t ticks = mach_absolute_time();
	uint64_t millis = ticks * s_timebase_info.numer / s_timebase_info.denom / 1000000;
	return millis;
}

const auto SAVE_DUMP_INTERVAL = 10000; // in milliseconds
const auto MAX_BLOCK_DUMP_COUNT = 3;

void *PLSBlockDump::checkThread(void *param)
{
	PLS_INFO(MAINFRAME_MODULE, "Thread for UI block entered.");
	auto self = static_cast<PLSBlockDump *>(param);
	self->checkThreadInner();
	PLS_INFO(MAINFRAME_MODULE, "Thread for UI block to exit.");
	return 0;
}

PLSBlockDump *PLSBlockDump::instance()
{
	static PLSBlockDump *instance = new PLSBlockDump();
	return instance;
}

PLSBlockDump::PLSBlockDump()
{
	threadExitEvent = new Event();
	initSavePath();
}

PLSBlockDump::~PLSBlockDump() noexcept
{
	stopMonitor();
	threadExitEvent->close();
}

void PLSBlockDump::startMonitor()
{
	if (!heartbeatTimer) {
		heartbeatTimer = this->startTimer(HEARTBEAT_INTERVAL);
		assert(heartbeatTimer > 0);
	}

	if (!checkBlockThread) {
		threadExitEvent->reset();
		checkBlockThread = new Event();

		checkBlockThread->beginThread(this);
	}
}

void PLSBlockDump::stopMonitor()
{
	if (heartbeatTimer) {
		this->killTimer(heartbeatTimer);
		heartbeatTimer = 0;
	}

	if (checkBlockThread) {
		threadExitEvent->set();
#ifdef DEBUG
		checkBlockThread->waitForSingleObject(LONG_MAX);
#else
		if (checkBlockThread->waitForSingleObject(5000) != 0) {
			PLS_WARN(MAINFRAME_MODULE, "Failed to wait block thread exit, terminate it");
			checkBlockThread->terminate();
		}
#endif
		checkBlockThread->close();
		checkBlockThread = nullptr;
	}
}

void PLSBlockDump::signExitEvent()
{
	PLS_INFO(MAINFRAME_MODULE, "Notify to exit block thread");
	threadExitEvent->set();
}

void PLSBlockDump::updateNotifyEvent(QObject *obj, QEvent *)
{
	preEventTime = getTickCount();

	if (!obj)
		return;

	std::lock_guard<std::recursive_mutex> lock(lockPreName);
	preObjectName = obj->objectName();
	preClassName = obj->metaObject()->className();
}

std::string PLSBlockDump::getPreviousObject()
{
	QString preObject;
	QString preClass;

	{
		std::lock_guard<std::recursive_mutex> lock(lockPreName);
		preObject = preObjectName;
		preClass = preClassName;
	}

	if (!preObject.isEmpty() && !preClass.isEmpty()) {
		auto text = QString("%1::%2").arg(preClass).arg(preObject);
		return text.toStdString();
	} else if (!preObject.isEmpty())
		return preObject.toStdString();
	else if (!preClass.isEmpty())
		return preClass.toStdString();
	else
		return "unknown";
}

void PLSBlockDump::initSavePath()
{
	NSArray *directories = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
	if ([directories count] == 0) {
		NSLog(@"Could not locate cache directory path.");
		return;
	}
	NSString *cachePath = [directories objectAtIndex:0];
	if ([cachePath length] == 0) {
		NSLog(@"Could not locate cache directory path.");
		return;
	}

	NSString *dumpDirectory = [cachePath stringByAppendingPathComponent:@"PRISMLiveStudio/BlockDump"];
	this->dumpDirectory = dumpDirectory.UTF8String;

	if (![[NSFileManager defaultManager] fileExistsAtPath:dumpDirectory]) {
		NSError *error = nil;
		[[NSFileManager defaultManager] createDirectoryAtPath:dumpDirectory withIntermediateDirectories:YES attributes:nil error:&error];
		if (error) {
			PLS_WARN(MAINFRAME_MODULE, "Failed to create directory for saving block dump. error:%d", error.code);
		}
	}
}

#define RESET_CHECK                                                                                                                                                             \
	{                                                                                                                                                                       \
		preEventTime = getTickCount();                                                                                                                                  \
		preDumpTime = 0;                                                                                                                                                \
		dumpCount = 0;                                                                                                                                                  \
		recorder.Reset();                                                                                                                                               \
		if (isBlocked) {                                                                                                                                                \
			isBlocked = false;                                                                                                                                      \
			pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);                                                                                              \
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s UI thread recovered and Mac may have slept", \
				  debug_mode ? "[Debug Mode]" : "");                                                                                                            \
		}                                                                                                                                                               \
	}

void PLSBlockDump::checkThreadInner()
{
	preEventTime = getTickCount();

	bool isBlocked = false;
	int dumpCount = 0;
	uint64_t preDumpTime = 0;

#ifdef _DEBUG
	bool debug_mode = true;
#else
	bool debug_mode = false;
#endif

	auto timeoutSec = PLSGpopData::instance()->getUIBlockingTimeS();
	QString timeoutStr = QString::number(timeoutSec);
	pls_add_global_field("blockTimeoutS", timeoutStr.toUtf8().constData(), PLS_SET_TAG_CN);

	PLSBlockRecorder recorder;
	uint64_t preLoopTime = getTickCount();

	while (!exception_happened.load() && !isHandleSigned(threadExitEvent, HEARTBEAT_INTERVAL)) {
		pls_sleep_ms(100);

		if (pls_ignore_render_drop()) {
			RESET_CHECK;
			continue;
		}

		auto crtTime = getTickCount();
		if (crtTime - preLoopTime > (10 * 1000)) {
			RESET_CHECK;
		}

		preLoopTime = crtTime;

		bool sreBlocked = isBlockState(preEventTime, crtTime, SRE_BLOCK_TIMEOUT);
		recorder.UpdateBlockState(sreBlocked, sreBlocked ? getPreviousObject() : std::string());

		bool blocked = isBlockState(preEventTime, crtTime, timeoutSec * 1000);
		if (blocked != isBlocked) {
			isBlocked = blocked;
			if (blocked) {
				std::string preObj = getPreviousObject();
				PLS_LOGEX(PLS_LOG_WARN, MAINFRAME_MODULE,
					  {
						  {"UIBlock", GlobalVars::prismSession.c_str()},
						  {"Position", preObj.c_str()},
					  },
					  "[PLSBlockDump] %s UI thread is blocked at %s", debug_mode ? "[Debug Mode]" : "", preObj.c_str());
			} else {
				pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
				PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s UI thread recovered", debug_mode ? "[Debug Mode]" : "");
			}
		}

		if (!blocked) {
			preDumpTime = 0;
			dumpCount = 0;
			continue;
		}

		if (dumpCount < MAX_BLOCK_DUMP_COUNT) {
			int dumpInterval = dumpCount * SAVE_DUMP_INTERVAL;
			if (crtTime - preDumpTime < dumpInterval) {
				continue;
			}

			std::string path = saveDumpFile();

			if (pls_ignore_render_drop()) {
				RESET_CHECK;
				continue;
			}

			if (exception_happened.load() || isHandleSigned(threadExitEvent, 0)) {
				PLS_INFO(MAINFRAME_MODULE, "[PLSBlockDump] Ignore the saved block dump because to exit thread");
				break;
			}

			if (!path.empty()) {
				pls_add_global_field("blockDumpPath", path.c_str(), PLS_SET_TAG_CN);
				PLS_INFO(MAINFRAME_MODULE, "[PLSBlockDump] blocked dump is sent to log process");
			}

			preDumpTime = getTickCount();
			++dumpCount;
		}
	}

	if (isBlocked) {
		pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
		PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s PRISM is exiting, so we think UI thread recovered",
			  debug_mode ? "[Debug Mode]" : "");
	}
}

bool PLSBlockDump::isBlockState(uint64_t preHeartbeat, uint64_t currentTime, int timeoutMs)
{
	if (currentTime <= preHeartbeat) {
		return false; // normal state
	}

	uint64_t heartbeatSpaceS = currentTime - preHeartbeat;
	if (heartbeatSpaceS < (uint64_t)timeoutMs) {
		return false; // normal state
	}

	return true; // blocked
}

bool PLSBlockDump::isHandleSigned(Event *event, int milliSecond)
{
	if (!event) {
		return false;
	}
	long result = event->waitForSingleObject(milliSecond);
	return result == 0;
}

std::string PLSBlockDump::saveDumpFile()
{
	return pls::generate_dump_file("UI block happen", "Block dump generated.");
}
