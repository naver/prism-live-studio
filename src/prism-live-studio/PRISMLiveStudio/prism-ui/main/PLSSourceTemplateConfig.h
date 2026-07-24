#ifndef PLSSOURCETEMPLATECONFIG_H
#define PLSSOURCETEMPLATECONFIG_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QPoint>
#include <obs.h>

// Canvas aspect ratio category
enum class CanvasAspectRatio {
	Aspect16_9, // Widescreen (landscape)
	Aspect1_1,  // Square
	Aspect9_16  // Portrait
};

// Source guide tab index
enum class SourceGuideTab {
	Chatting = 0,    // "Source.Guide.Tab.Chatting"
	Game = 1,        // "Source.Guide.Tab.Game"
	Mobile = 2,      // "Source.Guide.Tab.Mobile"
	Vtuber = 3,      // "Source.Guide.Tab.Vtuber"
	Shopping = 4,    // "Source.Guide.Tab.Shopping"
	Presentation = 5 // "Source.Guide.Tab.Presentation"
};

// Size mode for source template
enum class SourceSizeMode {
	KeepOriginal,   // Keep source's original size (scale = 1.0)
	RelativeWidth,  // Width as relative value (0-1), height calculated to maintain aspect ratio
	RelativeHeight, // Height as relative value (0-1), width calculated to maintain aspect ratio
	RelativeBoth,   // Both width and height as relative values (0-1)
	FullScreen      // 100% of canvas (width=1.0, height=1.0)
};

// Source template configuration
// Position and size use relative values (0.0-1.0) based on canvas dimensions.
//
// (xRatio, yRatio) = canvas position (0..1) of the source's reference point. No conversion: pass directly to OBS.
// alignment = OBS alignment (which point of source is at that position):
//   LEFT|TOP, RIGHT|TOP, LEFT|BOTTOM, CENTER; LEFT only = left edge + vertical center; RIGHT only = right edge + vertical center.
struct SourceTemplateConfig {
	const char *sourceId;    // Source ID (e.g., PRISM_LENS_SOURCE_ID, BROWSER_SOURCE_ID)
	SourceSizeMode sizeMode; // How to calculate source size
	double widthRatio;       // Width ratio (0-1) when sizeMode is RelativeWidth or RelativeBoth
	double heightRatio;      // Height ratio (0-1) when sizeMode is RelativeHeight or RelativeBoth
	double xRatio;           // X position ratio (0-1), 0=left, 1=right (interpreted per alignment)
	double yRatio;           // Y position ratio (0-1), 0=top, 1=bottom (interpreted per alignment)
	uint32_t alignment;      // OBS alignment: which point of source is at (xRatio,yRatio)
	bool isBackground;       // Whether this is a background layer (lowest z-order)
};

// Get canvas aspect ratio category from current video settings
CanvasAspectRatio getCanvasAspectRatio();

// Apply template position and size to a scene item
void applyTemplatePosition(obs_sceneitem_t *item, const SourceTemplateConfig &config);

// Apply template positions to a list of sources added from guide view
// Returns list of unmatched source display names for user confirmation
QStringList applySourceTemplatePositions(const QStringList &sourceList, int tabIndex, QVector<obs_sceneitem_t *> &addedItems);

#endif // PLSSOURCETEMPLATECONFIG_H
