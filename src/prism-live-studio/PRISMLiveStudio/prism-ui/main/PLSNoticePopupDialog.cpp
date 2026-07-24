#include "PLSNoticePopupDialog.hpp"
#include "ui_PLSNoticePopupDialog.h"
#include "window-basic-main.hpp"
#include "libui.h"
#include "log/log.h"
#include "login-user-info.hpp"
#include "PLSNoticeUpdateTypes.hpp"
#include "PLSNoticeUpdateRepository.hpp"
#include "PLSMainView.hpp"
#include "PLSTextLoadingView.h"
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QList>
namespace {

constexpr int kIconSize = 10;
constexpr int kNoticeTabHeight = 55;
constexpr int kNoticeTabHorizontalPadding = 17;

class PLSNoticeTabButton : public QPushButton {
public:
	explicit PLSNoticeTabButton(QWidget *parent = nullptr) : QPushButton(parent)
	{
		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		setFlat(true);
		setAutoDefault(false);
		setDefault(false);
	}

	QSize sizeHint() const override { return minimumSizeHint(); }

	QSize minimumSizeHint() const override
	{
		const QString &txt = text();
		if (txt.isEmpty())
			return QSize(kNoticeTabHorizontalPadding * 2, kNoticeTabHeight);

		QFont normalFont = font();
		const QFontMetrics fmNormal(normalFont);
		int textW = fmNormal.horizontalAdvance(txt);
		QFont boldFont = normalFont;
		boldFont.setBold(true);
		const QFontMetrics fmBold(boldFont);
		textW = qMax(textW, fmBold.horizontalAdvance(txt));
		return QSize(textW + kNoticeTabHorizontalPadding * 2, kNoticeTabHeight);
	}

protected:
	void changeEvent(QEvent *e) override
	{
		QPushButton::changeEvent(e);
		if (e->type() == QEvent::FontChange || e->type() == QEvent::LanguageChange)
			updateGeometry();
	}
};

class PLSNoticeTextIconButton : public QPushButton {
public:
	explicit PLSNoticeTextIconButton(QWidget *parent = nullptr) : QPushButton(parent)
	{
		setObjectName("NoticeTextIconButton");
		setCursor(Qt::PointingHandCursor);
		setFlat(true);
		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		setProperty("hovered", false);
		setProperty("pressed", false);
		m_textLabel = new QLabel(this);
		m_textLabel->setObjectName("textLabel");
		m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		m_iconLabel = new QLabel(this);
		m_iconLabel->setObjectName("iconLabel");
		m_iconLabel->setFixedSize(kIconSize, kIconSize);
		m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		auto *layout = new QHBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(3);
		layout->addWidget(m_textLabel);
		layout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
	}

	void setButtonText(const QString &text)
	{
		m_textLabel->setText(text);
		update();
	}

	QSize sizeHint() const override
	{
		const int textW = m_textLabel->sizeHint().width();
		const int total = textW + 3 + kIconSize;
		return QSize(total, 18);
	}

protected:
	bool event(QEvent *e) override
	{
		switch (e->type()) {
		case QEvent::Enter:
			setProperty("hovered", true);
			pls_flush_style_recursive(this);
			break;
		case QEvent::Leave:
			setProperty("hovered", false);
			setProperty("pressed", false);
			pls_flush_style_recursive(this);
			break;
		case QEvent::MouseButtonPress:
			setProperty("pressed", true);
			pls_flush_style_recursive(this);
			break;
		case QEvent::MouseButtonRelease:
			setProperty("pressed", false);
			pls_flush_style_recursive(this);
			break;
		default:
			break;
		}
		return QPushButton::event(e);
	}

private:
	QLabel *m_textLabel = nullptr;
	QLabel *m_iconLabel = nullptr;
};

} // namespace

static QString contentUrlForPopup(const PLSNoticeUpdateItem &item)
{
	return !item.contentPopUrl.isEmpty() ? item.contentPopUrl : item.contentDetailUrl;
}

QString PLSNoticePopupDialog::noticePopupTopLabelText() const
{
	if (m_items.isEmpty())
		return {};
	if (m_noticeProvider == PLSNoticeProvider::B2B && m_items.size() > 1)
		return {};
	if (m_noticeProvider == PLSNoticeProvider::PRISM) {
		const auto &item = m_items.first();
		if (item.category == PLSNoticeCategory::Update)
			return tr("Notice.Popup.TopLabel.PrismUpdate");
		return tr("Notice.Popup.TopLabel.PrismNotice");
	}
	if (m_noticeProvider == PLSNoticeProvider::B2B) {
		QString serviceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
		if (serviceName.isEmpty())
			serviceName = QStringLiteral("B2B");
		return tr("Notice.B2B.TopLabel").arg(serviceName);
	}
	return {};
}

QList<PLSNoticeUpdateItem> PLSNoticePopupDialog::visitedItems() const
{
	QList<PLSNoticeUpdateItem> items;
	for (int index = 0; index < m_items.size(); ++index) {
		if (m_visitedIndexes.contains(index))
			items.append(m_items.at(index));
	}
	return items;
}

bool PLSNoticePopupDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (ui && watched == ui->middleWidget && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
		updateContentLoadingGeometry();
	}
	return PLSDialogView::eventFilter(watched, event);
}

int PLSNoticePopupDialog::indexForProvider(PLSNoticeProvider provider) const
{
	for (int index = 0; index < m_items.size(); ++index) {
		if (m_items.at(index).provider == provider)
			return index;
	}
	return -1;
}

void PLSNoticePopupDialog::markCurrentNoticeVisited()
{
	if (m_selectTab >= 0 && m_selectTab < m_items.size())
		m_visitedIndexes.insert(m_selectTab);
}

void PLSNoticePopupDialog::syncDualNoticeControls()
{
	if (m_noticeProvider != PLSNoticeProvider::B2B || m_items.size() <= 1)
		return;

	const bool showingPrism = m_items.value(m_selectTab).provider == PLSNoticeProvider::PRISM;
	ui->confirmButton->setText(showingPrism ? tr("Close") : tr("Next"));
	ui->learnMoreButton->setVisible(showingPrism);
	ui->learnMoreButton->setText(tr("Back"));
}

void PLSNoticePopupDialog::switchToNoticeIndex(int index)
{
	if (index < 0 || index >= m_items.size())
		return;

	const bool changed = m_selectTab != index;
	m_selectTab = index;
	markCurrentNoticeVisited();

	if (m_noticeProvider == PLSNoticeProvider::B2B && m_items.size() > 1) {
		if (auto *b2bTab = m_tabGroup.button(static_cast<int>(PLSNoticeProvider::B2B)))
			b2bTab->setChecked(m_items.at(index).provider == PLSNoticeProvider::B2B);
		if (auto *prismTab = m_tabGroup.button(static_cast<int>(PLSNoticeProvider::PRISM)))
			prismTab->setChecked(m_items.at(index).provider == PLSNoticeProvider::PRISM);
		syncDualNoticeControls();
	}

	if (changed)
		updateContent(contentUrlForPopup(m_items.at(index)));
}

QList<QList<PLSNoticeUpdateItem>> PLSNoticePopupDialog::buildNoticePopupPayloads(const QList<PLSNoticeUpdateItem> &noticeInfos, bool isB2BMode)
{
	QList<PLSNoticeUpdateItem> b2bItems;
	QList<PLSNoticeUpdateItem> prismItems;
	for (const auto &item : noticeInfos) {
		if (item.id.isEmpty() || item.category != PLSNoticeCategory::Notice)
			continue;
		if (item.provider == PLSNoticeProvider::B2B)
			b2bItems.append(item);
		else if (item.provider == PLSNoticeProvider::PRISM)
			prismItems.append(item);
	}

	QList<QList<PLSNoticeUpdateItem>> payloads;
	if (!isB2BMode) {
		for (const auto &item : prismItems) {
			QList<PLSNoticeUpdateItem> payload;
			payload.append(item);
			payloads.append(payload);
		}
		return payloads;
	}

	int b2bIndex = 0;
	int prismIndex = 0;
	while (b2bIndex < b2bItems.size() || prismIndex < prismItems.size()) {
		QList<PLSNoticeUpdateItem> payload;
		if (b2bIndex < b2bItems.size())
			payload.append(b2bItems.at(b2bIndex++));
		if (prismIndex < prismItems.size())
			payload.append(prismItems.at(prismIndex++));
		if (!payload.isEmpty())
			payloads.append(payload);
	}
	return payloads;
}

bool PLSNoticePopupDialog::showNoticePopupQueue(const QList<PLSNoticeUpdateItem> &noticeInfos, QWidget *parent, PLSMainView *mainView, PLSNoticeFilter *outOpenCenterFilter)
{
	if (noticeInfos.isEmpty())
		return false;

	PLSNoticeUpdateRepository *repo = PLSNoticeUpdateRepository::instance();
	const bool isB2BMode = !PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName().isEmpty();
	bool openCenterRequested = false;
	PLSNoticeFilter requestedFilter = PLSNoticeFilter::PrismOnly;
	const auto unreadNoticeInfos = repo->filterUnreadNoticeItems(noticeInfos);
	const auto payloads = buildNoticePopupPayloads(unreadNoticeInfos, isB2BMode);
	if (payloads.isEmpty())
		return false;

	const auto &payload = payloads.first();
	if (payload.isEmpty() || pls_is_main_window_closing())
		return false;

	const auto popupProvider = payload.size() > 1 ? PLSNoticeProvider::B2B : payload.first().provider;
	PLSNoticePopupDialog dlg(payload, popupProvider, parent);
	dlg.exec();
	const auto visitedItems = dlg.visitedItems();
	for (const auto &noticeInfo : visitedItems)
		repo->markItemAsRead(noticeInfo);
	if (mainView)
		mainView->setNoticeTips(repo->hasUnreadNotice(isB2BMode));
	if (dlg.m_openCenterRequested) {
		openCenterRequested = true;
		requestedFilter = dlg.m_requestedCenterFilter;
	}
	if (openCenterRequested) {
		if (outOpenCenterFilter)
			*outOpenCenterFilter = requestedFilter;
	}
	return openCenterRequested;
}

void PLSNoticePopupDialog::setupCommon()
{
	auto initHeight = 460;
	for (auto item : m_items) {
		if (item.imageIncluded) {
			initHeight = 680;
			break;
		}
	}
	setFixedSize(650, initHeight);
	initSize(650, initHeight);
	auto closeEvent = [this](QCloseEvent *event) -> bool {
		if (m_browserWidget) {
			m_browserWidget->closeBrowser();
		}
		if (m_noticeProvider == PLSNoticeProvider::UPDATE && event) {
			reject();
			return true;
		}
		hide();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSNoticePopupDialog::PLSNoticePopupDialog(const QList<PLSNoticeUpdateItem> &items, PLSNoticeProvider provider, QWidget *parent, bool showViewAllButton)
	: PLSDialogView(parent), m_items(items), m_noticeProvider(provider), m_showViewAllButton(showViewAllButton)
{
	pls_uistep_v2_set_title(this, QStringLiteral("PLSNoticePopupDialog"));
	ui = pls_new<Ui::PLSNoticePopupDialog>();
	initUI();
	setupCommon();
}

PLSNoticePopupDialog::PLSNoticePopupDialog(const QString &contentUrl, bool hasUpdate, bool hasForceUpdate, const QString &detailUrl, QWidget *parent)
	: PLSDialogView(parent), m_noticeProvider(PLSNoticeProvider::UPDATE), m_hasUpdate(hasUpdate), m_hasForceUpdate(hasForceUpdate)
{
	PLSNoticeUpdateItem item;
	item.title = m_hasForceUpdate ? tr("Update.Toptip.Force.Text") : tr("Update.Toptip.Advise.Text");
	if (!m_hasUpdate)
		item.title = tr("Update.Lastest.Version.Title");
	item.contentPopUrl = contentUrl;
	item.contentDetailUrl = detailUrl.isEmpty() ? contentUrl : detailUrl;
	m_items.append(item);

	pls_uistep_v2_set_title(this, QStringLiteral("PLSNoticePopupDialog"));
	ui = pls_new<Ui::PLSNoticePopupDialog>();
	initUI();
	setupCommon();
}

PLSNoticePopupDialog::~PLSNoticePopupDialog()
{
	if (ui && ui->middleWidget) {
		ui->middleWidget->removeEventFilter(this);
	}
	hideContentLoading();
	pls_delete(ui);
}

void PLSNoticePopupDialog::createTopLayout()
{
	auto topLayout = new QHBoxLayout(this);
	topLayout->setContentsMargins(25, 0, 29, 0);
	topLayout->setSpacing(0);
	ui->topLabel->setLayout(topLayout);

	if (m_noticeProvider == PLSNoticeProvider::B2B && m_items.size() > 1) {
		ui->topLabel->clear();
		auto b2bTab = new PLSNoticeTabButton(this);
		b2bTab->setCheckable(true);
		b2bTab->setObjectName("NoticeB2B");
		b2bTab->setText(!m_items.isEmpty() && !m_items.first().providerName.isEmpty() ? m_items.first().providerName : QStringLiteral("B2B"));
		auto prismTab = new PLSNoticeTabButton(this);
		prismTab->setCheckable(true);
		prismTab->setObjectName("NoticePRISM");
		prismTab->setText(tr("Notice.PRISM.Tab"));
		b2bTab->setChecked(true);
		m_tabGroup.addButton(b2bTab, static_cast<int>(PLSNoticeProvider::B2B));
		m_tabGroup.addButton(prismTab, static_cast<int>(PLSNoticeProvider::PRISM));
		m_tabGroup.setExclusive(true);
		connect(b2bTab, &QPushButton::clicked, this, [this]() { switchToNoticeIndex(indexForProvider(PLSNoticeProvider::B2B)); });
		connect(prismTab, &QPushButton::clicked, this, [this]() { switchToNoticeIndex(indexForProvider(PLSNoticeProvider::PRISM)); });
		topLayout->addWidget(b2bTab, Qt::AlignLeft);
		topLayout->addWidget(prismTab, Qt::AlignLeft);
		topLayout->addStretch(1);
	} else if (m_noticeProvider == PLSNoticeProvider::UPDATE) {
		ui->topLabel->setText(m_items.first().title);

		ui->learnMoreButton->setText(m_hasForceUpdate ? tr("Update.Bottom.ExitApp.Button.Text") : tr("Update.Bottom.Next.Button.Text"));
		ui->confirmButton->setText(m_hasForceUpdate ? tr("Confirm") : tr("Update.Bottom.Force.Button.Text"));
		if (!m_hasUpdate) {
			ui->confirmButton->setText(tr("OK"));
			ui->learnMoreButton->setVisible(false);
		}
	} else {
		ui->topLabel->setText(noticePopupTopLabelText());
		ui->learnMoreButton->setVisible(false);
	}
	auto *topViewAllBtn = new PLSNoticeTextIconButton(ui->topWidget);
	topViewAllBtn->setButtonText(tr("Notice.Extern.Link"));
	topViewAllBtn->setVisible(m_showViewAllButton && m_noticeProvider != PLSNoticeProvider::UPDATE);
	connect(topViewAllBtn, &QPushButton::clicked, this, [this]() {
		PLSNoticeFilter initialFilter = PLSNoticeFilter::PrismOnly;
		if (m_noticeProvider == PLSNoticeProvider::B2B && !m_items.isEmpty()) {
			const auto &currentItem = m_items.value(m_selectTab);
			initialFilter = currentItem.provider == PLSNoticeProvider::B2B ? PLSNoticeFilter::B2BOnly : PLSNoticeFilter::PrismOnly;
		} else if (!m_items.isEmpty() && m_items.first().provider == PLSNoticeProvider::B2B) {
			initialFilter = PLSNoticeFilter::B2BOnly;
		}
		m_openCenterRequested = true;
		m_requestedCenterFilter = initialFilter;
		accept();
	});
	topLayout->addWidget(topViewAllBtn, 0, Qt::AlignRight);
}

void PLSNoticePopupDialog::initUI()
{
	//setup view rect and content
	setupUi(ui);
	setResizeEnabled(false);
	pls_add_css(this, {"PLSNoticePopupDialog"});
	createTopLayout();
	ui->middleWidget->installEventFilter(this);

	QString initialUrl;
	if (!m_items.isEmpty())
		initialUrl = contentUrlForPopup(m_items.first());
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(initialUrl)
								 .allowPopups(false)
								 .initBkgColor(QColor(30, 30, 30))
								 .showAtLoadEnded(true)
								 .loadEnded([this](pls::browser::Browser *) { hideContentLoading(); }));
	ui->horizontalMiddleLayout->addWidget(m_browserWidget);
	if (!initialUrl.isEmpty())
		showContentLoading();

	setWindowTitle(m_noticeProvider == PLSNoticeProvider::UPDATE ? tr("Mac.Title.Update") : tr("Notice.Title"));
	if (m_noticeProvider == PLSNoticeProvider::B2B) {
		if (m_items.size() > 1)
			ui->confirmButton->setText(tr("Next"));
		else
			ui->confirmButton->setText(tr("OK"));
		ui->learnMoreButton->setVisible(false);
	}
	switchToNoticeIndex(0);
}

void PLSNoticePopupDialog::on_confirmButton_clicked() //next
{
	if ((m_noticeProvider == PLSNoticeProvider::B2B && ui->learnMoreButton->isVisible()) || (m_noticeProvider != PLSNoticeProvider::B2B)) {
		accept();
		return;
	}
	// Single B2B: no tabs, first click should close (same as Prism single notice).
	if (m_items.size() <= 1) {
		accept();
		return;
	}
	switchToNoticeIndex(indexForProvider(PLSNoticeProvider::PRISM));
}

void PLSNoticePopupDialog::on_learnMoreButton_clicked() //back
{
	if (m_noticeProvider == PLSNoticeProvider::B2B && m_items.size() > 1) {
		switchToNoticeIndex(indexForProvider(PLSNoticeProvider::B2B));
	} else if (m_noticeProvider == PLSNoticeProvider::UPDATE) {
		reject();
	}
}

void PLSNoticePopupDialog::updateContent(const QString &url)
{
	if (!url.isEmpty()) {
		showContentLoading();
		m_browserWidget->url(url);
	} else {
		hideContentLoading();
		PLS_INFO("PLSNoticeView", "url is empty");
	}
}

void PLSNoticePopupDialog::showContentLoading()
{
	if (!ui || !ui->middleWidget)
		return;

	if (!m_contentLoading) {
		m_contentLoading = pls_new<PLSTextLoadingView>(tr("ResolutionGuide.LoadingMessage"), ui->middleWidget);
	}
	updateContentLoadingGeometry();
	m_contentLoading->show();
	m_contentLoading->raise();
}

void PLSNoticePopupDialog::hideContentLoading()
{
	if (m_contentLoading) {
		pls_delete(m_contentLoading);
		m_contentLoading = nullptr;
	}
}

void PLSNoticePopupDialog::updateContentLoadingGeometry() const
{
	if (m_contentLoading && ui && ui->middleWidget) {
		m_contentLoading->setGeometry(ui->middleWidget->rect());
	}
}
