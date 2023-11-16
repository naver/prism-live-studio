#ifndef QQRCODER_H
#define QQRCODER_H

#include <QByteArray>
#include <QDebug>
#include <QPainter>
#include <QTextOption>
#include "qrencode.h"

Q_DECLARE_METATYPE(QTextOption)

namespace QRCoderSpace {
extern const QString QRFont;
extern const QString QROption;
extern const QString QRWrapTopMargin;

extern const QString QRMaxHeightStr;
extern const QString QRMaxWidthStr;
extern const QString QRTipMaxheightStr;

}

class QQRCoder {
public:
	QQRCoder();

public:
	enum QR_MODE {
		MODE_NUL = QR_MODE_NUL,
		MODE_NUM = QR_MODE_NUM,
		MODE_AN = QR_MODE_AN,
		MODE_8 = QR_MODE_8,
		MODE_KANJI = QR_MODE_KANJI,
		MODE_STRUCTURE = QR_MODE_STRUCTURE,
		MODE_ECI = QR_MODE_ECI,
		MODE_FNC1FIRST = QR_MODE_FNC1FIRST,
		MODE_FNC1SECOND = QR_MODE_FNC1SECOND
	};

	enum QR_LEVEL { LEVEL_L = QR_ECLEVEL_L, LEVEL_M = QR_ECLEVEL_M, LEVEL_Q = QR_ECLEVEL_Q, LEVEL_H = QR_ECLEVEL_H };

	void setContent(const QByteArray &txt) { mContext = txt; }
	void setForeground(const QColor &color) { mForeground = color; }

	void setBackground(const QColor &color) { mBackground = color; }

	void setMargin(const int &margin) { this->mMargin = margin; }

	void setIsCacesensitive(bool isSen = true) { mIsCasesensitive = isSen; }

	void setQRVersion(int version) { mQRVersion = version; }

	void setLevel(int level) { mLevel = level; }

	void setCenterImage(const QString &path, qreal percent);
	void setCenterImage(const QImage &srcLogo, qreal percent);

	QImage paintImage(const QSize &outPutSize = QSize(300, 300));

	static QImage paintTextImage(const QByteArray &text, const QSize &outPutSize = QSize(300, 300));

	static QImage wrapImageWithTips(const QImage &src, const QString &tips, const QVariantMap &drawInfo);

private:
	QByteArray mContext;
	QColor mBackground;
	QColor mForeground;
	int mMargin;
	bool mIsCasesensitive;
	int mMode;
	int mLevel;
	int mQRVersion;
	float mPercent;
	QImage mLogo;
};

#endif // QQRCODER_H
