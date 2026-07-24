#include "PLSSourceTemplateConfig.h"
#include "window-basic-main.hpp"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "PLSAddSourceGuideView.h"
#include <cmath>

using namespace common;

#if defined(Q_OS_WIN)
#define PLATFORM_VIDEO_CAPTURE_SOURCE_ID OBS_DSHOW_SOURCE_ID
#elif defined(Q_OS_MACOS)
#define PLATFORM_VIDEO_CAPTURE_SOURCE_ID OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID
#endif

// Get canvas aspect ratio category from current video settings
// Figma templates use 16:9, 1:1, 9:16 as categories for widescreen, square, and portrait
CanvasAspectRatio getCanvasAspectRatio()
{
	obs_video_info ovi;
	obs_get_video_info(&ovi);
	uint32_t width = ovi.base_width;
	uint32_t height = ovi.base_height;

	if (width == 0 || height == 0) {
		return CanvasAspectRatio::Aspect16_9; // Default to widescreen
	}

	// Calculate aspect ratio (width / height)
	double aspect = (double)width / (double)height;

	// Categorize based on aspect ratio ranges:
	// - Widescreen (landscape): aspect > 1.3 → use 16:9 template
	// - Square: 0.7 <= aspect <= 1.3 → use 1:1 template
	// - Portrait: aspect < 0.7 → use 9:16 template
	if (aspect > 1.3) {
		return CanvasAspectRatio::Aspect16_9; // Widescreen
	} else if (aspect >= 0.7) {
		return CanvasAspectRatio::Aspect1_1; // Square
	} else {
		return CanvasAspectRatio::Aspect9_16; // Portrait
	}
}

// Apply browser source width/height so that with scale=1 the Transform Size shows target. Call after size calculation.
static void applyBrowserSourceSettings(obs_sceneitem_t *item, obs_source_t *source, double targetWidth, double targetHeight)
{
	(void)item;
	if (!pls_is_equal(obs_source_get_id(source), BROWSER_SOURCE_ID)) {
		return;
	}
	if (targetWidth < 1.0 || targetHeight < 1.0) {
		return;
	}
	obs_data_t *settings = obs_source_get_settings(source);
	if (settings) {
		obs_data_set_int(settings, "width", static_cast<int>(std::round(targetWidth)));
		obs_data_set_int(settings, "height", static_cast<int>(std::round(targetHeight)));
		obs_source_update(source, settings);
		obs_data_release(settings);
	}
}

// Apply template position and size to a scene item
// Position and size use relative values (0.0-1.0) based on canvas dimensions
void applyTemplatePosition(obs_sceneitem_t *item, const SourceTemplateConfig &config)
{
	if (!item) {
		return;
	}

	// Get actual canvas size
	obs_video_info ovi;
	obs_get_video_info(&ovi);
	uint32_t canvasWidth = ovi.base_width;
	uint32_t canvasHeight = ovi.base_height;

	if (canvasWidth == 0 || canvasHeight == 0) {
		return;
	}

	obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);

	// Get source dimensions (may be 0 for newly added or dynamic sources)
	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t sourceWidth = obs_source_get_width(source);
	uint32_t sourceHeight = obs_source_get_height(source);

	// Use source-reported size only. If it is 0, skip applying size/position.
	if (sourceWidth == 0 || sourceHeight == 0) {
		const char *id = obs_source_get_id(source);
		if (pls_is_equal(id, PRISM_TIMER_SOURCE_ID)) {
			// Timer source may report 0 until its internal browser initializes.
			sourceWidth = 800;
			sourceHeight = 600;
		} else if (pls_is_equal(id, BROWSER_SOURCE_ID)) {
			// Browser source may report 0 until it finishes initialization.
			sourceWidth = 800;
			sourceHeight = 600;
		} else if (QString(id).startsWith(SLIDESHOW_SOURCE_ID)) {
			// Slideshow defaults to current canvas size.
			sourceWidth = canvasWidth;
			sourceHeight = canvasHeight;
		} else if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
			// Lens may report 0 until camera is active (timing can differ on Win vs Mac).
			// Use fixed 1280x720 so both platforms get same aspect-ratio calculation; avoid canvas size
			sourceWidth = 1280;
			sourceHeight = 720;
		} else {
			return;
		}
	}

	// No offset for shared positions - sources can overlap

	// Calculate source size based on size mode
	float scaleX = 1.0f, scaleY = 1.0f;
	double targetWidth = 0.0, targetHeight = 0.0;

	switch (config.sizeMode) {
	case SourceSizeMode::KeepOriginal:
		// Keep original size, scale = 1.0
		scaleX = 1.0f;
		scaleY = 1.0f;
		targetWidth = sourceWidth;
		targetHeight = sourceHeight;
		break;

	case SourceSizeMode::RelativeWidth:
		// Width matches ratio, height calculated to maintain aspect ratio
		targetWidth = config.widthRatio * canvasWidth;
		targetHeight = targetWidth * sourceHeight / sourceWidth;
		scaleX = (float)(targetWidth / sourceWidth);
		scaleY = scaleX; // Maintain aspect ratio
		break;

	case SourceSizeMode::RelativeHeight: {
		// Height matches ratio, width calculated to maintain aspect ratio (sourceHeight never 0 here: see fallback above)
		targetHeight = config.heightRatio * canvasHeight;
		targetWidth = targetHeight * (double)sourceWidth / (double)sourceHeight;
		scaleY = (float)(targetHeight / sourceHeight);
		scaleX = scaleY; // Maintain aspect ratio
		break;
	}

	case SourceSizeMode::RelativeBoth:
		// Both width and height match ratios
		targetWidth = config.widthRatio * canvasWidth;
		targetHeight = config.heightRatio * canvasHeight;
		scaleX = (float)(targetWidth / sourceWidth);
		scaleY = (float)(targetHeight / sourceHeight);
		break;

	case SourceSizeMode::FullScreen:
		// 100% of canvas
		targetWidth = canvasWidth;
		targetHeight = canvasHeight;
		scaleX = (float)(targetWidth / sourceWidth);
		scaleY = (float)(targetHeight / sourceHeight);
		break;
	}

	// Lens/LensMobile: when source resolution != canvas (prism) resolution, show at original resolution (scale=1).
	// Browser (RelativeHeight): set source to target size later, use scale=1 so Transform Size = target.
	const char *sourceId = obs_source_get_id(source);
	if (pls_is_equal(sourceId, BROWSER_SOURCE_ID) && config.sizeMode == SourceSizeMode::RelativeHeight && config.heightRatio > 0.0) {
		scaleX = 1.0f;
		scaleY = 1.0f;
	}

	// Figma (xRatio, yRatio) is the canvas coordinate of the source's reference point.
	// OBS alignment specifies which point of the source that is (left/right/center, top/bottom/center).
	// No conversion: pass (xRatio*cw, yRatio*ch) and alignment directly.
	float posX = (float)(config.xRatio * canvasWidth);
	float posY = (float)(config.yRatio * canvasHeight);

	vec2_set(&info.pos, posX, posY);
	vec2_set(&info.scale, scaleX, scaleY);
	info.alignment = config.alignment;

	info.bounds_type = OBS_BOUNDS_NONE;

	// Call after size calculation: set Browser source width/height so that with scale=1 the Transform Size matches target.
	applyBrowserSourceSettings(item, source, targetWidth, targetHeight);

	// Apply transform (scale=1 for Browser so displayed size = source size = target)
	obs_sceneitem_set_info2(item, &info);
}

// Template configuration data structure
struct SourceTemplate {
	SourceGuideTab tab;
	CanvasAspectRatio aspectRatio;
	QVector<SourceTemplateConfig> sources;
};

// Initialize all 18 templates from Figma. Convention:
// - (xRatio, yRatio) = canvas position (0..1) of the source's REFERENCE POINT (same as OBS).
// - alignment = OBS alignment: which point of the source is at (xRatio,yRatio).
//   e.g. RIGHT|TOP => reference point = top-right corner; X.0.9478,Y.0.0928 = that corner at (94.78%w, 9.28%h).
// - Size: widthRatio/heightRatio per sizeMode (RelativeWidth/RelativeHeight/RelativeBoth); FullScreen = 100%.
static QVector<SourceTemplate> initializeTemplates()
{
	QVector<SourceTemplate> templates;

	// Template index = tabIndex * 3 + aspectRatioIndex. Source order = initSourceTabList() per tab.
	// Tab: Chatting=0, Game=1, Mobile=2, Vtuber=3, Shopping=4, Presentation=5. Aspect: 16:9=0, 1:1=1, 9:16=2.

	// Chatting Tab - 16:9 (Full Screen)
	// Source order from initSourceTabList: PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2, WINDOW, BROWSER, PRISM_TEXT_TEMPLATE, IMAGE, BGM, PRISM_STICKER
	SourceTemplate chatting_16_9;
	chatting_16_9.tab = SourceGuideTab::Chatting;
	chatting_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	chatting_16_9.sources = QVector<SourceTemplateConfig>{
		{PRISM_TEXT_TEMPLATE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false}, // Bottom, X.0, keep default size
		{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},             // Figma: w.0.3156 너비 맞춤, Center Align; h = w*aspect
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false}, // Figma: X.0.0522, Y.Center → left edge at 5.22%, vertical center
#if defined(Q_OS_WIN)
		{WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.3349 높이 맞춤
#elif defined(Q_OS_MACOS)
		{OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#endif
		{BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.3349 높이 맞춤
		{IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},         // Figma: h.0.3349 높이 맞춤 / X.1, Y.0
		{BGM_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PRISM_LENS_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::FullScreen, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(chatting_16_9);

	// Chatting Tab - 1:1 (Two - Custom) — Figma: left half / right half; Chat left center, Text bottom, Browser/Window right
	// Source order: PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2, WINDOW, BROWSER, PRISM_TEXT_TEMPLATE, IMAGE, BGM, PRISM_STICKER
	SourceTemplate chatting_1_1;
	chatting_1_1.tab = SourceGuideTab::Chatting;
	chatting_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	chatting_1_1.sources =
		QVector<SourceTemplateConfig>{{PRISM_TEXT_TEMPLATE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
					      {PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false}, // Figma: w.0.3156 너비 맞춤, Center Align
					      {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
#if defined(Q_OS_WIN)
					      {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.3349 높이 맞춤
#elif defined(Q_OS_MACOS)
					      {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#endif
					      {BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
					      {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.3349 높이 맞춤 / X.1, Y.0
					      {BGM_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
					      {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
					      {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
					      {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(chatting_1_1);

	// Chatting Tab - 9:16 (Two - Portrait Layout) — Figma: two vertical strips; Chat left center, Text bottom, Browser/Window right
	// Source order: PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2, WINDOW, BROWSER, PRISM_TEXT_TEMPLATE, IMAGE, BGM, PRISM_STICKER
	SourceTemplate chatting_9_16;
	chatting_9_16.tab = SourceGuideTab::Chatting;
	chatting_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	chatting_9_16.sources = QVector<SourceTemplateConfig>{
		{PRISM_TEXT_TEMPLATE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
		{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false}, // Figma: w.0.3156 너비 맞춤, Center Align
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
#if defined(Q_OS_WIN)
		{WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.16745 높이 맞춤
#elif defined(Q_OS_MACOS)
		{OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#endif
		{BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false}, // Figma: h.0.16745 높이 맞춤 / X.1, Y.0
		{BGM_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
		{PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(chatting_9_16);

	// Game Tab - 16:9 — Figma: game fullscreen; overlays bottom-right (h.0.31296); Chat right vertical center
	// Source order: GAME, WINDOW, PRISM_MONITOR, PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2
	SourceTemplate game_16_9;
	game_16_9.tab = SourceGuideTab::Game;
	game_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	game_16_9.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.9478, 0.5, OBS_ALIGN_RIGHT,
							   false}, // Figma: X.0.9478, Y.Center → right edge, vertical center
							  {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
							  {GAME_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							  {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							  {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
							  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#endif
							  {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(game_16_9);

	// Game Tab - 1:1 — Figma: game main; overlays bottom-right; Chat right vertical center
	SourceTemplate game_1_1;
	game_1_1.tab = SourceGuideTab::Game;
	game_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	game_1_1.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.9478, 0.5, OBS_ALIGN_RIGHT,
							  false}, // Figma: X.0.9478, Y.Center → right edge, vertical center
							 {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							 {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
							 {GAME_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							 {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							 {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
							 {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#endif
							 {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(game_1_1);

	// Game Tab - 9:16 — Figma: game main; overlays bottom-right; Chat right vertical center
	SourceTemplate game_9_16;
	game_9_16.tab = SourceGuideTab::Game;
	game_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	game_9_16.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.9478, 0.5, OBS_ALIGN_RIGHT,
							   false}, // Figma: X.0.9478, Y.Center → right edge, vertical center
							  {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
							  {GAME_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							  {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							  {WINDOW_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
							  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
#endif
							  {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(game_9_16);

	// Mobile Tab - 16:9 — Figma: mobile fullscreen; Lens/Video overlay bottom-right; Chat right vertical center
	// Source order: PRISM_LENS_MOBILE, PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2
	SourceTemplate mobile_16_9;
	mobile_16_9.tab = SourceGuideTab::Mobile;
	mobile_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	mobile_16_9.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							    {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							    {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							    {PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							    {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(mobile_16_9);

	// Mobile Tab - 1:1 — Figma: mobile main; Lens/Video overlay bottom-right; Chat right vertical center
	SourceTemplate mobile_1_1;
	mobile_1_1.tab = SourceGuideTab::Mobile;
	mobile_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	mobile_1_1.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							   {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							   {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.4082, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							   {PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							   {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(mobile_1_1);

	// Mobile Tab - 9:16 — Figma: mobile main; Lens/Video overlay bottom-right; Chat right vertical center
	SourceTemplate mobile_9_16;
	mobile_9_16.tab = SourceGuideTab::Mobile;
	mobile_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	mobile_9_16.sources = QVector<SourceTemplateConfig>{{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							    {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							    {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							    {PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							    {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(mobile_9_16);

	// Vtuber Tab - 16:9 — Figma: Spout/Screen fullscreen; Chat left vertical center; overlays top-left
	// Source order (initSourceTabList): Win: Spout, LensMobile, Audio, Chat, Bkg, Game, Window, Monitor, Sticker. Mac: ScreenCapture, LensMobile, Audio, Chat, Bkg.
	SourceTemplate vtuber_16_9;
	vtuber_16_9.tab = SourceGuideTab::Vtuber;
	vtuber_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	vtuber_16_9.sources = QVector<SourceTemplateConfig>{
#if defined(Q_OS_WIN)
		{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false}, // Figma: w.0.3156 너비 맞춤, Center Align
		{WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_INPUT_SPOUT_CAPTURE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#elif defined(Q_OS_MACOS)
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#endif
	};
	templates.append(vtuber_16_9);

	// Vtuber Tab - 1:1 — Figma: Spout/Screen main; Chat left vertical center; overlays top-left
	SourceTemplate vtuber_1_1;
	vtuber_1_1.tab = SourceGuideTab::Vtuber;
	vtuber_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	vtuber_1_1.sources = QVector<SourceTemplateConfig>{
#if defined(Q_OS_WIN)
		{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_INPUT_SPOUT_CAPTURE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#elif defined(Q_OS_MACOS)
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#endif
	};
	templates.append(vtuber_1_1);

	// Vtuber Tab - 9:16 — Figma: Spout/Screen main; Chat left vertical center; overlays top-left
	SourceTemplate vtuber_9_16;
	vtuber_9_16.tab = SourceGuideTab::Vtuber;
	vtuber_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	vtuber_9_16.sources = QVector<SourceTemplateConfig>{
#if defined(Q_OS_WIN)
		{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_INPUT_SPOUT_CAPTURE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#elif defined(Q_OS_MACOS)
		{PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
		{PRISM_LENS_MOBILE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
		{OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
		{PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
		{AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}
#endif
	};
	templates.append(vtuber_9_16);

	// Shopping Tab - 16:9 — Figma: same layout logic as Chatting (Lens/Video full; Chat left center; Browser/Window right; Text bottom)
	// Source order: PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT, PRISM_CHATV2, BROWSER, WINDOW, IMAGE, SLIDESHOW, GDIP_TEXT, PRISM_TIMER, PRISM_STICKER
	SourceTemplate shopping_16_9;
	shopping_16_9.tab = SourceGuideTab::Shopping;
	shopping_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	shopping_16_9.sources =
		QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false}, // Figma: w.0.3156 너비 맞춤, Center Align
					      {PRISM_TIMER_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
					      {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
					      {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
					      {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#endif
					      {BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
					      {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
					      {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
					      {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
					      {PRISM_LENS_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
					      {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
					      {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(shopping_16_9);

	// Shopping Tab - 1:1 — Figma: same layout logic as Chatting 1:1
	SourceTemplate shopping_1_1;
	shopping_1_1.tab = SourceGuideTab::Shopping;
	shopping_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	shopping_1_1.sources = QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							     {PRISM_TIMER_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							     {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
							     {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
							     {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#endif
							     {BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							     {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							     {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							     {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
							     {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							     {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							     {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(shopping_1_1);

	// Shopping Tab - 9:16 — Figma: same layout logic as Chatting 9:16
	SourceTemplate shopping_9_16;
	shopping_9_16.tab = SourceGuideTab::Shopping;
	shopping_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	shopping_9_16.sources = QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
							      {PRISM_TIMER_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false},
							      {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
							      {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
							      {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP,
							       false},
#endif
							      {BROWSER_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							      {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							      {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
							      {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
							      {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							      {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
							      {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(shopping_9_16);

	// Presentation Tab - 16:9 — Figma: main content fullscreen; Chat left vertical center; overlays as needed
	// Source order (initSourceTabList): PRISM_STICKER, GDIP_TEXT, WINDOW, PRISM_MONITOR, SLIDESHOW, IMAGE, PRISM_CHATV2, PRISM_LENS, PLATFORM_VIDEO_CAPTURE, AUDIO_INPUT
	SourceTemplate presentation_16_9;
	presentation_16_9.tab = SourceGuideTab::Presentation;
	presentation_16_9.aspectRatio = CanvasAspectRatio::Aspect16_9;
	presentation_16_9.sources = QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								  {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
								  {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
								  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP,
								   false},
#endif
								  {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
								  {PRISM_LENS_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								  {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(presentation_16_9);

	// Presentation Tab - 1:1 — Figma: main content half; Chat left vertical center; Text bottom
	SourceTemplate presentation_1_1;
	presentation_1_1.tab = SourceGuideTab::Presentation;
	presentation_1_1.aspectRatio = CanvasAspectRatio::Aspect1_1;
	presentation_1_1.sources = QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								 {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
								 {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								 {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
								 {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP,
								  false},
#endif
								 {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								 {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.3349, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								 {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
								 {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								 {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 1.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								 {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(presentation_1_1);

	// Presentation Tab - 9:16 — Figma: main content; Chat left vertical center; Text bottom
	SourceTemplate presentation_9_16;
	presentation_9_16.tab = SourceGuideTab::Presentation;
	presentation_9_16.aspectRatio = CanvasAspectRatio::Aspect9_16;
	presentation_9_16.sources = QVector<SourceTemplateConfig>{{PRISM_STICKER_SOURCE_ID, SourceSizeMode::RelativeWidth, 0.3156, 0.0, 0.5, 0.5, OBS_ALIGN_CENTER, false},
								  {GDIP_TEXT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
#if defined(Q_OS_WIN)
								  {WINDOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {PRISM_MONITOR_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8955, 0.1855, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
#elif defined(Q_OS_MACOS)
								  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.8433, 0.2783, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP,
								   false},
#endif
								  {SLIDESHOW_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 0.9478, 0.0928, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {IMAGE_SOURCE_ID, SourceSizeMode::RelativeHeight, 0.0, 0.16745, 1.0, 0.0, OBS_ALIGN_RIGHT | OBS_ALIGN_TOP, false},
								  {PRISM_CHATV2_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0522, 0.5, OBS_ALIGN_LEFT, false},
								  {PRISM_LENS_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
								  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, SourceSizeMode::RelativeWidth, 1.0, 0.0, 0.0, 1.0, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM, false},
								  {AUDIO_INPUT_SOURCE_ID, SourceSizeMode::KeepOriginal, 0.0, 0.0, 0.0, 0.0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, false}};
	templates.append(presentation_9_16);

	return templates;
}

// Get template configuration for given tab and aspect ratio
static const SourceTemplate *getTemplate(int tabIndex, CanvasAspectRatio aspectRatio)
{
	static QVector<SourceTemplate> templates = initializeTemplates();

	// Find matching template
	for (const auto &tmpl : templates) {
		if (tmpl.tab == (SourceGuideTab)tabIndex && tmpl.aspectRatio == aspectRatio) {
			return &tmpl;
		}
	}

	return nullptr;
}

// Find config for a source by source ID
static const SourceTemplateConfig *findConfigForSource(const SourceTemplate *tmpl, const QString &sourceId)
{
	if (!tmpl) {
		return nullptr;
	}

	for (const auto &config : tmpl->sources) {
		if (QString::fromUtf8(config.sourceId) == sourceId) {
			return &config;
		}
	}

	return nullptr;
}

// Apply template positions to a list of sources added from guide view
QStringList applySourceTemplatePositions(const QStringList &sourceList, int tabIndex, QVector<obs_sceneitem_t *> &addedItems)
{
	QStringList unmatchedSources;

	// Get canvas aspect ratio
	CanvasAspectRatio aspectRatio = getCanvasAspectRatio();

	// Get template configuration
	const SourceTemplate *tmpl = getTemplate(tabIndex, aspectRatio);
	if (!tmpl) {
		// No template found, return all sources as unmatched
		for (const auto &sourceId : sourceList) {
			unmatchedSources.append(sourceId);
		}
		return unmatchedSources;
	}

	// Get canvas size for printing
	obs_video_info ovi;
	obs_get_video_info(&ovi);

	// Apply positions in the order sources were added
	for (int i = 0; i < sourceList.size() && i < addedItems.size(); i++) {
		const QString &sourceId = sourceList[i];

		if (sourceId.isEmpty()) {
			continue;
		}

		// Find corresponding template config by source ID
		const SourceTemplateConfig *config = findConfigForSource(tmpl, sourceId);
		if (!config) {
			unmatchedSources.append(sourceId);
			continue;
		}

		obs_sceneitem_t *item = addedItems[i];
		if (!item) {
			continue;
		}

		applyTemplatePosition(item, *config);
		// Ensure visibility after applying transform (in case it was not set or was cleared)
		obs_sceneitem_set_visible(item, true);
		obs_sceneitem_release(item);
	}

	return unmatchedSources;
}