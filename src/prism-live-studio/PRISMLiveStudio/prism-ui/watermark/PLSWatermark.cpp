//
//  PLSWatermark.cpp
//  PRISMLiveStudio
//
//  Created by Keven on 2/21/24.
//  
//
	

#include "PLSWatermark.hpp"
#include "nlohmann/json.hpp"
#include <sstream>
#include <math.h>

#define PLS_WATERMARK_ID "pls_watermark"
#define PLS_WATERMARK_CHANNEL 32

static const float MAX_H_LOGO_HEIGHT = 240;
static const float MAX_H_LOGO_WIDTH = 1950;

static const float DESIGN_H_LOGO_HEIGHT = 48;
static const float DESIGN_H_LOGO_WIDTH = 390;

static const float MAX_V_LOGO_HEIGHT = 480;
static const float MAX_V_LOGO_WIDTH = 960;

static const float DESIGN_V_LOGO_HEIGHT = 96;
static const float DESIGN_V_LOGO_WIDTH = 192;

PLSWatermarkConfig::PLSWatermarkConfig(std::function<void(std::string)> logFunction) {
	channel = PLS_WATERMARK_CHANNEL;
	this->logFunction = logFunction;
}

PLSWatermarkConfig::PLSWatermarkConfig(std::string configJson, std::string channelName, std::function<void(std::string)> logFunction) {
	channel = PLS_WATERMARK_CHANNEL;
	this->logFunction = logFunction;

	try {
		auto json = nlohmann::json::parse(configJson);
		auto policy = json["policy"];
		auto service = policy[channelName];
		auto resource = service["resource"];
		
		std::string schema = service.value("schema", "none");
		if (schema == "periodic") {
			auto schemaConfig = json["schemas"][schema];
			
			transition.type = PLS_WATERMARK_TRANSITION_NONE;
			std::string transitionType = schemaConfig["transition"];
			if (transitionType == "fade") {
				transition.type = PLS_WATERMARK_TRANSITION_FADE;
			}
			if (transitionType == "swipe") {
				transition.type = PLS_WATERMARK_TRANSITION_SWIPE;
			}
			if (transitionType == "slide") {
				transition.type = PLS_WATERMARK_TRANSITION_SLIDE;
			}
			uint32_t transitionDudation = schemaConfig["animationMs"];
			transition.durationMs = transitionDudation;
			
			uint32_t startDelay = schemaConfig["startUpMs"];
			startDelayMs = startDelay;
			
			uint32_t interval = schemaConfig["intervalMs"];
			showingIntervalMs = interval;
			
			uint32_t duration = schemaConfig["periodicMs"];
			showingDurationMs = duration;
		}
		
		float paddindX = resource["paddingH"];
		paddingOffsetX = paddindX;
		
		float paddingY = resource["paddingV"];
		paddingOffsetY = paddingY;
		
		std::string alignmentH = resource["alignmentH"];
		uint32_t alignH = translateAlignment(alignmentH);
		
		std::string alignmentV = resource["alignmentV"];
		uint32_t alignV = translateAlignment(alignmentV);
		
		alignment = alignH | alignV;
	} catch (const nlohmann::json::exception& e) {
		if (logFunction) {
			std::stringstream message;
			message << "[watermark] Failed to parse config json: " << e.what();
			logFunction(message.str());
		}
	}
	
	if (logFunction) {
		std::stringstream message;
		message << "[watermark] Create watermark with config json: " << configJson << "channel: " << channelName;
		logFunction(message.str());
	}
}

uint32_t PLSWatermarkConfig::translateAlignment(std::string alignment) {
	static std::map<std::string, uint32_t> alignmentMap = {
		{"center", OBS_ALIGN_CENTER},
		{"left", OBS_ALIGN_LEFT},
		{"right", OBS_ALIGN_RIGHT},
		{"top", OBS_ALIGN_TOP},
		{"bottom", OBS_ALIGN_BOTTOM}
	};
	return alignmentMap[alignment];
}

PLSWatermark::PLSWatermark(std::shared_ptr<PLSWatermarkConfig> config) {
	initWithConfig(config);
}

void PLSWatermark::initWithConfig(std::shared_ptr<PLSWatermarkConfig> config) {
	_config = config;
	
	obs_scene_t *scene = obs_scene_create_private(PLS_WATERMARK_ID);
	if (scene) {
		_scene = scene;
		
//		obs_set_output_source(_config->channel, obs_scene_get_source(scene));
		
		if (config->filePath.empty() == false) {
			obs_data_t *image_settings = obs_data_create();
			obs_data_set_string(image_settings, "file", config->filePath.u8string().data());
			obs_source_t *image_source = obs_source_create_private("image_source", "", image_settings);
			obs_data_release(image_settings);
			
			if (image_source) {
				_source = image_source;
				
				obs_sceneitem_t *item = obs_scene_add(scene, image_source);
				
//				if (config->transition.type != PLS_WATERMARK_TRANSITION_NONE) {
//					std::string transitionId = transitionIdByType(config->transition.type);
//					if (transitionId.empty() == false) {
//						obs_source_t *fade = obs_source_create_private(transitionId.c_str(), "", NULL);
//						
//						uint32_t duration_ms = config->transition.durationMs;
//						if (duration_ms <= 0) {
//							duration_ms = 2000;
//						}
//						
//						obs_sceneitem_set_transition(item, true, fade);
//						obs_sceneitem_set_transition_duration(item, true, duration_ms);
//						
//						obs_sceneitem_set_transition(item, false, fade);
//						obs_sceneitem_set_transition_duration(item, false, duration_ms);
//					}
//				}
				
				_item = item;
				
				updatePosition();
				
//				obs_sceneitem_set_visible(item, false);
				
				if (_config->showingDurationMs == 0) {
					_config->showingDurationMs = 10000;
				}
			} else {
				if (_config->logFunction) {
					std::stringstream message;
					message << "[watermark] Failed to create image source: " << _config->filePath;
					_config->logFunction(message.str());
				}
			}
		} else {
			if (_config->logFunction) {
				_config->logFunction("[watermark] Invalid config file path: empty.");
			}
		}
	} else {
		if (_config->logFunction) {
			std::stringstream message;
			message << "[watermark] Failed to create private scene: " << PLS_WATERMARK_ID;
			_config->logFunction(message.str());
		}
	}
}

std::shared_ptr<PLSWatermarkConfig> PLSWatermark::getConfig() {
	return _config;
}

void PLSWatermark::updatePosition() {
	if (!_item) {
		return;
	}
	
	obs_sceneitem_set_bounds_type(_item, OBS_BOUNDS_STRETCH);
		
	float image_width = obs_source_get_width(_source);
	float image_height = obs_source_get_height(_source);
	
	struct vec2 size;
	
	if (image_height == MAX_H_LOGO_HEIGHT) {
		size.y = DESIGN_H_LOGO_HEIGHT;
		float h_scale = DESIGN_H_LOGO_HEIGHT / MAX_H_LOGO_HEIGHT;
		size.x = std::min(MAX_H_LOGO_WIDTH * h_scale, image_width * h_scale);
	} else if (image_height == MAX_V_LOGO_HEIGHT) {
		size.y = DESIGN_V_LOGO_HEIGHT;
		float v_scale = DESIGN_V_LOGO_HEIGHT / MAX_V_LOGO_HEIGHT;
		size.x = std::min(MAX_V_LOGO_WIDTH * v_scale, image_width * v_scale);
	} else {
		if (image_width > image_height) {
			size.y = DESIGN_H_LOGO_HEIGHT;
			float h_scale = DESIGN_H_LOGO_HEIGHT / image_height;
			size.x = std::min(MAX_H_LOGO_WIDTH * h_scale, image_width * h_scale);
		} else {
			size.y = DESIGN_V_LOGO_HEIGHT;
			float v_scale = DESIGN_V_LOGO_HEIGHT / image_height;
			size.x = std::min(MAX_V_LOGO_WIDTH * v_scale, image_width * v_scale);
		}
	}
	
	obs_sceneitem_set_bounds(_item, &size);

	float scene_width = obs_source_get_width(obs_scene_get_source(_scene));
	float scene_height = obs_source_get_height(obs_scene_get_source(_scene));
	
	float paddingOffsetX = _config->paddingOffsetX;
	float paddingOffsetY = _config->paddingOffsetY;
	
	uint32_t alignment = _config->alignment;
	if (alignment & OBS_ALIGN_RIGHT) {
		paddingOffsetX = scene_width - size.x - abs(_config->paddingOffsetX);
	}
	if (alignment & OBS_ALIGN_BOTTOM) {
		paddingOffsetY = scene_height - size.y - abs(_config->paddingOffsetY);
	}
	
	const struct vec2 pos = {
		paddingOffsetX,
		paddingOffsetY
	};
	obs_sceneitem_set_pos(_item, &pos);
}

void PLSWatermark::runningThread() {
	std::unique_lock<std::mutex> lock(_runningMutex);
	
	if (_config->startDelayMs > 0) {
		_runningCv.wait_for(lock, std::chrono::milliseconds(_config->startDelayMs));
	}
	
	while (_running) {
		obs_set_output_source(_config->channel, obs_scene_get_source(_scene));
		
		if (_running) {
			_runningCv.wait_for(lock, std::chrono::milliseconds(_config->showingDurationMs));
		}
		
		obs_set_output_source(_config->channel, NULL);

		if (_running) {
			_runningCv.wait_for(lock, std::chrono::milliseconds(_config->showingIntervalMs));
		}
	}
}

std::string PLSWatermark::transitionIdByType(PLSWatermarkTransitionType type) {
	switch (type) {
		case PLS_WATERMARK_TRANSITION_FADE:
			return "fade_transition";
		case PLS_WATERMARK_TRANSITION_SWIPE:
			return "swipe_transition";
		case  PLS_WATERMARK_TRANSITION_SLIDE:
			return "slide_transition";
		default:
			return "";
	}
}

void PLSWatermark::start() {
	if (_config->showingIntervalMs > 0) {
		_running = true;
		_runningThread = std::thread(&PLSWatermark::runningThread, this);
	} else {
		updatePosition();
//		obs_sceneitem_set_visible(_item, true);
		obs_set_output_source(_config->channel, obs_scene_get_source(_scene));
	}
}

void PLSWatermark::stop() {
	_running = false;
	{
		std::lock_guard<std::mutex> lock(_runningMutex);
		_runningCv.notify_one();
	}
	
	if (_runningThread.joinable()) {
		_runningThread.join();
	}
}

PLSWatermark::~PLSWatermark() {
	stop();
	
	obs_set_output_source(_config->channel, NULL);
	
	if (_item) {
		obs_sceneitem_remove(_item);
	}
	
	if (_source) {
		obs_source_release(_source);
	}
	
	if (_scene) {
		obs_source_remove(obs_scene_get_source(_scene));
		obs_scene_release(_scene);
	}
}
