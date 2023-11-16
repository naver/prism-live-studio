#include "PLSShareSourceItem.h"

#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
#include <QPainter>
#include <QUrl>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "libui.h"
#include "pls-channel-const.h"

using namespace ChannelData;

constexpr int maxUrlLenth = 259;

static inline QPoint operator+(const QPoint &pt, const QSize &sz)
{
	return QPoint(pt.x() + sz.width(), pt.y() + sz.height());
}

PLSShareSourceItem::PLSShareSourceItem(QWidget *parent) : PLSDialogView(parent)
{
	ui = pls_new<Ui::ShareSourceItem>();
	setupUi(ui);
	setFixedSize(QSize(480, 369));
#if defined(Q_OS_MACOS)
	this->setHasCloseButton(true);
	this->setHasMinButton(false);
	this->setHasMaxResButton(false);
	this->setWindowTitle(tr("Channels.ShareBroadcast"));
	ui->CloseBtn->hide();
#else
	this->setHasCaption(false);
	this->setHasHLine(false);
#endif

	pls_add_css(this, {"PLSShareSourceItem"});
	connect(ui->CopyBtn, &QPushButton::toggled, ui->UrlComplexFrame, &CheckedFrame::setChecked);
	connect(ui->CopyBtn, &QPushButton::clicked, this, &PLSShareSourceItem::onCopyPressed);
	connect(ui->GotoBtn, SIGNAL(clicked()), this, SLOT(openUrl()));
	connect(ui->OpenUrlTwitter, SIGNAL(clicked()), this, SLOT(openUrl()));
	connect(ui->OpenUrlFaceBook, SIGNAL(clicked()), this, SLOT(openUrl()));

	ui->UserIconLabel->installEventFilter(this);
	this->installEventFilter(this);
	auto centerShow = [this, parent]() { move(parent->pos() + (parent->size() - size()) / 2); };

	centerShow();
}

PLSShareSourceItem::~PLSShareSourceItem()
{
	delete ui;
}

void PLSShareSourceItem::initInfo(const QVariantMap &source)
{
	mSource = source;

	auto disName = getInfo(mSource, g_displayLine1);
	QString disElidName = getElidedText(ui->DisnameLabel, disName, ui->DisnameLabel->contentsRect().width());
	ui->DisnameLabel->setText(disElidName);
	auto urlStr = getInfo(mSource, g_shareUrlTemp);
	if (urlStr.isEmpty()) {
		urlStr = getInfo(mSource, g_shareUrl);
	}

	ui->GotoBtn->setProperty(SHARE_URL_KEY, urlStr);
	QString disTxt = getElidedText(ui->UrlPtn, urlStr, ui->UrlPtn->contentsRect().width());
	ui->UrlPtn->setText(disTxt);

	//encode value
	disName = QUrl::toPercentEncoding(disName);
	urlStr = QUrl::toPercentEncoding(urlStr);

	auto facebookShareUrl = g_facebookShareUrl.arg(urlStr);
	ui->OpenUrlFaceBook->setProperty(SHARE_URL_KEY, facebookShareUrl);

	auto twitterUrl = g_twitterShareUrl.arg(disName + " Broadcast Share ", urlStr);
	ui->OpenUrlTwitter->setProperty(SHARE_URL_KEY, twitterUrl);

	updatePixmap();
}

void PLSShareSourceItem::onCopyPressed()
{
	auto clipboard = QGuiApplication::clipboard();
	QString url = getInfoOfObject(ui->GotoBtn, SHARE_URL_KEY, QString());
	if (url.isEmpty()) {
		return;
	}
	clipboard->setText(url);
	emit UrlCopied();
	ui->CopyBtn->setEnabled(false);
}

void PLSShareSourceItem::openUrl() const
{
	auto obj = sender();
	if (obj) {
		QString url = getInfoOfObject(obj, SHARE_URL_KEY, QString());
		if (!QDesktopServices::openUrl(url)) {
			qDebug() << " error on openning url :" << url;
		}
	}
}

void PLSShareSourceItem::on_CloseBtn_clicked()
{
	this->close();
}

bool PLSShareSourceItem::eventFilter(QObject *watched, QEvent *event)
{

	if (watched == this && event->type() == QEvent::WindowDeactivate) {
		this->setHidden(true);
		this->deleteLater();
		return true;
	}

	return PLSDialogView::eventFilter(watched, event);
}

void PLSShareSourceItem::updatePixmap()
{
	QString uuid = getInfo(mSource, g_channelUUID);
	QString userHeaderPic;
	QString platformPic;
	getComplexImageOfChannel(uuid, userHeaderPic, platformPic, "", ".*profile");
	ui->UserIconLabel->setMainPixmap(userHeaderPic, QSize(110, 110));
	ui->UserIconLabel->setPlatformPixmap(platformPic, QSize(34, 34));
}
