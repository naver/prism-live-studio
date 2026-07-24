#pragma once

#include <QtGlobal>
#include <QString>
#include <QWidget>

class QLabel;
class QJsonObject;
class QShowEvent;
class QEvent;

class PLSRecentLoginBadge : public QWidget {
public:
	explicit PLSRecentLoginBadge(QWidget *parent = nullptr);
	void setBadgeText(const QString &text);

protected:
	void showEvent(QShowEvent *event) override;
	void changeEvent(QEvent *event) override;

private:
	void reloadSvg();
	QLabel *m_iconLabel{nullptr};
	QString m_text;
};

/** Persists last successful Prism login channel for the "recent login" badge (QSettings: Win registry / macOS plist). */
class PLSRecentLoginStore {
public:
	/** Not a PRISMLOGINTYPE; reserved for email + sign-up-email flows. */
	static constexpr qint32 KindEmail = 100;

	static void recordSuccess(qint32 kind);
	static void clear();
	static void read(qint32 &outKind, qint64 &outEpochSec);
	static bool isStaleOrInvalid(qint32 kind, qint64 epochSec);
	static void storeByCacheFile(const QJsonObject &userObj, const QString &userFilePath);

private:
	static void getLensCacheDirectValue(qint32 &outKind, qint64 &outEpochSec, const QJsonObject &userObj);
	static void getLensCacheAuthTypeValue(qint32 &outKind, qint64 &outEpochSec, const QJsonObject &userObj, const QString &camUserFilePath);

private:
	static constexpr auto KeyKind = "recent_kind";
	static constexpr auto KeyEpoch = "recent_last_success_epoch";
	static constexpr qint64 Sec30Days = 30LL * 24 * 3600;
};
