#ifndef PLS_NOTICE_UPDATE_TYPES_HPP
#define PLS_NOTICE_UPDATE_TYPES_HPP

#include <QDateTime>
#include <QString>

enum class PLSNoticeProvider { B2B, PRISM, UPDATE };

enum class PLSNoticeCategory { Notice, Update };

enum class PLSNoticeFilter { All, PrismOnly, B2BOnly };

enum class PLSLoadState { Idle, Loading, Success, Empty, Failed };

struct PLSNoticeUpdateItem {
	QString id;
	PLSNoticeProvider provider{PLSNoticeProvider::PRISM};
	PLSNoticeCategory category{PLSNoticeCategory::Notice};
	QString providerName;
	QString title;
	qint64 publishAtMs{0};
	bool unread{true};

	QString contentPopUrl;
	QString contentDetailUrl;
	qint64 startAtMs{0};
	qint64 endAtMs{0};
	QString detailType;
	bool imageIncluded{false};
	bool isEmpty() const { return id.isEmpty(); }
};

#endif // PLS_NOTICE_UPDATE_TYPES_HPP
