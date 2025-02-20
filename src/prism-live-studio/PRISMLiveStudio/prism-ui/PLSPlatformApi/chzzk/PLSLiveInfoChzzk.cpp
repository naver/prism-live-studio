#include "PLSLiveInfoChzzk.h"
#include "ui_PLSLiveInfoChzzk.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"
#include "QTextFrame"
#include <QTextDocument>
#include <QTextFrameFormat>
#include <qdebug.h>
#include "login-common-helper.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "utils-api.h"
#include "libui.h"
#include "PLSGuideButton.h"
#include <QShowEvent>
#include <QTextDocument>

static const char *const liveInfoThisModule = "PLSLiveInfoChzzk";

static QString getHtmlListTooltip(const QList<QString> &args)
{
	const QString liTemplate = R"(
<li style=" margin-top:0px; margin-bottom:0px; margin-left:10px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
	%1
</li>)";
	QString content;
	for (QString data : args) {
		data = data.trimmed();
		data = data.replace("\n", "<br>");
		if (!data.isEmpty())
			content.append(liTemplate.arg(data));
	}
	QString tooltipText = R"(
<body style=" font-family: .AppleSystemUIFont, Segoe UI, Malgun Gothic, Dotum, Gulim, sans-serif, -apple-system, BlinkMacSystemFont !important; font-size:11px; font-weight:400; font-style:normal;">
	<ul style='margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 0;'>
		%1
	</ul>
</body>)";
	tooltipText = tooltipText.arg(content);
	return tooltipText;
}

static void replaceHtmlTooltipByRecursive(QWidget *parent)
{
	if (!parent) {
		return;
	}
	for (QObject *child : parent->children()) {
		if (!child->isWidgetType()) {
			continue;
		}
		if (auto childWidget = dynamic_cast<PLSHelpIcon *>(child); childWidget) {
			auto tip = childWidget->toolTip();
			if (!tip.startsWith("*")) {
				continue;
			}
			auto subStr = tip.split("*");
			auto changedTip = getHtmlListTooltip(subStr);

			childWidget->setToolTip(changedTip);

			QTextDocument doc;
			doc.setHtml(changedTip);
			int limitWidth = qMin(doc.size().width(), parent->topLevelWidget()->width() * 0.7);
			childWidget->setStyleSheet(QString("QToolTip { min-width: %1px; max-width: %1px; }").arg(limitWidth));
		} else {
			replaceHtmlTooltipByRecursive(dynamic_cast<QWidget *>(child));
		}
	}
}

PLSLiveInfoChzzk::PLSLiveInfoChzzk(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent), m_platform(dynamic_cast<PLSPlatformChzzk *>(pPlatformBase))
{
	ui = pls_new<Ui::PLSLiveInfoChzzk>();
	pls_add_css(this, {"PLSLiveInfoChzzk"});
	setupUi(ui);
	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoChzzk::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoChzzk::cancelButtonClicked);
	updateStepTitle(ui->okButton);

	ui->dualWidget->setText(tr("chzzk.liveinfo.title"))->setUUID(m_platform->getChannelUUID());

	ui->thumbnailButton->setTipLabelString("(1280x720)");
	ui->thumbnailButton->setShowDeleteBtn(true);
#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
	}
#endif
	m_platform->liveInfoIsShowing();

	setupFirstUI();
	setupGuideButton();
	PLSAPICommon::createHelpIconWidget(ui->leftTitle_category, tr("chzzk.category.tooltip"), ui->formLayout, this, &PLSLiveInfoChzzk::shown);
	PLSAPICommon::createHelpIconWidget(ui->leftTitle_age, tr("chzzk.age.limit.tooltip"), ui->formLayout, this, &PLSLiveInfoChzzk::shown);
	PLSAPICommon::createHelpIconWidget(ui->leftTitle_money, tr("chzzk.need.money.tooltip"), ui->formLayout, this, &PLSLiveInfoChzzk::shown);
	replaceHtmlTooltipByRecursive(content());
}

PLSLiveInfoChzzk::~PLSLiveInfoChzzk()
{
	pls_delete(ui);
}

void PLSLiveInfoChzzk::setupFirstUI()
{
	ui->liveTitleLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));

	ui->lineEditTitle->setText("");
	m_chatGroups = pls_new<PLSRadioButtonGroup>(ui->ageRadioWidget);
	m_chatGroups->addButton(ui->radioButton_chat_all, 0);
	m_chatGroups->addButton(ui->radioButton_chat_follow, 1);
	m_chatGroups->addButton(ui->radioButton_chat_manager, 2);

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoChzzk::titleEdited, Qt::QueuedConnection);
	connect(
		m_platform, &PLSPlatformChzzk::closeDialogByExpired, this,
		[this]() {
			hideLoading();
			reject();
		},
		Qt::DirectConnection);
	connect(
		m_platform, &PLSPlatformChzzk::toShowLoading, this,
		[this](bool isShow) {
			if (isShow) {
				showLoading(this->content());
			} else {

				hideLoading();
			}
		},
		Qt::DirectConnection);

	connect(ui->thumbnailButton, &PLSSelectImageButton::takeButtonClicked, this, []() { PLS_UI_STEP(liveInfoThisModule, "Chzzk LiveInfo's Thumbnail Take Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::selectButtonClicked, this, []() { PLS_UI_STEP(liveInfoThisModule, "Chzzk LiveInfo's Thumbnail Select Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::deleteButtonClicked, this, [this]() { m_isClickedDeleteBtn = true; });
	connect(m_platform, &PLSPlatformChzzk::changeClipToNotAllow, this, [this]() {
		ui->radioButton_clip_allow->setChecked(false);
		ui->radioButton_clip_not_allow->setChecked(true);
	});
	connect(ui->lineEditCategory, &PLSSearchCombobox::startSearchText, m_platform, &PLSPlatformChzzk::requestSearchCategory);
	connect(m_platform, &PLSPlatformChzzk::onGetCategory, ui->lineEditCategory, &PLSSearchCombobox::receiveSearchData);
	connect(ui->pushButton_age, &QPushButton::clicked, []() { QDesktopServices::openUrl(QUrl(QString("%1/content-guidelines").arg(g_plsChzzkStudioHost))); });

	refreshUI();
}

void PLSLiveInfoChzzk::refreshUI()
{
	refreshThumButton();

	const auto &data = m_platform->getSelectData();
	ui->lineEditTitle->setText(data.title);
	ui->lineEditTitle->setCursorPosition(0);

	ui->lineEditCategory->setSelectData(data.categoryData);

	ui->age_checkbox->setCheckState(data.isAgeLimit == true ? Qt::Checked : Qt::Unchecked);
	ui->money_checkbox->setCheckState(data.isNeedMoney == true ? Qt::Checked : Qt::Unchecked);

	m_chatGroups->button(PLSPlatformChzzk::getIndexOfChatPermission(data.chatPermission))->setChecked(true);

	ui->radioButton_clip_allow->setChecked(data.clipActive);
	ui->radioButton_clip_not_allow->setChecked(!data.clipActive);
	doUpdateOkState();
}

void PLSLiveInfoChzzk::setupGuideButton()
{
	auto widget = pls_new<QWidget>();
	widget->setObjectName("guideBackView");
	auto manageButton = pls_new<GuideButton>(tr("chzzk.goto.studio"), false, nullptr, []() { QDesktopServices::openUrl(QUrl(g_plsChzzkStudioHost)); });
	manageButton->setObjectName("manageButton");
	manageButton->setProperty("showHandCursor", true);

	auto lineView = pls_new<QWidget>(nullptr);
	lineView->setObjectName("lineView");

	auto layout = pls_new<QHBoxLayout>(widget);
	layout->setContentsMargins(0, 0, 25, 0);
	layout->setSpacing(10);
	layout->addWidget(manageButton, 0, Qt::AlignVCenter | Qt::AlignRight);
	layout->addWidget(lineView, 0, Qt::AlignCenter);
	layout->addWidget(createResolutionButtonsFrame());
	ui->titleWidget->layout()->addWidget(widget);
}

void PLSLiveInfoChzzk::refreshThumButton()
{
	auto url = m_platform->getSelectData().thumbnailUrl;
	if (url.isEmpty()) {
		return;
	}

	auto localPath = PLSAPICommon::getMd5ImagePath(url);
	if (QFile(localPath).exists()) {
		ui->thumbnailButton->setImagePath(localPath);
		return;
	}
	auto _callBack = [this](bool ok, const QString &imagePath) {
		if (ok) {
			auto urlNew = this->m_platform->getSelectData().thumbnailUrl;
			auto localPathNew = PLSAPICommon::getMd5ImagePath(urlNew);
			if (localPathNew == imagePath) {
				ui->thumbnailButton->setImagePath(imagePath);
			}
		} else {
			PLS_INFO(MODULE_PLATFORM_NCB2B, "download user icon failed");
		}
	};
	PLSAPICommon::downloadImageAsync(this, url, _callBack, false, localPath);
}

void PLSLiveInfoChzzk::titleEdited()
{
	static const int s_titleLengthLimit = 100;
	QString newText = ui->lineEditTitle->text();

	bool isLargeToMax = false;
	if (newText.length() > s_titleLengthLimit) {
		isLargeToMax = true;
		newText = newText.left(s_titleLengthLimit);
	}

	if (newText.compare(ui->lineEditTitle->text()) != 0) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
	}
	doUpdateOkState();

	if (isLargeToMax) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(s_titleLengthLimit).arg(m_platform->getChannelName()));
	}
}

void PLSLiveInfoChzzk::doUpdateOkState()
{
	if (ui->lineEditTitle->text().trimmed().isEmpty() || m_chatGroups->checkedId() == -1) {
		ui->okButton->setEnabled(false);
		return;
	}
	ui->okButton->setEnabled(true);
}

void PLSLiveInfoChzzk::okButtonClicked()
{
	PLS_UI_STEP(liveInfoThisModule, "Chzzk liveinfo OK Button Click", ACTION_CLICK);

	if (getIsRunLoading()) {
		PLS_INFO(MODULE_PLATFORM_CHZZK, "Chzzk ignore OK Button Click, because is loading");
		return;
	}

	showLoading(content());
	auto _onNext = [this](bool isSucceed) {
		hideLoading();
		PLS_INFO(MODULE_PLATFORM_CHZZK, "Chzzk liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (PLS_PLATFORM_API->isPrepareLive()) {
			if (isSucceed) {
				PLS_LOGEX(PLS_LOG_INFO, MODULE_PLATFORM_CHZZK,
					  {{"platformName", CHZZK}, //
					   {"startLiveStatus", "Success"},
					   {"channelID", m_platform->subChannelID().toUtf8().constData()}},
					  "NCB2B start live success");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_CHZZK,
					  {{"platformName", CHZZK},
					   {"startLiveStatus", "Failed"},
					   {"startLiveFailed", m_platform->getFailedErr().toUtf8().constData()},
					   {"channelID", m_platform->subChannelID().toUtf8().constData()}},
					  "Chzzk start live failed");
			}
		}
		if (isSucceed)
			accept();
	};

	PLSChzzkLiveinfoData uiData = m_platform->getSelectData();
	uiData.title = ui->lineEditTitle->text();
	uiData.isAgeLimit = ui->age_checkbox->checkState() == Qt::Checked;
	uiData.isNeedMoney = ui->money_checkbox->checkState() == Qt::Checked;

	uiData.chatPermission = PLSPlatformChzzk::getChatPermissionByIndex(m_chatGroups->checkedId());
	uiData.categoryData = ui->lineEditCategory->getSelectData();
	uiData.clipActive = ui->radioButton_clip_allow->isChecked();
	m_platform->setFailedErr("");
	m_platform->saveSettings(_onNext, uiData, ui->thumbnailButton->getImagePath(), this, m_isClickedDeleteBtn);
}

void PLSLiveInfoChzzk::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoThisModule, "Chzzk liveinfo Cancel Button Click", ACTION_CLICK);
	done(Rejected);
}

void PLSLiveInfoChzzk::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());
	auto _onNextVideo = [this](bool value) {
		refreshUI();
		hideLoading();

		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
			return;
		}
	};
	m_platform->requestLiveInfo(this, _onNextVideo);
	PLSLiveInfoBase::showEvent(event);
}
