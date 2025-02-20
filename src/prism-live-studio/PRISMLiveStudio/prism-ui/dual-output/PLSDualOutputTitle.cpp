#include "PLSDualOutputTitle.h"

#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedLayout>

#include "libui.h"
#include "PLSBasic.h"
#include "ComplexHeaderIcon.h"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"

using namespace std;

PLSDualOutputTitle::PLSDualOutputTitle(QWidget *parent) : QFrame(parent)
{
	pls_set_css(this, {"PLSDualOutputTitle"});

	setFixedHeight(40);

	auto stackedLayout = new QStackedLayout(this);
	stackedLayout->setStackingMode(QStackedLayout::StackAll);

	auto widgetOuter = new QWidget();
	auto layoutOuter = new QHBoxLayout(widgetOuter);
	layoutOuter->setContentsMargins(QMargins());
	stackedLayout->addWidget(widgetOuter);

	m_buttonHPreview = new QPushButton();
	m_buttonHPreview->setObjectName("buttonHPreview");
	m_buttonHPreview->setCheckable(true);
	m_buttonHPreview->setChecked(true);
	m_buttonHPreview->setToolTip(tr("DualOutput.Preview.Title.Horizontal.Tip.Hide"));
	layoutOuter->addWidget(m_buttonHPreview);

	m_buttonVPreview = new QPushButton();
	m_buttonVPreview->setObjectName("buttonVPreview");
	m_buttonVPreview->setCheckable(true);
	m_buttonVPreview->setChecked(PLSBasic::instance()->getVerticalPreviewEnabled());
	m_buttonVPreview->setToolTip(tr("DualOutput.Preview.Title.Vertical.Tip.Hide"));
	layoutOuter->addWidget(m_buttonVPreview);

	layoutOuter->addStretch(1);

	auto buttonSetting = new QRadioButton(tr("DualOutput.Preview.Title.Settings"));
	buttonSetting->setObjectName("buttonSetting");
	buttonSetting->setCheckable(false);
	buttonSetting->setLayoutDirection(Qt::RightToLeft);
	layoutOuter->addWidget(buttonSetting);

	auto widgetInner = new QWidget();
	auto layoutInner = new QHBoxLayout(widgetInner);
	layoutInner->setContentsMargins(QMargins());
	stackedLayout->addWidget(widgetInner);

	m_widgetHPlatform = new QWidget();
	layoutInner->addWidget(m_widgetHPlatform);

	auto iconHPlatform = new QLabel();
	iconHPlatform->setObjectName("iconHPlatform");

	m_layoutHPlatform = new QHBoxLayout(m_widgetHPlatform);
	m_layoutHPlatform->setContentsMargins(QMargins());
	m_layoutHPlatform->addStretch();
	m_layoutHPlatform->addWidget(iconHPlatform);
	m_layoutHPlatform->addWidget(new QLabel(tr("DualOutput.Preview.Title.Horizontal")));
	m_layoutHPlatform->addStretch();

	m_widgetVPlatform = new QWidget();
	m_widgetVPlatform->setVisible(PLSBasic::instance()->getVerticalPreviewEnabled());
	layoutInner->addWidget(m_widgetVPlatform);

	auto iconVPlatform = new QLabel();
	iconVPlatform->setObjectName("iconVPlatform");

	m_layoutVPlatform = new QHBoxLayout(m_widgetVPlatform);
	m_layoutVPlatform->setContentsMargins(QMargins());
	m_layoutVPlatform->addStretch();
	m_layoutVPlatform->addWidget(iconVPlatform);
	m_layoutVPlatform->addWidget(new QLabel(tr("DualOutput.Preview.Title.Vertical")));
	m_layoutVPlatform->addStretch();

	initPlatformIcon();

	connect(m_buttonHPreview, &QPushButton::toggled, PLSBasic::instance(), &PLSBasic::showHorizontalDisplay);
	connect(m_buttonVPreview, &QPushButton::toggled, PLSBasic::instance(), &PLSBasic::showVerticalDisplay);
	
	connect(buttonSetting, &QPushButton::clicked, this, [] { PLSBasic::instance()->showSettingView(QString("Video"), QString()); });

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigSetChannelDualOutput, this, &PLSDualOutputTitle::onPlatformChanged);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, [this](const QString &uuid) {
		auto outputType = PLSCHANNELS_API->getValueOfChannel(uuid, ChannelData::g_channelDualOutput, ChannelData::NoSet);
		if (ChannelData::NoSet != outputType) {
			onPlatformChanged(uuid, outputType);
		}
	});
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, qOverload<const QString &>(&PLSDualOutputTitle::removePlatformIcon));

	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this, [this] {
		if (!m_slRemovedPlatform.isEmpty()) {
			for (auto &uuid : m_slRemovedPlatform) {
				removePlatformIcon(uuid);
			}

			m_slRemovedPlatform.clear();
		}
	});
}

void PLSDualOutputTitle::onPlatformChanged(const QString &uuid, ChannelData::ChannelDualOutput outputType)
{
	if (ChannelData::NoSet == outputType && PLS_PLATFORM_API->getLiveStatus() != LiveStatus::Normal) {
		m_slRemovedPlatform << uuid;
		return;
	}

	if (!removePlatformIcon(m_layoutHPlatform, uuid)) {
		removePlatformIcon(m_layoutVPlatform, uuid);
	}

	if (ChannelData::NoSet != outputType) {
		addPlatformIcon(uuid, ChannelData::HorizontalOutput == outputType);
	}
}

bool PLSDualOutputTitle::isIconExists(QHBoxLayout *layout, const QString &uuid)
{
	for (auto i = 0; i < layout->count(); ++i) {
		if (auto widget = layout->itemAt(i)->widget(); nullptr != widget) {
			auto iconPlatform = qobject_cast<ComplexHeaderIcon *>(widget);
			if (nullptr != iconPlatform && iconPlatform->property("id").toString() == uuid) {
				return true;
			}
		}
	}

	return false;
}

void PLSDualOutputTitle::addPlatformIcon(const QString &uuid, bool bMain)
{
	if (isIconExists(bMain ? m_layoutHPlatform : m_layoutVPlatform, uuid)) {
		return;
	}

	auto iconPlatform = new QWidget(this);
	iconPlatform->setFixedSize(36, 30);

	auto labelBig = new QLabel(iconPlatform);
	labelBig->setFixedSize(30, 30);
	labelBig->setScaledContents(true);

	auto labelSmall = new QLabel(iconPlatform);
	labelSmall->setFixedSize(16, 16);
	labelSmall->setScaledContents(true);
	labelSmall->move(20, 14);

	QString userIcon;
	QString platformIcon;
	getComplexImageOfChannel(uuid, channel_data::ImageType::tagIcon, userIcon, platformIcon);

	auto loadFile = [](const QString &path, const QSize& size) {
		QPixmap pixmap;
		if (path.toLower().endsWith(".svg")) {
			pixmap = pls_shared_paint_svg(path, size);
		} else {
			pixmap = QPixmap(path);
		}

		return pixmap;
	};

	auto loadPixmap = [loadFile](const QString &path, const QSize &size) {
		auto pixmap = loadFile(path, size);

		if (pixmap.isNull()) {
			pixmap = PLSCHANNELS_API->getImage(path, size);
		}

		if (pixmap.isNull()) {
			pixmap = loadFile(":/channels/resource/images/ChannelsSource/img-setting-profile-blank.svg", size);
		}

		pls_shared_circle_mask_image(pixmap);

		return pixmap;
	};

	labelBig->setPixmap(loadPixmap(userIcon, {120, 120}));
	labelSmall->setPixmap(loadPixmap(platformIcon, {64, 64}));

	iconPlatform->setProperty("id", uuid);
	if (bMain) {
		m_layoutHPlatform->insertWidget(m_layoutHPlatform->count() - 1, iconPlatform);
	} else {
		m_layoutVPlatform->insertWidget(m_layoutVPlatform->count() - 1, iconPlatform);
	}

	iconPlatform->show();
}

bool PLSDualOutputTitle::removePlatformIcon(QHBoxLayout *layout, const QString &uuid)
{
	for (auto i = 0; i < layout->count(); ++i) {
		if (auto widget = layout->itemAt(i)->widget(); nullptr != widget) {
			auto iconPlatform = qobject_cast<QWidget *>(widget);
			if (nullptr != iconPlatform && iconPlatform->property("id").toString() == uuid) {
				widget->deleteLater();

				return true;
			}
		}
	}

	return false;
}

void PLSDualOutputTitle::removePlatformIcon(const QString &uuid)
{
	if (!removePlatformIcon(m_layoutHPlatform, uuid)) {
		removePlatformIcon(m_layoutVPlatform, uuid);
	}
}

void PLSDualOutputTitle::initPlatformIcon()
{
	QStringList horOutputList, verOutputList;
	PLSChannelDataAPI::getInstance()->getChannelCountOfOutputDirection(horOutputList, verOutputList);

	for_each(horOutputList.begin(), horOutputList.end(), bind(&PLSDualOutputTitle::addPlatformIcon, this, placeholders::_1, true));
	for_each(verOutputList.begin(), verOutputList.end(), bind(&PLSDualOutputTitle::addPlatformIcon, this, placeholders::_1, false));
}

void PLSDualOutputTitle::showHorizontalDisplay(bool bVisible)
{
	m_widgetHPlatform->setVisible(bVisible);
	m_buttonHPreview->setToolTip(bVisible ? tr("DualOutput.Preview.Title.Horizontal.Tip.Hide") : tr("DualOutput.Preview.Title.Horizontal.Tip.Show"));

	QSignalBlocker signalBlocker(m_buttonHPreview);
	m_buttonHPreview->setChecked(bVisible);
}

void PLSDualOutputTitle::showVerticalDisplay(bool bVisible)
{
	m_widgetVPlatform->setVisible(bVisible);
	m_buttonVPreview->setToolTip(bVisible ? tr("DualOutput.Preview.Title.Vertical.Tip.Hide") : tr("DualOutput.Preview.Title.Vertical.Tip.Show"));

	QSignalBlocker signalBlocker(m_buttonVPreview);
	m_buttonVPreview->setChecked(bVisible);
}
