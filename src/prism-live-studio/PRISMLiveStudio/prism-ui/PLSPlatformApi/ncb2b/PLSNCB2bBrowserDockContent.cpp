#include "PLSNCB2bBrowserDockContent.h"
#include "ui_PLSNCB2bBrowserDockContent.h"
#include "libutils-api.h"
#include "PLSPushButton.h"
#include "libui.h"
#include "frontend-api.h"
#include "PLSLoginDataHandler.h"
#include "PLSBasic.h"
#include <QScrollBar>
#include <QDesktopServices>

static const char *ncb2bBrowserSettingsModuleName = "PLSNCB2bBrowserDockContent";
static const QColor DEFAULT_BROWSER_BKG_COLOR(17, 17, 17);
static const QString CATEGORY_BUTTON_OBJECT_NAME = QStringLiteral("categoryButton");
static const char *URL_PROPERTY_NAME = "url";
static const int CATEGORY_BUTTON_FIXED_WIDTH = 150;
static const int CATEGORY_BUTTON_LAYOUT_SPACING = 10;

PLSNCB2bBrowserDockContent::PLSNCB2bBrowserDockContent(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSNCB2bBrowserDockContent>();
	ui->setupUi(this);
	pls_add_css(this, {"PLSNCB2bBrowserDockContent"});
	ui->nextFrame->setVisible(false);
	ui->preFrame->setVisible(false);
	btnGroup = pls_new<QButtonGroup>(this);
	connect(btnGroup, &QButtonGroup::idClicked, this, &PLSNCB2bBrowserDockContent::onBtnGroupClicked);
	connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &PLSNCB2bBrowserDockContent::onStackedWidgetCurrentChanged);
	connect(ui->preBtn, &QPushButton::clicked, this, &PLSNCB2bBrowserDockContent::onPreButtonClicked);
	connect(ui->nextBtn, &QPushButton::clicked, this, &PLSNCB2bBrowserDockContent::onNextButtonClicked);
	connect(ui->scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, &PLSNCB2bBrowserDockContent::checkPreNextButtonNeedShow);
	connect(ui->scrollArea->horizontalScrollBar(), &QScrollBar::rangeChanged, this, &PLSNCB2bBrowserDockContent::checkPreNextButtonNeedShow);

	connect(ui->noSourcesLabel, &QLabel::linkActivated, this, [](const QString &link) {
		if (link == "open-naver-cloud-platform") {
			//TODO: @Rainny.liu replace url when ux provided and put in common place.
			pls_async_invoke([]() { QDesktopServices::openUrl(QUrl("")); });
		} else if (link == "open-browser-settings") {
			if (auto basic = PLSBasic::instance(); basic) {
				PLSBasic::instance()->showNcb2bBrowserSettings();
			}
		}
	});

	cefWidget = pls::browser::newBrowserWidget(pls::browser::Params().initBkgColor(DEFAULT_BROWSER_BKG_COLOR).showAtLoadEnded(true));
	ui->verticalLayoutCefWindow->addWidget(cefWidget);
}

PLSNCB2bBrowserDockContent::~PLSNCB2bBrowserDockContent()
{
	pls_delete(ui, nullptr);
}

void PLSNCB2bBrowserDockContent::refreshUI(PLSErrorHandler::ErrCode errCode)
{
	if (errCode == PLSErrorHandler::INVALID) {
		return;
	}
	m_errorCode = errCode;
	if (m_errorCode != PLSErrorHandler::SUCCESS) {
		switchToNoSourcePage();
		return;
	}
	auto selectedDatas = PLSNCB2bBroSettingsManager::instance()->getDatas(true);
	const auto dataCount = selectedDatas.count();
	const auto currentCount = btnGroup->buttons().count();
	const auto minCount = qMin(dataCount, currentCount);

	for (int i = 0; i < minCount; i++) {
		updateUI(selectedDatas[i], i);
	}

	if (dataCount > currentCount) {
		for (int j = currentCount; j < dataCount; j++) {
			createCategoryButton(selectedDatas[j], j);
		}
	} else if (dataCount < currentCount) {
		removeExtraButtons(dataCount, currentCount);
	}

	pls_async_call(this, [this]() {
		check();
		checkPreNextButtonNeedShow();
	});
}

void PLSNCB2bBrowserDockContent::refreshScrollArea()
{
	checkPreNextButtonNeedShow();
}

void PLSNCB2bBrowserDockContent::closeBrowser()
{
	if (cefWidget) {
		cefWidget->closeBrowser();
	}
}

void PLSNCB2bBrowserDockContent::setCheckedTitle(const QString &selectedTitle)
{
	m_checkedTitle = selectedTitle;
}

QString PLSNCB2bBrowserDockContent::getCheckedTitle()
{
	return m_checkedTitle;
}

void PLSNCB2bBrowserDockContent::createCategoryButton(const PLSNCB2bBrowserSettingData &data, int index)
{
	auto btn = pls_new<PLSPushButton>(this);
	btn->setObjectName(CATEGORY_BUTTON_OBJECT_NAME);
	btn->setCheckable(true);
	QFontMetrics fontMetrics(btn->font());
	btn->setFixedWidth(qMin(fontMetrics.horizontalAdvance(data.title), 150));
	btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	btn->setText(data.title);
	btn->setVisible(data.selected);
	btn->setProperty(URL_PROPERTY_NAME, data.url);
	btnGroup->addButton(btn, index);
	if (data.title == m_checkedTitle) {
		onBtnGroupClicked(index);
	}
	ui->categroyHorizontalLayout->insertWidget(index, btn);
}

void PLSNCB2bBrowserDockContent::removeExtraButtons(int keepCount, int totalCount)
{
	const auto buttons = btnGroup->buttons();
	for (int j = keepCount; j < totalCount; j++) {
		if (j < buttons.size()) {
			auto btn = buttons[j];
			btnGroup->removeButton(btn);
			pls_delete(btn);
		}
	}
}

bool PLSNCB2bBrowserDockContent::isValidButtonIndex(int index) const
{
	return index >= 0 && index < btnGroup->buttons().count();
}

void PLSNCB2bBrowserDockContent::switchToNoSourcePage()
{
	switch (m_errorCode) {
	case PLSErrorHandler::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED:
		ui->noSourcesLabel->setText(tr("Ncb2b.Service.Disable.Status"));
		break;
	case PLSErrorHandler::COMMON_NETWORK_ERROR:
		ui->noSourcesLabel->setText(tr("Ncpb2b.Browser.Settings.No.Network.Desc"));
		break;
	default:
		ui->noSourcesLabel->setText(tr("Ncpb2b.Browser.Settings.Empty"));
		break;
	}
	ui->stackedWidget->setCurrentWidget(ui->pageNoSources);
}

double PLSNCB2bBrowserDockContent::getHorScrollBarValue()
{
	auto bar = ui->scrollArea->horizontalScrollBar();
	int tmpValue = ui->buttonFrame->width() / (CATEGORY_BUTTON_FIXED_WIDTH + CATEGORY_BUTTON_LAYOUT_SPACING);
	return (double)tmpValue * CATEGORY_BUTTON_FIXED_WIDTH;
}

void PLSNCB2bBrowserDockContent::updateUI(const PLSNCB2bBrowserSettingData &data, int index)
{
	if (!isValidButtonIndex(index)) {
		return;
	}

	if (PLSPushButton *btn = static_cast<PLSPushButton *>(btnGroup->buttons()[index]); btn) {
		btn->setText(data.title);
		btn->setProperty(URL_PROPERTY_NAME, data.url);
		btn->setChecked(false);
		if (data.title == m_checkedTitle) {
			onBtnGroupClicked(index);
		}
		btn->setVisible(data.selected);
	}
}

void PLSNCB2bBrowserDockContent::check()
{
	const auto buttons = btnGroup->buttons();
	const auto currentCount = buttons.count();

	if (currentCount == 0) {
		m_checkedTitle.clear();
		switchToNoSourcePage();
		return;
	}
	ui->stackedWidget->setCurrentWidget(ui->pageContent);

	bool haveChecked = false;
	for (const auto tmpBtn : buttons) {
		auto btn = static_cast<PLSPushButton *>(tmpBtn);
		if (btn && btn->isVisible() && btn->getOriginalText() == m_checkedTitle) {
			haveChecked = true;
			break;
		}
	}

	if (haveChecked) {
		return;
	}

	m_checkedTitle.clear();
	const auto firstBtn = getFirstVisibleButton();
	if (firstBtn == -1) {
		switchToNoSourcePage();
		return;
	}

	onBtnGroupClicked(firstBtn);
}

int PLSNCB2bBrowserDockContent::getFirstVisibleButton()
{
	const auto buttons = btnGroup->buttons();
	for (int i = 0; i < buttons.count(); ++i) {
		if (buttons[i]->isVisible()) {
			return i;
		}
	}
	return -1;
}

void PLSNCB2bBrowserDockContent::onBtnGroupClicked(int index)
{
	PLS_PERFORMANCE_GLOBAL_START("PLSNCB2bBrowserDockContent::onBtnGroupClicked");
	if (!isValidButtonIndex(index)) {
		return;
	}

	auto btn = static_cast<PLSPushButton *>(btnGroup->buttons()[index]);
	if (!btn || btn->isChecked() && btn->getOriginalText() == m_checkedTitle) {
		return;
	}

	btn->setChecked(true);
	pls_async_call(this, [this, btn]() { scroll_to_category_button(static_cast<QPushButton *>(btn), ui->scrollArea); });
	m_checkedTitle = btn->getOriginalText();

	PLS_PERFORMANCE_GLOBAL_START("cefWidgetLoadUrl", "PLSNCB2bBrowserDockContent::onBtnGroupClicked");
	auto url = btn->property(URL_PROPERTY_NAME).toString();
	if (!url.isEmpty()) {
		cefWidget->url(btn->property(URL_PROPERTY_NAME).toString());
	}
	PLS_PERFORMANCE_GLOBAL_END("cefWidgetLoadUrl");
	PLS_PERFORMANCE_GLOBAL_END("PLSNCB2bBrowserDockContent::onBtnGroupClicked");
}

void PLSNCB2bBrowserDockContent::onStackedWidgetCurrentChanged(int index)
{
	QWidget *widgetByIndex = ui->stackedWidget->widget(index);
	if (widgetByIndex == ui->pageContent) {
		ui->titleFrame->setVisible(true);
	} else if (widgetByIndex == ui->pageNoSources) {
		ui->titleFrame->setVisible(false);
	}
}

void PLSNCB2bBrowserDockContent::onPreButtonClicked()
{
	double val = getHorScrollBarValue();
	auto bar = ui->scrollArea->horizontalScrollBar();
	int newValue = qMax((int)(bar->value() - val), bar->minimum());
	bar->setValue(newValue);
}

void PLSNCB2bBrowserDockContent::onNextButtonClicked()
{
	double val = getHorScrollBarValue();
	auto bar = ui->scrollArea->horizontalScrollBar();
	int newValue = qMin((int)(bar->value() + val), bar->maximum());
	bar->setValue(newValue);
}

void PLSNCB2bBrowserDockContent::checkPreNextButtonNeedShow()
{
	int value = ui->scrollArea->horizontalScrollBar()->value();
	int maxValue = ui->scrollArea->horizontalScrollBar()->maximum();
	int minValue = ui->scrollArea->horizontalScrollBar()->minimum();
	if (maxValue > 0) {
		bool scrollToEnd = (value >= maxValue);
		bool scrollToStart = (value == minValue);
		ui->preFrame->setVisible(!scrollToStart);
		ui->nextFrame->setVisible(!scrollToEnd);
	} else {
		ui->preFrame->setVisible(false);
		ui->nextFrame->setVisible(false);
	}
	ui->horizontalLayout->update();
}
