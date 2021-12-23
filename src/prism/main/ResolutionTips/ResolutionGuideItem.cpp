#include "ResolutionGuideItem.h"
#include "ui_ResolutionGuideItem.h"
#include "ChannelCommonFunctions.h"
#include "ResolutionConst.h"

ResolutionGuideItem::ResolutionGuideItem(QWidget *parent) : QFrame(parent), ui(new Ui::ResolutionGuideItem)
{
	ui->setupUi(this);
}

ResolutionGuideItem::~ResolutionGuideItem()
{
	delete ui;
}

void ResolutionGuideItem::initialize(const QVariantMap &data)
{
	auto platform = getInfo(data, ChannelData::g_platformName);
	auto mainIconPath = getPlatformImageFromName(platform);
	ui->PlatformIcon->setPixmap(mainIconPath, QSize(34, 34), false);

	auto type = getInfo(data, ChannelData::g_data_type, ChannelData::ChannelType);
	if (type == ChannelData::RTMPType) {
		ui->PlatformIcon->setPlatformPixmap(ChannelData::g_defualtPlatformSmallIcon, QSize(18, 18));
	}

	auto prefer = getInfo(data, ResolutionConstSpace::g_PreferResolution);
	ui->ResolutionLabel->setText(prefer);
	ui->ResolutionApplyBtn->setProperty("link", platform + ":" + prefer);
	//ui->ResolutionApplyBtn->installEventFilter(this);
	connect(ui->ResolutionApplyBtn, &QAbstractButton::clicked, this, &ResolutionGuideItem::onLinkSelected);
	auto descriptionMap = getInfo(data, ResolutionConstSpace::g_AllResolutionDecription, QVariantMap());
	auto langStr = pls_get_current_language_short_str();
	auto srcDecription = descriptionMap.value(langStr.toUpper()).toString();
	auto srcList = srcDecription.split(QRegularExpression("\\n"), QString::SkipEmptyParts);
	QString desList;
	QString templateStr = R"(<div style=" text-indent: -10px; margin-left: 10px;font-weight: normal;line-height: 18px;color: #bababa;"><span style="color:#666666">&bull; </span>%1</div>)";
	for (auto &line : srcList) {
		desList.append(templateStr.arg(line));
	}
	auto description = QString("<p>%1</p>").arg(desList);
	ui->DescriptionLabel->setText(description);

	mData = data;
}

void ResolutionGuideItem::onLinkSelected()
{
	auto url = ui->ResolutionApplyBtn->property("link").toString();
	emit sigResolutionSelected(url);
}

void ResolutionGuideItem::setLinkEnanled(bool isEnable)
{
	ui->MiddleFrame->setEnabled(isEnable);
}

void ResolutionGuideItem::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

bool ResolutionGuideItem::eventFilter(QObject *obj, QEvent *event)
{
	auto widget = dynamic_cast<QWidget *>(obj);
	switch (event->type()) {
	case QEvent::HoverEnter: {
		if (widget->isEnabled()) {
			widget->setCursor(Qt::PointingHandCursor);
		}

	} break;
	case QEvent::HoverLeave: {
		widget->setCursor(Qt::ArrowCursor);
	} break;
	default:
		break;
	}
	return QFrame::eventFilter(obj, event);
}
