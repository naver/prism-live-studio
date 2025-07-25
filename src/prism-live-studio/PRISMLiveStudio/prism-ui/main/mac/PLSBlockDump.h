//
//  PLSBlockDump.h
//  PRISMLiveStudio
//
//  Created by Keven on 4/25/23.
//
//

#ifndef PLSBlockDump_h
#define PLSBlockDump_h

#pragma once
#include <atomic>
#include <string>
#include <QObject>
#include <QTimerEvent>
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <sys/time.h>
#include <thread>

uint64_t getTickCount();

class Event;

class PLSBlockDump : public QObject {
	Q_OBJECT

	PLSBlockDump();

public:
	static void *checkThread(void *param);

	static PLSBlockDump *instance();

	~PLSBlockDump() noexcept override;

	PLSBlockDump(const PLSBlockDump &) = delete;
	PLSBlockDump &operator=(const PLSBlockDump &) = delete;
	PLSBlockDump(PLSBlockDump &&) = delete;
	PLSBlockDump &operator=(PLSBlockDump &&) = delete;

	void startMonitor();
	void stopMonitor();
	void signExitEvent();

protected:
	void initSavePath();
	void checkThreadInner();
	bool isHandleSigned(Event *event, int milliSecond);
	bool isBlockState(uint64_t preHeartbeat, uint64_t currentTime);
	std::string saveDumpFile();

private:
	std::string dumpDirectory = "";

	int heartbeatTimer = 0;

	std::atomic<uint64_t> preEventTime = getTickCount();
	std::atomic<uint64_t> preObject = 0;
	std::atomic<uint64_t> preEvent = 0;

	Event *checkBlockThread = nullptr;
	Event *threadExitEvent = nullptr;
};

class Event {
public:
	Event()
	{
		pthread_mutex_init(&mutex_, NULL);
		pthread_cond_init(&cond_, NULL);
		signaled_ = false;
		shouldStop_ = false;
	}

	~Event() noexcept
	{
		pthread_mutex_destroy(&mutex_);
		pthread_cond_destroy(&cond_);
	}

	void set()
	{
		pthread_mutex_lock(&mutex_);
		signaled_ = true;
		pthread_cond_signal(&cond_);
		pthread_mutex_unlock(&mutex_);
	}

	int waitForSingleObject(int timeout_ms)
	{
		bool earlyReturn = false;
		pthread_mutex_lock(&mutex_);
		if (signaled_) {
			earlyReturn = true;
		}
		pthread_mutex_unlock(&mutex_);

		if (earlyReturn) {
			return 0;
		}

		int rc = 0;
		struct timespec ts;
		timeout_ms += 100;
		ts.tv_sec = timeout_ms / 1000;
		ts.tv_nsec = (timeout_ms % 1000) * 1000000;
		pthread_mutex_lock(&mutex_);
		rc = pthread_cond_timedwait(&cond_, &mutex_, &ts);
		signaled_ = false;
		pthread_mutex_unlock(&mutex_);
		return rc;
	}

	void close()
	{
		pthread_mutex_destroy(&mutex_);
		pthread_cond_destroy(&cond_);
	}

	void reset()
	{
		pthread_mutex_lock(&mutex_);
		signaled_ = false;
		pthread_mutex_unlock(&mutex_);
	}

	uintptr_t beginThread(void *context)
	{
		pthread_t thread;
		int rc = pthread_create(&thread, nullptr, PLSBlockDump::checkThread, context);
		if (rc != 0) {
			return 0;
		}
		return reinterpret_cast<uintptr_t>(thread);
	}

	void terminate()
	{
		shouldStop_ = true;
		set();
	}

private:
	pthread_mutex_t mutex_;
	pthread_cond_t cond_;
	std::atomic<bool> signaled_;
	std::atomic<bool> shouldStop_;
};

#endif /* PLSBlockDump_h */
