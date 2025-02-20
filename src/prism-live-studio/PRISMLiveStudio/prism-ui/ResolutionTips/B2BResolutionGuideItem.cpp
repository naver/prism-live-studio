#include "B2BResolutionGuideItem.h"
#include "ui_B2BResolutionGuideItem.h"
#include "ChannelCommonFunctions.h"
#include "ResolutionConst.h"

B2BResolutionGuideItem::B2BResolutionGuideItem(QWidget *parent) : QFrame(parent), ui(new Ui::B2BResolutionGuideItem)
{
	ui->setupUi(this);
}

B2BResolutionGuideItem::~B2BResolutionGuideItem()
{
	delete ui;
}

void B2BResolutionGuideItem::initialize(const B2BResolutionPara &data, ResolutionGuidePage *page)
{
	m_data = data;
	connect(page, &ResolutionGuidePage::downloadThumbnailFinish, this, &B2BResolutionGuideItem::setThumbnail, Qt::QueuedConnection);
	setThumbnail();
	ui->templateName->setText(data.templateName);
	auto prefer = data.output_FPS;
	ui->output_FPS->setText(prefer);
	ui->bitrate->setText(data.bitrate);
	ui->keyframeInterval->setText(data.keyframeInterval);
	ui->ResolutionApplyBtn->setProperty("link", data.templateName + ":" + prefer + ":" + data.bitrate + ":" + data.keyframeInterval);
	connect(ui->ResolutionApplyBtn, &QAbstractButton::clicked, this, &B2BResolutionGuideItem::onLinkSelected);
}

void B2BResolutionGuideItem::onLinkSelected()
{
	auto url = ui->ResolutionApplyBtn->property("link").toString();
	emit sigResolutionSelected(url);
}

void B2BResolutionGuideItem::setLinkEnanled(bool isEnable)
{
	ui->MiddleFrame->setEnabled(isEnable);
}

void B2BResolutionGuideItem::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	case QEvent::Show:
		break;
	default:
		break;
	}
}

bool B2BResolutionGuideItem::eventFilter(QObject *obj, QEvent *event)
{
	auto widget = dynamic_cast<QWidget *>(obj);
	switch (event->type()) {
	case QEvent::HoverEnter:
		if (widget->isEnabled()) {
			widget->setCursor(Qt::PointingHandCursor);
		}

		break;
	case QEvent::HoverLeave:
		widget->setCursor(Qt::ArrowCursor);
		break;
	default:
		break;
	}
	return QFrame::eventFilter(obj, event);
}

void B2BResolutionGuideItem::setThumbnail()
{
	auto mainIconPath = getPlatformImageFromName(m_data.serviceName, channel_data::ImageType::tagIcon);
	if (!QFile::exists(m_data.streamingPresetThumbnail)) {
		if (!mainIconPath.isEmpty()) {
			QString tmpPath = QString("PRISMLiveStudio/resources/library/library_Policy_PC/%1").arg(mainIconPath);
			tmpPath = pls_get_user_path(tmpPath);
			if (QFile::exists(tmpPath)) {
				PLS_INFO("GuidePage", "use sync server path ! path is %s", tmpPath.toUtf8().constData());
				mainIconPath = tmpPath;
			}
		}
	} else {
		mainIconPath = m_data.streamingPresetThumbnail;
	}
	ui->PlatformIcon->setMainPixmap(mainIconPath, QSize(34, 34));
}
