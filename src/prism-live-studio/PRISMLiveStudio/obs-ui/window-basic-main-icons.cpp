#include <window-basic-main.hpp>
#include <pls/pls-source.h>

extern QString GetIconKey(obs_icon_type type);
extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);
#define NORMALICONPATH QStringLiteral(":/resource/images/add-source-view/icon-source-%1-normal.svg")
#define SELECTICONPATH QStringLiteral(":/resource/images/add-source-view/icon-source-%1-select.svg")

QIcon OBSBasic::GetSourceIcon(const char *id) const
{
	obs_icon_type type = obs_source_get_icon_type(id);

	switch (type) {
	case OBS_ICON_TYPE_IMAGE:
		return GetImageIcon();
	case OBS_ICON_TYPE_COLOR:
		return GetColorIcon();
	case OBS_ICON_TYPE_SLIDESHOW:
		return GetSlideshowIcon();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return GetAudioInputIcon();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return GetAudioOutputIcon();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return GetDesktopCapIcon();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return GetWindowCapIcon();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return GetGameCapIcon();
	case OBS_ICON_TYPE_CAMERA:
		return GetCameraIcon();
	case OBS_ICON_TYPE_TEXT:
		return GetTextIcon();
	case OBS_ICON_TYPE_MEDIA:
		return GetMediaIcon();
	case OBS_ICON_TYPE_BROWSER:
		return GetBrowserIcon();
	case OBS_ICON_TYPE_CUSTOM:
		return GetDefaultIcon();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return GetAudioProcessOutputIcon();
	default:
		break;
	}

	if (QIcon icon; GetSourceIcon(icon, type)) {
		return icon;
	}
	return GetDefaultIcon();
}
bool OBSBasic::GetSourceIcon(QIcon &icon, int type) const
{
	return false;
}

void OBSBasic::SetImageIcon(const QIcon &icon)
{
	imageIcon = icon;
}

void OBSBasic::SetColorIcon(const QIcon &icon)
{
	colorIcon = icon;
}

void OBSBasic::SetSlideshowIcon(const QIcon &icon)
{
	slideshowIcon = icon;
}

void OBSBasic::SetAudioInputIcon(const QIcon &icon)
{
	audioInputIcon = icon;
}

void OBSBasic::SetAudioOutputIcon(const QIcon &icon)
{
	audioOutputIcon = icon;
}

void OBSBasic::SetDesktopCapIcon(const QIcon &icon)
{
	desktopCapIcon = icon;
}

void OBSBasic::SetWindowCapIcon(const QIcon &icon)
{
	windowCapIcon = icon;
}

void OBSBasic::SetGameCapIcon(const QIcon &icon)
{
	gameCapIcon = icon;
}

void OBSBasic::SetCameraIcon(const QIcon &icon)
{
	cameraIcon = icon;
}

void OBSBasic::SetTextIcon(const QIcon &icon)
{
	textIcon = icon;
}

void OBSBasic::SetMediaIcon(const QIcon &icon)
{
	mediaIcon = icon;
}

void OBSBasic::SetBrowserIcon(const QIcon &icon)
{
	browserIcon = icon;
}

void OBSBasic::SetGroupIcon(const QIcon &icon)
{
	groupIcon = icon;
}

void OBSBasic::SetSceneIcon(const QIcon &icon)
{
	sceneIcon = icon;
}

void OBSBasic::SetDefaultIcon(const QIcon &icon)
{
	defaultIcon = icon;
}

void OBSBasic::SetAudioProcessOutputIcon(const QIcon &icon)
{
	audioProcessOutputIcon = icon;
}

QIcon OBSBasic::GetImageIcon() const
{
	return imageIcon;
}

QIcon OBSBasic::GetColorIcon() const
{
	return colorIcon;
}

QIcon OBSBasic::GetSlideshowIcon() const
{
	return slideshowIcon;
}

QIcon OBSBasic::GetAudioInputIcon() const
{
	return audioInputIcon;
}

QIcon OBSBasic::GetAudioOutputIcon() const
{
	return audioOutputIcon;
}

QIcon OBSBasic::GetDesktopCapIcon() const
{
	return desktopCapIcon;
}

QIcon OBSBasic::GetWindowCapIcon() const
{
	return windowCapIcon;
}

QIcon OBSBasic::GetGameCapIcon() const
{
	return gameCapIcon;
}

QIcon OBSBasic::GetCameraIcon() const
{
	return cameraIcon;
}

QIcon OBSBasic::GetTextIcon() const
{
	return textIcon;
}

QIcon OBSBasic::GetMediaIcon() const
{
	return mediaIcon;
}

QIcon OBSBasic::GetBrowserIcon() const
{
	return browserIcon;
}

QIcon OBSBasic::GetGroupIcon() const
{
	return groupIcon;
}

QIcon OBSBasic::GetSceneIcon() const
{
	return sceneIcon;
}

QPixmap OBSBasic::GetSourcePixmap(const QString &id, bool selected, QSize size)
{
	QString iconKey;
	if (id == "scene") {
		iconKey = "scene";
	} else if (id == "group") {
		iconKey = "group";
	} else {
		iconKey = GetIconKey(obs_source_get_icon_type(id.toStdString().c_str())).toLower();
	}

	QPixmap pix;
	QString path = selected ? SELECTICONPATH : NORMALICONPATH;
	loadPixmap(pix, path.arg(iconKey), size);
	return pix;
}

QIcon OBSBasic::GetDefaultIcon() const
{
	return defaultIcon;
}

QIcon OBSBasic::GetAudioProcessOutputIcon() const
{
	return audioProcessOutputIcon;
}
