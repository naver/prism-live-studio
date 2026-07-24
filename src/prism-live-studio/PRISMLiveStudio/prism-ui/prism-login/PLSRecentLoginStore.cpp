#include "PLSRecentLoginStore.hpp"

#include "libutils-api.h"
#include "pls-common-define.hpp"

#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QRectF>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QShowEvent>
#include <QtMath>
#include <QVBoxLayout>
#include "login-select-platform-view.hpp"
#include "liblog.h"

namespace {

QString recentLoginBadgeSvgResource()
{
	const QString loc = pls_get_locale();
	QString lang = loc.section(QRegularExpression(QStringLiteral("\\W+")), 0, 0).trimmed().toLower();
	static const QStringList supported = {QStringLiteral("ko"), QStringLiteral("ja"), QStringLiteral("es"), QStringLiteral("id"), QStringLiteral("pt"),
					      QStringLiteral("vi"), QStringLiteral("en")};
	if (!supported.contains(lang)) {
		lang = QStringLiteral("en");
	}
	return QStringLiteral(":/resource/images/prism-login/img_recentlogin_%1.svg").arg(lang);
}

} // namespace

PLSRecentLoginBadge::PLSRecentLoginBadge(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_iconLabel = new QLabel(this);
	m_iconLabel->setAttribute(Qt::WA_TranslucentBackground);
	m_iconLabel->setScaledContents(false);
	layout->addWidget(m_iconLabel);

	reloadSvg();
}

void PLSRecentLoginBadge::setBadgeText(const QString &text)
{
	if (m_text == text) {
		return;
	}
	m_text = text;
	reloadSvg();
}

void PLSRecentLoginBadge::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	reloadSvg();
}

void PLSRecentLoginBadge::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange || event->type() == QEvent::DevicePixelRatioChange) {
		reloadSvg();
	}
	QWidget::changeEvent(event);
}

void PLSRecentLoginBadge::reloadSvg()
{
	if (!m_iconLabel) {
		return;
	}

	const QString path = recentLoginBadgeSvgResource();
	QSvgRenderer renderer;
	if (!renderer.load(path)) {
		renderer.load(QStringLiteral(":/resource/images/prism-login/img_recentlogin_en.svg"));
	}
	if (!renderer.isValid()) {
		setFixedSize(1, 1);
		m_iconLabel->clear();
		return;
	}

	const QSize svgSize = renderer.defaultSize();
	if (!svgSize.isValid() || svgSize.isEmpty()) {
		setFixedSize(1, 1);
		m_iconLabel->clear();
		return;
	}

	const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
	const QSize pixSize(qCeil(svgSize.width() * dpr), qCeil(svgSize.height() * dpr));
	QPixmap pix(pixSize);
	pix.fill(Qt::transparent);

	{
		QPainter painter(&pix);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
		renderer.render(&painter, QRectF(0, 0, pixSize.width(), pixSize.height()));
	}
	pix.setDevicePixelRatio(dpr);
	m_iconLabel->setPixmap(pix);
	setFixedSize(svgSize);
}

void PLSRecentLoginStore::recordSuccess(qint32 kind)
{
	if (kind <= 0) {
		return;
	}
	pls_set_qsetting_value(KeyKind, kind);
	pls_set_qsetting_value(KeyEpoch, QDateTime::currentSecsSinceEpoch());
}

void PLSRecentLoginStore::clear()
{
	pls_set_qsetting_value(KeyKind, 0);
	pls_set_qsetting_value(KeyEpoch, 0);
}

void PLSRecentLoginStore::read(qint32 &outKind, qint64 &outEpochSec)
{
	outKind = pls_get_qsetting_value(KeyKind, 0).toInt();
	outEpochSec = pls_get_qsetting_value(KeyEpoch, 0).toLongLong();
}

bool PLSRecentLoginStore::isStaleOrInvalid(qint32 kind, qint64 epochSec)
{
	if (kind == 0 || epochSec <= 0)
		return true;
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	return (now - epochSec) > Sec30Days;
}

void PLSRecentLoginStore::storeByCacheFile(const QJsonObject &userObj, const QString &userFilePath)
{
	qint32 kind = 0;
	qint64 epochSec = 0;

	getLensCacheDirectValue(kind, epochSec, userObj);
	if (kind == 0 || epochSec <= 0) {
		kind = 0;
		epochSec = 0;
		getLensCacheAuthTypeValue(kind, epochSec, userObj, userFilePath);
	}
	PLS_INFO("UserInfo", "get recent info. kind:%d epochSec:%lld", kind, epochSec);
	if (!isStaleOrInvalid(kind, epochSec)) {
		pls_set_qsetting_value(KeyKind, kind);
		pls_set_qsetting_value(KeyEpoch, epochSec);
	}
}

void PLSRecentLoginStore::getLensCacheDirectValue(qint32 &outKind, qint64 &outEpochSec, const QJsonObject &userObj)
{
	outKind = pls_get_attr<qint32>(userObj, KeyKind, 0);
	outEpochSec = pls_get_attr<qint64>(userObj, KeyEpoch, 0);
}

void PLSRecentLoginStore::getLensCacheAuthTypeValue(qint32 &outKind, qint64 &outEpochSec, const QJsonObject &userObj, const QString &userFilePath)
{
	//"authType": "ncp","authType": "google"
	const QString authType = userObj.value(common::LOGIN_USERINFO_AUTHTYPE).toString().trimmed();
	static const struct {
		QString name;
		qint32 kind;
	} kTable[] = {{QStringLiteral("facebook"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::Facebook)}, //
		      {QStringLiteral("google"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::Google)},     //
		      {QStringLiteral("twitch"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::Twitch)},
		      {QStringLiteral("naver"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::NAVER)}, //
		      {QStringLiteral("apple"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::Apple)},
		      {QStringLiteral("line"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::LINE)}, //
		      {QStringLiteral("ncp"), static_cast<qint32>(PLSSelectLoginPlatformView::PRISMLOGINTYPE::NAVER_Cloud_B2B)},
		      {QStringLiteral("email"), static_cast<qint32>(PLSRecentLoginStore::KindEmail)}};

	for (const auto &e : kTable) {
		if (authType.compare(e.name, Qt::CaseInsensitive) == 0) {
			outKind = e.kind;
			break;
		}
	}

	if (outKind <= 0)
		return;

	QFileInfo fi(userFilePath);
	if (fi.exists())
		outEpochSec = fi.birthTime().toSecsSinceEpoch();
}
