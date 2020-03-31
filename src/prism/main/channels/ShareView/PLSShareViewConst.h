#ifndef SHAREVIEWCONST_H
#define SHAREVIEWCONST_H
#include <QString>
#include <QSize>
namespace shareView {

#define SHARE_TYPE "shareType"
const QString DEFAULT_URL = "";
#define DEFAULT_HEAD_PIC "icon-source-img-off.png"
#define STEAM_PIC "stremsetting-vlive-s.png"
#define YOUTUBE_PIC "img-youtube-profile.png"
#define LIVE_PIC "badge-live-on.png"
#define FACEBOOK_HOVER_PIC "icon-share-fb-over.png"
#define FACEBOOK_PIC "icon-share-fb-normal.png"

#define IMAGE_PATH ":/Images/skin/"

#define PIC_PATH(pic)                                    \
	{                                                \
		QString(IMAGE_PATH) + "/" + QString(pic) \
	}

#define BUTTONFLAG "buttonFlag"

const QSize g_headerSize(34, 34);
const QSize g_platformSize(17, 17);
const QSize g_buttonSize(213, 60);
const QSize g_iconSize(22, 22);
} // namespace shareView

#endif // SHAREVIEWCONST_H
