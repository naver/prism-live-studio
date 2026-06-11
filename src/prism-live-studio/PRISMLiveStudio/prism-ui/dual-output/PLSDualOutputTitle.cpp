#include "PLSDualOutputTitle.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>

#include "libui.h"
#include "PLSBasic.h"
#include "ComplexHeaderIcon.h"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"

using namespace std;

const int PLATFORM_MARGING = 66;
const int SETTING_LEFT_MARGING = 5;

PLSDualOutputTitle::PLSDualOutputTitle(QWidget *parent) : QFrame(parent)
{
	pls_set_css(this, {"PLSDualOutputTitle"});

	setFixedHeight(40);

	m_widgetLeft = new QWidget(this);
	m_widgetRight = new QWidget(this);

	auto layoutLeft = new QHBoxLayout(m_widgetLeft);
	layoutLeft->setContentsMargins(QMargins(PLATFORM_MARGING, 0, PLATFORM_MARGING, 0));

	m_widgetHPlatform = new QWidget();
	m_widgetHPlatform->installEventFilter(this);
	layoutLeft->addWidget(m_widgetHPlatform, 0, Qt::AlignCenter);

	auto iconHPlatform = new QLabel();
	iconHPlatform->setObjectName("iconHPlatform");

	m_labelHPlatformCircle = new QLabel();
	m_labelHPlatformCircle->hide();
	m_labelHPlatformCircle->setObjectName("hPlatformCircle");

	m_layoutHPlatform = new QHBoxLayout(m_widgetHPlatform);
	m_layoutHPlatform->setContentsMargins(QMargins());
	m_layoutHPlatform->setSpacing(4);
	m_layoutHPlatform->addStretch();
	m_layoutHPlatform->addWidget(iconHPlatform);
	m_labelHPlatformOutput = new QLabel(tr("DualOutput.Preview.Title.Horizontal"));
	m_layoutHPlatform->addWidget(m_labelHPlatformOutput);
	m_layoutHPlatform->addWidget(m_labelHPlatformCircle);
	m_layoutHPlatform->addSpacing(3);
	m_layoutHPlatform->addStretch();

	auto layoutRight = new QHBoxLayout(m_widgetRight);
	layoutRight->setContentsMargins(QMargins(PLATFORM_MARGING, 0, PLATFORM_MARGING, 0));

	m_widgetVPlatform = new QWidget();
	m_widgetVPlatform->installEventFilter(this);
	layoutRight->addWidget(m_widgetVPlatform, 0, Qt::AlignCenter);

	auto iconVPlatform = new QLabel();
	iconVPlatform->setObjectName("iconVPlatform");

	m_labelVPlatformCircle = new QLabel();
	m_labelVPlatformCircle->hide();
	m_labelVPlatformCircle->setObjectName("vPlatformCircle");

	m_layoutVPlatform = new QHBoxLayout(m_widgetVPlatform);
	m_layoutVPlatform->setContentsMargins(QMargins());
	m_layoutVPlatform->setSpacing(4);
	m_layoutVPlatform->addStretch();
	m_layoutVPlatform->addWidget(iconVPlatform);
	m_labelVPlatformOutput = new QLabel(tr("DualOutput.Preview.Title.Vertical"));
	m_layoutVPlatform->addWidget(m_labelVPlatformOutput);
	m_layoutVPlatform->addWidget(m_labelVPlatformCircle);
	m_layoutVPlatform->addSpacing(3);
	m_layoutVPlatform->addStretch();

	auto hboxLayout = new QHBoxLayout(this);
	hboxLayout->setSpacing(6);
	hboxLayout->setContentsMargins(QMargins());

	m_buttonHPreview = new QPushButton();
	m_buttonHPreview->setObjectName("buttonHPreview");
	m_buttonHPreview->setCheckable(true);
	m_buttonHPreview->setChecked(true);
	m_buttonHPreview->setToolTip(tr("DualOutput.Preview.Title.Horizontal.Tip.Hide"));
	hboxLayout->addWidget(m_buttonHPreview);

	m_buttonVPreview = new QPushButton();
	m_buttonVPreview->setObjectName("buttonVPreview");
	m_buttonVPreview->setCheckable(true);
	m_buttonVPreview->setChecked(true);
	m_buttonVPreview->setToolTip(tr("DualOutput.Preview.Title.Vertical.Tip.Hide"));
	hboxLayout->addWidget(m_buttonVPreview);

	hboxLayout->addStretch(1);

	m_buttonSetting = new QRadioButton(tr("DualOutput.Preview.Title.Settings"));
	m_buttonSetting->installEventFilter(this);
	m_buttonSetting->setObjectName("buttonSetting");
	m_buttonSetting->setCheckable(false);
	m_buttonSetting->setLayoutDirection(Qt::RightToLeft);
	hboxLayout->addWidget(m_buttonSetting);

	pls_async_call(this, [this] { initPlatformIcon(); });

	connect(m_buttonHPreview, &QPushButton::toggled, PLSBasic::instance(), &PLSBasic::showHorizontalDisplay);
	connect(m_buttonVPreview, &QPushButton::toggled, PLSBasic::instance(), &PLSBasic::showVerticalDisplay);

	connect(m_buttonSetting, &QPushButton::clicked, this, [] { QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Q_ARG(QString, "Video"), Q_ARG(QString, "")); });
	connect(qobject_cast<PLSMainView *>(pls_get_toplevel_view(this)), &PLSMainView::onGolivePending, m_buttonSetting, &QWidget::setDisabled);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigSetChannelDualOutput, this, &PLSDualOutputTitle::onPlatformChanged);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, [this](const QString &uuid) {
		auto outputType = PLSCHANNELS_API->getValueOfChannel(uuid, ChannelData::g_channelDualOutput, ChannelData::NoSet);
		if (ChannelData::NoSet != outputType) {
			onPlatformChanged(uuid, outputType);
		}
	});
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, qOverload<const QString &>(&PLSDualOutputTitle::removePlatformIcon));
}

void PLSDualOutputTitle::onPlatformChanged(const QString &uuid, ChannelData::ChannelDualOutput outputType)
{
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
		if (auto widget = layout->itemAt(i)->widget(); nullptr != widget && widget->property("id").toString() == uuid) {
			return true;
		}
	}

	return false;
}

void PLSDualOutputTitle::addPlatformIcon(const QString &uuid, bool bMain)
{
	if (isIconExists(bMain ? m_layoutHPlatform : m_layoutVPlatform, uuid)) {
		return;
	}

	QSignalBlocker signalBlocker(bMain ? m_widgetHPlatform : m_widgetVPlatform);

	m_bAllowChecking = false;

	auto labelIcon = new QLabel(this);
	labelIcon->setObjectName("platformIcon");
	labelIcon->setScaledContents(true);

	QString userIcon;
	QString platformIcon;
	getComplexImageOfChannel(uuid, channel_data::ImageType::tagIcon, userIcon, platformIcon);

	auto loadPixmap = [](const QString &path, const QSize &size) {
		QPixmap pixmap;
		if (path.toLower().endsWith(".svg")) {
			pixmap = pls_shared_paint_svg(path, size);
		} else {
			pixmap = QPixmap(path);
		}

		pls_shared_circle_mask_image(pixmap);

		return pixmap;
	};

	labelIcon->setPixmap(loadPixmap(platformIcon, {56, 56}));

	labelIcon->setProperty("id", uuid);
	if (bMain) {
		m_layoutHPlatform->insertWidget(m_layoutHPlatform->count() - 1, labelIcon);
	} else {
		m_layoutVPlatform->insertWidget(m_layoutVPlatform->count() - 1, labelIcon);
	}

	if (bMain) {
		m_labelHPlatformCircle->setVisible(m_bShowHPlatforms);
	} else {
		m_labelVPlatformCircle->setVisible(m_bShowVPlatforms);
	}

	labelIcon->setVisible(bMain ? m_bShowHPlatforms : m_bShowVPlatforms);
	labelIcon->repaint();

	m_bAllowChecking = true;
	if (bMain) {
		pls_async_call(this, [this] {
			checkSettingButtonWidth();
			checkHPlatformTooLarge();
		});
	} else {
		pls_async_call(this, [this] {
			checkSettingButtonWidth();
			checkVPlatformTooLarge();
		});
	}
}

bool PLSDualOutputTitle::removePlatformIcon(QHBoxLayout *layout, const QString &uuid)
{
	bool isRemoved = false;
	for (auto i = 0; i < layout->count(); ++i) {
		if (auto widget = layout->itemAt(i)->widget(); nullptr != widget && widget->property("id").toString() == uuid) {
			widget->hide();
			delete widget;

			isRemoved = true;
		}
	}

	if (isRemoved) {
		auto hasPlatforms = false;
		for (auto i = 0; i < layout->count(); ++i) {
			if (auto widget = layout->itemAt(i)->widget(); nullptr != widget && !widget->property("id").isNull()) {
				hasPlatforms = true;
			}
		}

		if (m_layoutHPlatform == layout) {
			m_labelHPlatformCircle->setVisible(hasPlatforms && m_bShowHPlatforms);
			m_widgetLeft->repaint();

			auto bShowHPlatforms = m_bShowHPlatforms;
			auto bShowHPlatformOutput = m_bShowHPlatformOutput;
			if (!m_bShowHPlatforms) {
				m_bShowHPlatforms = true;
			} else if (!m_bShowHPlatformOutput) {
				m_bShowHPlatformOutput = true;
			}

			pls_async_call(this, [this, bShowHPlatforms, bShowHPlatformOutput] {
				checkHPlatformTooLarge();
				m_bShowHPlatforms = bShowHPlatforms;
				m_bShowHPlatformOutput = bShowHPlatformOutput;
				pls_async_call(this, [this] { checkHPlatformIsEnough(); });
			});
		} else {
			m_labelVPlatformCircle->setVisible(hasPlatforms && m_bShowHPlatforms);
			m_widgetRight->repaint();

			auto bShowVPlatforms = m_bShowVPlatforms;
			auto bShowVPlatformOutput = m_bShowVPlatformOutput;
			if (!m_bShowVPlatforms) {
				m_bShowVPlatforms = true;
			} else if (!m_bShowVPlatformOutput) {
				m_bShowVPlatformOutput = true;
			}

			pls_async_call(this, [this, bShowVPlatforms, bShowVPlatformOutput] {
				checkVPlatformTooLarge();
				m_bShowVPlatforms = bShowVPlatforms;
				m_bShowVPlatformOutput = bShowVPlatformOutput;
				pls_async_call(this, [this] { checkVPlatformIsEnough(); });
			});
		}

		pls_async_call(this, [this] { checkSettingButtonWidth(); });
	}

	return isRemoved;
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
	m_widgetLeft->setVisible(bVisible);
	resizePaltformParent();
	m_buttonHPreview->setToolTip(bVisible ? tr("DualOutput.Preview.Title.Horizontal.Tip.Hide") : tr("DualOutput.Preview.Title.Horizontal.Tip.Show"));

	QSignalBlocker signalBlocker(m_buttonHPreview);
	m_buttonHPreview->setChecked(bVisible);
}

void PLSDualOutputTitle::showVerticalDisplay(bool bVisible)
{
	m_widgetRight->setVisible(bVisible);
	resizePaltformParent();
	m_buttonVPreview->setToolTip(bVisible ? tr("DualOutput.Preview.Title.Vertical.Tip.Hide") : tr("DualOutput.Preview.Title.Vertical.Tip.Show"));

	QSignalBlocker signalBlocker(m_buttonVPreview);
	m_buttonVPreview->setChecked(bVisible);
}

void PLSDualOutputTitle::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	resizePaltformParent();
}

bool PLSDualOutputTitle::eventFilter(QObject *watched, QEvent *event)
{
	auto bResult = QWidget::eventFilter(watched, event);

	if (m_bAllowChecking) {
		if (watched == m_buttonSetting && QEvent::Resize == event->type()) {
			checkSettingButtonWidth();
		} else if (watched == m_widgetHPlatform && QEvent::Resize == event->type()) {
			checkHPlatformTooLarge();
			checkSettingButtonWidth();
		} else if (watched == m_widgetHPlatform && QEvent::Move == event->type()) {
			checkHPlatformIsEnough();
			checkSettingButtonWidth();
		} else if (watched == m_widgetVPlatform && QEvent::Resize == event->type()) {
			checkVPlatformTooLarge();
			checkSettingButtonWidth();
		} else if (watched == m_widgetVPlatform && QEvent::Move == event->type()) {
			checkVPlatformIsEnough();
			checkSettingButtonWidth();
		}
	}

	return bResult;
}

void PLSDualOutputTitle::resizePaltformParent()
{
	if (!m_widgetLeft->isVisible() || !m_widgetRight->isVisible()) {
		m_widgetLeft->setGeometry(0, 0, width(), height());
		m_widgetRight->setGeometry(0, 0, width(), height());
	} else {
		m_widgetLeft->setGeometry(0, 0, width() / 2, height());
		m_widgetRight->setGeometry(width() / 2, 0, width() / 2, height());
	}

	pls_async_call(this, [this] { checkSettingButtonWidth(); });
}

void PLSDualOutputTitle::checkSettingButtonWidth()
{
	auto iLeft = m_buttonSetting->mapTo(this, QPoint(0, 0)).x();
	if (!m_buttonSetting->text().isEmpty()) {
		if (m_widgetRight->isVisible()) {
			if (iLeft - SETTING_LEFT_MARGING <= m_widgetVPlatform->mapTo(this, QPoint(m_widgetVPlatform->width(), 0)).x()) {
				if (0 == m_ibuttonSettingWidth) {
					m_ibuttonSettingWidth = m_buttonSetting->width();
				}
				m_buttonSetting->setText(QString());
			}
		} else if (m_widgetLeft->isVisible()) {
			if (iLeft - SETTING_LEFT_MARGING <= m_widgetHPlatform->mapTo(this, QPoint(m_widgetHPlatform->width(), 0)).x()) {
				if (0 == m_ibuttonSettingWidth) {
					m_ibuttonSettingWidth = m_buttonSetting->width();
				}
				m_buttonSetting->setText(QString());
			}
		}
	} else {
		if (m_widgetRight->isVisible()) {
			if (iLeft - SETTING_LEFT_MARGING + m_buttonSetting->width() - m_ibuttonSettingWidth > m_widgetVPlatform->mapTo(this, QPoint(m_widgetVPlatform->width(), 0)).x()) {
				m_buttonSetting->setText(tr("DualOutput.Preview.Title.Settings"));
			}
		} else if (m_widgetLeft->isVisible()) {
			if (iLeft - SETTING_LEFT_MARGING + m_buttonSetting->width() - m_ibuttonSettingWidth > m_widgetHPlatform->mapTo(this, QPoint(m_widgetHPlatform->width(), 0)).x()) {
				m_buttonSetting->setText(tr("DualOutput.Preview.Title.Settings"));
			}
		} else {
			m_buttonSetting->setText(tr("DualOutput.Preview.Title.Settings"));
		}
	}
}

void PLSDualOutputTitle::checkHPlatformTooLarge()
{
	auto checkPlatformIcons = [this] {
		if (!m_bShowHPlatforms) {
			return;
		}

		m_iHPlatformsWidth = m_layoutHPlatform->sizeHint().width();
		if (m_widgetHPlatform->width() < m_iHPlatformsWidth) {
			m_bShowHPlatforms = false;

			for (auto i = 0; i < m_layoutHPlatform->count(); ++i) {
				if (auto widget = m_layoutHPlatform->itemAt(i)->widget(); nullptr != widget && !widget->property("id").isNull()) {
					widget->hide();
				}
			}

			m_labelHPlatformCircle->hide();
		}
	};

	auto checkOutputText = [this, checkPlatformIcons] {
		if (!m_bShowHPlatformOutput) {
			return;
		}

		m_iHPlatformOutputWidth = m_layoutHPlatform->sizeHint().width();
		if (m_widgetHPlatform->width() < m_iHPlatformOutputWidth) {
			m_bShowHPlatformOutput = false;
			m_labelHPlatformOutput->setVisible(false);

			pls_async_call(this, checkPlatformIcons);
		}
	};

	if (m_bShowHPlatformOutput) {
		checkOutputText();
	} else {
		checkPlatformIcons();
	}
}

void PLSDualOutputTitle::checkHPlatformIsEnough()
{
	if (!m_bShowHPlatforms && m_widgetLeft->width() > (PLATFORM_MARGING * 2 + m_iHPlatformsWidth)) {
		m_bShowHPlatforms = true;

		m_labelHPlatformCircle->show();
		for (auto i = 0; i < m_layoutHPlatform->count(); ++i) {
			if (auto widget = m_layoutHPlatform->itemAt(i)->widget(); nullptr != widget && !widget->property("id").isNull()) {
				widget->show();
			}
		}
	}

	if (m_bShowHPlatforms && !m_bShowHPlatformOutput && m_widgetLeft->width() > (PLATFORM_MARGING * 2 + m_iHPlatformOutputWidth)) {
		m_bShowHPlatformOutput = true;
		m_labelHPlatformOutput->setVisible(true);
	}
}

void PLSDualOutputTitle::checkVPlatformTooLarge()
{
	auto checkPlatformIcons = [this] {
		if (!m_bShowVPlatforms) {
			return;
		}

		m_iVPlatformsWidth = m_layoutVPlatform->sizeHint().width();
		if (m_widgetVPlatform->width() < m_iVPlatformsWidth) {
			m_bShowVPlatforms = false;

			for (auto i = 0; i < m_layoutVPlatform->count(); ++i) {
				if (auto widget = m_layoutVPlatform->itemAt(i)->widget(); nullptr != widget && !widget->property("id").isNull()) {
					widget->hide();
				}
			}

			m_labelVPlatformCircle->hide();
		}
	};

	auto checkOutputText = [this, checkPlatformIcons] {
		if (!m_bShowVPlatformOutput) {
			return;
		}

		m_iVPlatformOutputWidth = m_layoutVPlatform->sizeHint().width();
		if (m_widgetVPlatform->width() < m_iVPlatformOutputWidth) {
			m_bShowVPlatformOutput = false;
			m_labelVPlatformOutput->setVisible(false);

			pls_async_call(this, checkPlatformIcons);
		}
	};

	if (m_bShowVPlatformOutput) {
		checkOutputText();
	} else {
		checkPlatformIcons();
	}
}

void PLSDualOutputTitle::checkVPlatformIsEnough()
{
	if (!m_bShowVPlatforms && m_widgetRight->width() > (PLATFORM_MARGING * 2 + m_iVPlatformsWidth)) {
		m_bShowVPlatforms = true;

		m_labelVPlatformCircle->show();
		for (auto i = 0; i < m_layoutVPlatform->count(); ++i) {
			if (auto widget = m_layoutVPlatform->itemAt(i)->widget(); nullptr != widget && !widget->property("id").isNull()) {
				widget->show();
			}
		}
	}

	if (m_bShowVPlatforms && !m_bShowVPlatformOutput && m_widgetRight->width() > (PLATFORM_MARGING * 2 + m_iVPlatformOutputWidth)) {
		m_bShowVPlatformOutput = true;
		m_labelVPlatformOutput->setVisible(true);
	}
}
