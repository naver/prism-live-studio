//
//  PLSWatermark.hpp
//  PRISMLiveStudio
//
//  Created by Keven on 2/21/24.
//
//

#ifndef PLSWatermark_hpp
#define PLSWatermark_hpp

#include <stdio.h>
#include <string>
#include <filesystem>
#include <obs-module.h>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <obs.hpp>

enum PLSWatermarkTransitionType { PLS_WATERMARK_TRANSITION_NONE, PLS_WATERMARK_TRANSITION_FADE, PLS_WATERMARK_TRANSITION_SWIPE, PLS_WATERMARK_TRANSITION_SLIDE };

struct PLSWatermarkConfig {
	std::filesystem::path filePath;

	uint32_t startDelayMs = 0;
	uint32_t showingIntervalMs = 0;
	uint32_t showingDurationMs = 0;

	uint32_t alignment = 0;
	float paddingOffsetX = 0;
	float paddingOffsetY = 0;

	uint32_t channel = 0;

	struct {
		PLSWatermarkTransitionType type = PLS_WATERMARK_TRANSITION_NONE;
		uint32_t durationMs = 0;
	} transition;

	std::function<void(std::string)> logFunction;

	PLSWatermarkConfig(std::function<void(std::string)> logFunction = nullptr);
	PLSWatermarkConfig(std::string configJson, std::string channelName, std::function<void(std::string)> logFunction = nullptr);

private:
	uint32_t translateAlignment(std::string alignment);
};

struct PLSWatermarkSource {
	OBSSource source = nullptr;
	OBSSceneItem item = nullptr;
};

class PLSWatermark {
public:
	PLSWatermark(std::shared_ptr<PLSWatermarkConfig> config);
	~PLSWatermark();

	std::shared_ptr<PLSWatermarkConfig> getConfig();

	void start();
	void stop();

	void updatePosition();

private:
	mutable std::mutex _mutex;

	OBSScene _scene = nullptr;
	std::shared_ptr<PLSWatermarkSource> horizontalSource;
	std::shared_ptr<PLSWatermarkSource> verticalSource;
	std::thread _runningThread;
	std::atomic<bool> _running = false;
	std::condition_variable _runningCv;
	std::mutex _runningMutex;
	std::shared_ptr<PLSWatermarkConfig> _config;

	void initWithConfig(std::shared_ptr<PLSWatermarkConfig> config);

	std::shared_ptr<PLSWatermarkSource> createSource(bool isVertical);

	void runningThread();

	std::string transitionIdByType(PLSWatermarkTransitionType type);

	void updatePosition(std::shared_ptr<PLSWatermarkSource> source);
};

#endif /* PLSWatermark_hpp */
