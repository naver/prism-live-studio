#include "PLSAddSourceGuideView.h"
#include "ui_PLSAddSourceGuideView.h"
#include "frontend-api.h"
#include <QSet>
#include "pls-common-define.hpp"
#include "liblog.h"
#include "PLSAPICommon.h"
#include "PLSAddSourceView.h"
#include "PLSBasic.h"
#include "pls-performance.h"
#include <QToolButton>

using namespace common;
#define SOURCE_LIST_DEFAULT_COUNT 5
constexpr auto iconPath = ":/resource/images/add-source-view/addSourceGuide/%1_%2.png";
constexpr auto guideTooltipWin = "Source.Guide.ToolTip.%1.Win";
constexpr auto guideTooltipMac = "Source.Guide.ToolTip.%1.Mac";
#if defined(Q_OS_WIN)
#define PLATFORM_VIDEO_CAPTURE_SOURCE_ID OBS_DSHOW_SOURCE_ID
#define GAME_SHOW_TOOLTIP true
#elif defined(Q_OS_MACOS)
#define PLATFORM_VIDEO_CAPTURE_SOURCE_ID OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID
#define GAME_SHOW_TOOLTIP false
#endif

extern QString GetIconKey(obs_icon_type type);

PLSAddSourceGuideView::PLSAddSourceGuideView(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSAddSourceGuideView), m_sourceTabButtonGroup(new QButtonGroup(this))
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_PERFORMANCE_START(addsourceSetup);
	pls_add_css(this, {"PLSAddSourceGuideView"});
	ui = pls_new<Ui::PLSAddSourceGuideView>();
	setupUi(ui);
	PLS_PERFORMANCE_END(addsourceSetup);

	m_sourceTabButtonGroup->setExclusive(true);
	setResizeEnabled(false);
	initSize(935, 660);
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("Source.Guide.Window.Title"));
#elif defined(Q_OS_WIN)
	setWindowTitle("");
#endif

	PLS_PERFORMANCE_START(addsource_initTabList);
	m_sourceTabList = initSourceTabList();
	PLS_PERFORMANCE_END(addsource_initTabList);
	pls_uistep_v2_set_title(this, QStringLiteral("PLSAddSourceGuideView"));
	PLS_DISABLE_UISTEP_V2(this);
	pls_uistep_v2_set_custom_enter_leave_name(ui->helpIcon, "help icon");
	ui->horizontalLayout->setAlignment(ui->guideLayout, Qt::AlignRight);
	ui->horizontalLayout->setAlignment(ui->sourceListLayout, Qt::AlignLeft);
	ui->guideLayout->setAlignment(ui->iconLabel, Qt::AlignRight);
}

void PLSAddSourceGuideView::initUi()
{
	ui->verticalLayout->setAlignment(ui->tabLayout, Qt::AlignTop);
	ui->sourceTitleLabel->setText(tr("Source.Guide.Title"));
	ui->sourceAddButton->setText(tr("Source.Guide.Add"));
	m_sourceChecBox.resize(SOURCE_LIST_DEFAULT_COUNT);
	for (int i = 0; i < SOURCE_LIST_DEFAULT_COUNT; ++i) {
		m_sourceChecBox[i] = new PLSCheckBox(this);
		m_sourceChecBox[i]->setStyleSheet("min-height: 20px; max-height: 20px;");
		m_sourceChecBox[i]->setVisible(false);
		connect(m_sourceChecBox[i], &PLSCheckBox::stateChanged, this, &PLSAddSourceGuideView::sourceListStateChanged, Qt::QueuedConnection);
	}

	connect(m_sourceTabButtonGroup, &QButtonGroup::idClicked, this, &PLSAddSourceGuideView::updateSourceListLayout);

	for (auto sourceTabInter = m_sourceTabList->constBegin(); sourceTabInter != m_sourceTabList->constEnd(); ++sourceTabInter) {
		auto sourceTabName = sourceTabInter->first.first;
		auto sourceTabButton = new QPushButton(tr(sourceTabName), this);
		sourceTabButton->setCheckable(true);
		sourceTabButton->setObjectName("sourceTab");
		sourceTabButton->setFlat(true);
		ui->tabLayout->addWidget(sourceTabButton);
		m_sourceTabButtonGroup->addButton(sourceTabButton, sourceTabInter - m_sourceTabList->constBegin());
		auto id = QString(sourceTabName).split('.').last();
		auto resName = id.toLower();
		sourceTabButton->setProperty("id", id);
		m_iconPath.append(QString(iconPath).arg(resName).arg("en"));
	}
	ui->tabLayout->setAlignment(Qt::AlignHCenter);
	ui->sourceListLayout->setAlignment(Qt::AlignTop);
	if (auto firstButton = m_sourceTabButtonGroup->button(0)) {
		firstButton->setChecked(true);
		updateSourceListLayout(0);
	}
	pls_uistep_v2_tab(ui->tabLayout->children(), QStringLiteral("clicked"));
}

PLSAddSourceGuideView::~PLSAddSourceGuideView()
{
	delete ui;
}

QVector<QPair<QPair<const char *, bool>, QVector<SourceGuideAttr>>> *PLSAddSourceGuideView::initSourceTabList()
{
	static QVector<QPair<QPair<const char *, bool>, QVector<SourceGuideAttr>>> sourceTabList = {
		{{"Source.Guide.Tab.Chatting", false},
		 QVector<SourceGuideAttr>{{PRISM_TEXT_TEMPLATE_ID, pls_source_get_display_name(PRISM_TEXT_TEMPLATE_ID)},
					  {PRISM_STICKER_SOURCE_ID, pls_source_get_display_name(PRISM_STICKER_SOURCE_ID)},
					  {PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
#if defined(Q_OS_WIN)
					  {WINDOW_SOURCE_ID, pls_source_get_display_name(WINDOW_SOURCE_ID)},
#elif defined(Q_OS_MACOS)
					  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, pls_source_get_display_name(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)},
#endif
					  {BROWSER_SOURCE_ID, pls_source_get_display_name(BROWSER_SOURCE_ID)},
					  {IMAGE_SOURCE_ID, pls_source_get_display_name(IMAGE_SOURCE_ID)},
					  {BGM_SOURCE_ID, pls_source_get_display_name(BGM_SOURCE_ID)},
					  {PRISM_LENS_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_SOURCE_ID)},
					  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, pls_source_get_display_name(PLATFORM_VIDEO_CAPTURE_SOURCE_ID), false},
					  {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)}}},
		{{"Source.Guide.Tab.Game", GAME_SHOW_TOOLTIP},
		 QVector<SourceGuideAttr>{
			 {PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
			 {PRISM_LENS_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_SOURCE_ID)},
			 {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, pls_source_get_display_name(PLATFORM_VIDEO_CAPTURE_SOURCE_ID), false},
#if defined(Q_OS_WIN)
			 {GAME_SOURCE_ID, pls_source_get_display_name(GAME_SOURCE_ID)},
			 {PRISM_MONITOR_SOURCE_ID, pls_source_get_display_name(PRISM_MONITOR_SOURCE_ID), false},
			 {WINDOW_SOURCE_ID, pls_source_get_display_name(WINDOW_SOURCE_ID), false},
#elif defined(Q_OS_MACOS)
			 {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, pls_source_get_display_name(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)},
#endif
			 {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)},
		 }},
		{{"Source.Guide.Tab.Mobile", true},
		 QVector<SourceGuideAttr>{{PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
					  {PRISM_LENS_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_SOURCE_ID)},
					  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, pls_source_get_display_name(PLATFORM_VIDEO_CAPTURE_SOURCE_ID), false},
					  {PRISM_LENS_MOBILE_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_MOBILE_SOURCE_ID)},
					  {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)}}},
		{{"Source.Guide.Tab.Vtuber", true},
		 QVector<SourceGuideAttr>{
#if defined(Q_OS_WIN)
			 {PRISM_STICKER_SOURCE_ID, pls_source_get_display_name(PRISM_STICKER_SOURCE_ID)},
			 {WINDOW_SOURCE_ID, pls_source_get_display_name(WINDOW_SOURCE_ID), false},
			 {PRISM_MONITOR_SOURCE_ID, pls_source_get_display_name(PRISM_MONITOR_SOURCE_ID), false},
#endif
			 {PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
			 {PRISM_LENS_MOBILE_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_MOBILE_SOURCE_ID), false},
#if defined(Q_OS_WIN)
			 {GAME_SOURCE_ID, pls_source_get_display_name(GAME_SOURCE_ID)},
			 {OBS_INPUT_SPOUT_CAPTURE_ID, pls_source_get_display_name(OBS_INPUT_SPOUT_CAPTURE_ID)},
#elif defined(Q_OS_MACOS)
			 {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, pls_source_get_display_name(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)},
#endif
			 {PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, pls_source_get_display_name(PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)},
			 {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)}}},
		{{"Source.Guide.Tab.Shopping", false},
		 QVector<SourceGuideAttr>{{PRISM_STICKER_SOURCE_ID, pls_source_get_display_name(PRISM_STICKER_SOURCE_ID)},
					  {PRISM_TIMER_SOURCE_ID, pls_source_get_display_name(PRISM_TIMER_SOURCE_ID)},
					  {GDIP_TEXT_SOURCE_ID, pls_source_get_display_name(GDIP_TEXT_SOURCE_ID)},
#if defined(Q_OS_WIN)
					  {WINDOW_SOURCE_ID, pls_source_get_display_name(WINDOW_SOURCE_ID)},
#elif defined(Q_OS_MACOS)
					  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, pls_source_get_display_name(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)},
#endif
					  {BROWSER_SOURCE_ID, pls_source_get_display_name(BROWSER_SOURCE_ID)},
					  {SLIDESHOW_SOURCE_ID, pls_source_get_display_name(SLIDESHOW_SOURCE_ID)},
					  {IMAGE_SOURCE_ID, pls_source_get_display_name(IMAGE_SOURCE_ID)},
					  {PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
					  {PRISM_LENS_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_SOURCE_ID)},
					  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, pls_source_get_display_name(PLATFORM_VIDEO_CAPTURE_SOURCE_ID), false},
					  {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)}}},
		{{"Source.Guide.Tab.Presentation", false},
		 QVector<SourceGuideAttr>{{PRISM_STICKER_SOURCE_ID, pls_source_get_display_name(PRISM_STICKER_SOURCE_ID)},
					  {GDIP_TEXT_SOURCE_ID, pls_source_get_display_name(GDIP_TEXT_SOURCE_ID)},
#if defined(Q_OS_WIN)
					  {WINDOW_SOURCE_ID, pls_source_get_display_name(WINDOW_SOURCE_ID)},
					  {PRISM_MONITOR_SOURCE_ID, pls_source_get_display_name(PRISM_MONITOR_SOURCE_ID), false},
#elif defined(Q_OS_MACOS)
					  {OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID, pls_source_get_display_name(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID)},
#endif
					  {SLIDESHOW_SOURCE_ID, pls_source_get_display_name(SLIDESHOW_SOURCE_ID)},
					  {IMAGE_SOURCE_ID, pls_source_get_display_name(IMAGE_SOURCE_ID)},
					  {PRISM_CHATV2_SOURCE_ID, pls_source_get_display_name(PRISM_CHATV2_SOURCE_ID)},
					  {PRISM_LENS_SOURCE_ID, pls_source_get_display_name(PRISM_LENS_SOURCE_ID)},
					  {PLATFORM_VIDEO_CAPTURE_SOURCE_ID, pls_source_get_display_name(PLATFORM_VIDEO_CAPTURE_SOURCE_ID), false},
					  {AUDIO_INPUT_SOURCE_ID, pls_source_get_display_name(AUDIO_INPUT_SOURCE_ID)}}}};
	return &sourceTabList;
}

const QStringList &PLSAddSourceGuideView::selectSourceList()
{
	return m_selectSourceList;
}

void PLSAddSourceGuideView::showEvent(QShowEvent *event)
{
#if defined(Q_OS_MACOS)
	titleBar()->show();
	titleLabel()->hide();
	closeButton()->hide();
#endif

	PLSDialogView::showEvent(event);
	pls_async_call_mt([this]() { initUi(); });
}

void PLSAddSourceGuideView::updateSourceListLayout(int id)
{
	if (!m_sourceTabList || id < 0 || id >= m_sourceTabList->size() || id == m_currentTabIndex)
		return;
	m_currentTabIndex = id;
	ui->iconLabel->hide();
	auto pixmap = pls_load_pixmap_with_mode(m_iconPath.value(id), QSize(500, 312) * 3);

	ui->iconLabel->setPixmap(pixmap);

	QLayoutItem *item;
	while ((item = ui->sourceListLayout->takeAt(0)) != nullptr) {
		if (item->widget()) {
			item->widget()->setVisible(false);
		}
		delete item;
	}

	const QVector<SourceGuideAttr> &sourceGuideAttrs = m_sourceTabList->at(id).second;
	int itemCount = sourceGuideAttrs.size();

	while (m_sourceChecBox.size() < itemCount) {
		auto checkbox = new PLSCheckBox(this);
		checkbox->setStyleSheet("min-height: 20px; max-height: 20px;");
		m_sourceChecBox.append(checkbox);
		connect(checkbox, &PLSCheckBox::stateChanged, this, &PLSAddSourceGuideView::sourceListStateChanged, Qt::QueuedConnection);
	}

	for (int i = 0; i < itemCount; i++) {
		const auto &sourceGuidAttr = sourceGuideAttrs[i];
		m_sourceChecBox[i]->setText(sourceGuidAttr.displayName);
		m_sourceChecBox[i]->setProperty("sourceId", QString::fromUtf8(sourceGuidAttr.sourceId));
		auto sourceIsChecked = m_sourceChecBox[i]->property(m_sourceTabList->at(id).first.first);
		pls_uistep_v2_set_title(m_sourceChecBox[i], [this]() { return pls_uistep_v2_get_english(m_sourceTabList->at(m_currentTabIndex).first.first); });
		if (sourceIsChecked.isNull() || !sourceIsChecked.isValid()) {
			m_sourceChecBox[i]->setChecked(sourceGuidAttr.isChecked);
		} else {
			m_sourceChecBox[i]->setChecked(sourceIsChecked.toBool());
		}
		m_sourceChecBox[i]->setVisible(true);
		ui->sourceListLayout->addWidget(m_sourceChecBox[i]);
	}

	for (int i = itemCount; i < m_sourceChecBox.size(); i++) {
		m_sourceChecBox[i]->setVisible(false);
	}
	ui->sourceListLayout->addWidget(ui->tooltipFrame);
	ui->tooltipFrame->setVisible(m_sourceTabList->at(id).first.second);
	auto tabId = m_sourceTabButtonGroup->button(id)->property("id").toString();
#if defined(Q_OS_WIN)
	ui->helpIcon->setToolTip(tr(QString(guideTooltipWin).arg(tabId).toUtf8().constData()));
#elif defined(Q_OS_MACOS)
	ui->helpIcon->setToolTip(tr(QString(guideTooltipMac).arg(tabId).toUtf8().constData()));

#endif
	ui->iconLabel->show();
	PLS_UI_ACTION("show %s source list view", pls_uistep_v2_get_english(m_sourceTabList->at(id).first.first).toUtf8().constData());
}

void PLSAddSourceGuideView::on_sourceAddButton_clicked()
{
	m_selectSourceList.clear();
	for (auto checkBox : std::as_const(m_sourceChecBox)) {
		if (!checkBox || !checkBox->isVisible())
			continue;

		const auto sourceId = checkBox->property("sourceId").toString();
		if (sourceId.isEmpty())
			continue;

		if (checkBox->isChecked()) {
			m_selectSourceList.insert(0, sourceId);
		}
	}
	auto filterSourceList = PLSBasic::instance()->getFilterSourceList(m_selectSourceList);
	PLSAlertView::Result resultVale;
	if (!filterSourceList.isEmpty()) {
		QSet<QString> filterSet(filterSourceList.begin(), filterSourceList.end());
		QSet<QString> selectSet(m_selectSourceList.begin(), m_selectSourceList.end());
		PLSErrorHandler::ExtraData extraData("on_sourceAddButton_clicked");
		extraData.propertiesMap = {{"isChecked", true}};
		PLSErrorHandler::RetData retData;
		if (filterSet.contains(selectSet)) {
			PLS_UI_ACTION("close source guide view and show alert");

			retData = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SOURCE_GUIDE_ALL_SAME, PLSErrKeyAllAlert, {}, extraData, this);
		} else {
			PLS_UI_ACTION("close source guide view and show alert");
			retData = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SOURCE_GUIDE_PARTIAL_DUPLICATE, PLSErrKeyAllAlert, {}, extraData, this);
		}
		if (retData.clickedBtn != QDialogButtonBox::Yes) {
			pls_modal_check_app_exiting();
			return;
		} else if (retData.isCheckBoxClick) {
			for (auto it = m_selectSourceList.begin(); it != m_selectSourceList.end();) {
				if (filterSourceList.contains(*it)) {
					it = m_selectSourceList.erase(it);
				} else {
					++it;
				}
			}
		}
	}
	accept();
	PLS_UI_ACTION("close source guide view");
}

void PLSAddSourceGuideView::sourceListStateChanged(int status)
{
	auto checkBox = qobject_cast<PLSCheckBox *>(sender());
	if (checkBox) {
		auto currentTabName = m_sourceTabList->at(m_currentTabIndex).first.first;
		checkBox->setProperty(currentTabName, status != 0);
	}
	bool isAllUnCheck = true;
	for (auto checkbox : m_sourceChecBox) {
		if (checkbox->isVisible() && checkbox->isChecked()) {
			isAllUnCheck = false;
			break;
		}
	}

	ui->sourceAddButton->setEnabled(!isAllUnCheck);
}
