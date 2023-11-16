#pragma once

class QPixmap;
class QString;
class QThread;

class PLSNaverShoppingLIVEImageProcessFinished {
public:
	virtual ~PLSNaverShoppingLIVEImageProcessFinished() = default;

	virtual void processFinished(bool ok, QThread *thread, const QString &url, const QPixmap &normalPixmap, const QPixmap &selectedPixmap, const QPixmap &livePixmap,
				     const QPixmap &liveHoveredPixmap) = 0;
};
