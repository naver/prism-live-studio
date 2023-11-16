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
	ui->PlatformIcon->setMainPixmap(mainIconPath, QSize(34, 34), false);

	if (getInfo(data, ChannelData::g_data_type, ChannelData::ChannelType) >= ChannelData::CustomType) {
		ui->PlatformIcon->setPlatformPixmap(ChannelData::g_defualtPlatformSmallIcon, QSize(18, 18));
	}

	auto prefer = getInfo(data, resolution_const_space::g_PreferResolution);
	ui->ResolutionLabel->setText(prefer);
	ui->ResolutionApplyBtn->setProperty("link", platform + ":" + prefer);

	connect(ui->ResolutionApplyBtn, &QAbstractButton::clicked, this, &ResolutionGuideItem::onLinkSelected);
	auto descriptionMap = getInfo(data, resolution_const_space::g_AllResolutionDecription, QVariantMap());
	auto langStr = pls_get_current_language_short_str();
	auto srcDecription = descriptionMap.value(langStr.toUpper()).toString();
	auto srcList = srcDecription.split(QRegularExpression("\\n"), Qt::SkipEmptyParts);
	QString desList;
	QString templateStr =
		R"(<tr> <th style=" padding-top:4px; color:#666666; font-size:10px; ">&bull;</th> <td style="  padding-left:5px; font-weight:normal; line-height:18px;color:#bababa; ">%1</td> </tr>)";
	auto txtLay = new QVBoxLayout(ui->DescriptionLabel);
	txtLay->setContentsMargins(0, 0, 0, 0);
	txtLay->setSpacing(0);

	for (const auto &line : srcList) {
		auto blockTxt = templateStr.arg(line);
		desList.append(blockTxt);
	}

	blockLabel = new QLabel(ui->DescriptionLabel);
	blockLabel->setObjectName("blockLabel");
	blockLabel->setWordWrap(true);
	blockLabel->setText(QString("<table >%1</table>").arg(desList));
	blockLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	blockLabel->setMargin(1);
	txtLay->addWidget(blockLabel);

	if (platform.contains(YOUTUBE, Qt::CaseInsensitive)) {
		blockLabel->setText("");
	}

	checkBitrateData(data);

	mData = data;
}
void ResolutionGuideItem::checkBitrateData(const QVariantMap &data)
{
	auto bitrateLink = getInfo(data, resolution_const_space::g_BitrateGuide);

	if (!bitrateLink.isEmpty()) {
		auto srcTxt = blockLabel->text();
		QString templateStr =
			R"(<tr> <th style=" padding-top:4px; color:#666666; font-size:10px; ">&bull;</th> <td style="  padding-left:5px; font-weight:normal; line-height:18px;color:#bababa; ">%1</td> </tr>)";
		QString linkHtml = R"(<a href="%1"><span style=" padding-left:15px; text-decoration:none;font-weight:normal; line-height:18px;color:#effc35;">%2</span></a>)";
		linkHtml = linkHtml.arg(bitrateLink).arg(tr("Bitrate.Guide"));
		templateStr = templateStr.arg(linkHtml);
		srcTxt.append(templateStr);
		blockLabel->setText(srcTxt);
		blockLabel->setOpenExternalLinks(true);
	}
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
	case QEvent::Show:
		break;
	default:
		break;
	}
}

bool ResolutionGuideItem::eventFilter(QObject *obj, QEvent *event)
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
