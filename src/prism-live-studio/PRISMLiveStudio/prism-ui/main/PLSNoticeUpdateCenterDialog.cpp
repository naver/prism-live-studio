#include "PLSNoticeUpdateCenterDialog.hpp"
#include "ui_PLSNoticeUpdateCenterDialog.h"
#include "PLSNoticeUpdateRepository.hpp"
#include "PLSTextLoadingView.h"
#include "login-user-info.hpp"
#include "libutils-api.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QVBoxLayout>
#include "PLSNoticePopupDialog.hpp"
#include "PLSMainView.hpp"

#include <algorithm>
#include <memory>

namespace {

constexpr int UI_PAGE_SIZE = 15;

class PLSNoticeUpdateListItem : public QPushButton {
public:
	explicit PLSNoticeUpdateListItem(const PLSNoticeUpdateItem &item, QWidget *parent = nullptr) : QPushButton(parent), m_item(item)
	{
		setObjectName("NoticeUpdateListItem");
		setFlat(true);
		setCursor(Qt::PointingHandCursor);
		setFocusPolicy(Qt::NoFocus);
		m_titleLabel = new QLabel(this);
		m_titleLabel->setObjectName("itemTitle");
		m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		m_dateLabel = new QLabel(this);
		m_dateLabel->setObjectName("itemDate");
		m_dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		QString dateStr;
		if (item.publishAtMs > 0) {
			dateStr = QDateTime::fromMSecsSinceEpoch(item.publishAtMs).toString(QStringLiteral("yyyy.MM.dd"));
		}
		m_titleLabel->setText(item.title);
		m_dateLabel->setText(dateStr);
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(20, 15, 8, 15);
		layout->setSpacing(3);
		layout->addWidget(m_titleLabel);
		layout->addWidget(m_dateLabel);
		setToolTip(item.title);
	}

	const PLSNoticeUpdateItem &item() const { return m_item; }

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QPushButton::resizeEvent(event);
		QString text = this->fontMetrics().elidedText(m_item.title, Qt::ElideRight, m_titleLabel->width());
		m_titleLabel->setText(text);
	}

private:
	PLSNoticeUpdateItem m_item;
	QLabel *m_titleLabel{nullptr};
	QLabel *m_dateLabel{nullptr};
};

} // namespace

PLSNoticeUpdateCenterDialog::PLSNoticeUpdateCenterDialog(QWidget *parent) : PLSDialogView(parent), ui(pls_new<Ui::PLSNoticeUpdateCenterDialog>())
{
	m_repository = PLSNoticeUpdateRepository::instance();
	m_mainTabGroup = new QButtonGroup(this);
	m_subTabGroup = new QButtonGroup(this);
	setupUi(ui);
	setResizeEnabled(false);
	pls_add_css(this, {"PLSNoticeUpdateCenterDialog"});
	setFixedSize(650, 460);
	initSize(650, 460);

	m_mainTabGroup->setExclusive(true);
	m_mainTabGroup->addButton(ui->b2bTabButton, static_cast<int>(PLSNoticeFilter::B2BOnly));
	m_mainTabGroup->addButton(ui->prismTabButton, static_cast<int>(PLSNoticeFilter::PrismOnly));

	m_subTabGroup->setExclusive(true);
	m_subTabGroup->addButton(ui->noticeSubButton, static_cast<int>(PLSNoticeCategory::Notice));
	m_subTabGroup->addButton(ui->updateSubButton, static_cast<int>(PLSNoticeCategory::Update));

	connect(ui->b2bTabButton, &QPushButton::clicked, this, &PLSNoticeUpdateCenterDialog::onB2BTabClicked);
	connect(ui->prismTabButton, &QPushButton::clicked, this, &PLSNoticeUpdateCenterDialog::onPrismTabClicked);
	connect(ui->noticeSubButton, &QPushButton::clicked, this, &PLSNoticeUpdateCenterDialog::onNoticeSubClicked);
	connect(ui->updateSubButton, &QPushButton::clicked, this, &PLSNoticeUpdateCenterDialog::onUpdateSubClicked);
	connect(ui->closeButton, &QPushButton::clicked, this, &PLSNoticeUpdateCenterDialog::onCloseClicked);
	connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
		QScrollBar *bar = ui->scrollArea->verticalScrollBar();
		if (bar->maximum() > 0 && value >= bar->maximum() - 20)
			loadMore(m_filter, m_category);
	});

	auto closeEvent = [this](QCloseEvent *) -> bool {
		emit centerClosed();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSNoticeUpdateCenterDialog::~PLSNoticeUpdateCenterDialog()
{
	hideListTextLoading();
	pls_delete(ui);
}

void PLSNoticeUpdateCenterDialog::openWithState()
{
	const QString ncpServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	openWithState(!ncpServiceName.isEmpty() ? PLSNoticeFilter::B2BOnly : PLSNoticeFilter::PrismOnly, PLSNoticeCategory::Notice, true, true);
}

void PLSNoticeUpdateCenterDialog::openWithState(PLSNoticeFilter initialFilter, PLSNoticeCategory initialCategory, bool isApiRequest, bool prefetchPeerCache)
{
	const QString ncpServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	m_b2bMode = !ncpServiceName.isEmpty();
	m_b2bName = m_b2bMode ? ncpServiceName : QString();
	m_isOnlyPrismNotice = !m_b2bMode;

	m_filter = m_b2bMode ? initialFilter : PLSNoticeFilter::PrismOnly;
	m_category = initialCategory;

	if (!m_b2bMode) {
		ui->b2bTabButton->setText(ui->noticeSubButton->text());
		ui->prismTabButton->setText(ui->updateSubButton->text());
		ui->b2bTabButton->setChecked(m_category == PLSNoticeCategory::Notice);
		ui->prismTabButton->setChecked(m_category == PLSNoticeCategory::Update);
	} else {
		ui->b2bTabButton->setText(ncpServiceName);
		ui->prismTabButton->setText(tr("Notice.PRISM.Tab"));
		ui->b2bTabButton->setChecked(m_filter == PLSNoticeFilter::B2BOnly);
		ui->prismTabButton->setChecked(m_filter == PLSNoticeFilter::PrismOnly);
		ui->noticeSubButton->setChecked(m_category == PLSNoticeCategory::Notice);
		ui->updateSubButton->setChecked(m_category == PLSNoticeCategory::Update);
	}

	updateSubHeaderVisibility();
	refreshList(m_filter, m_category, isApiRequest, prefetchPeerCache);
}

void PLSNoticeUpdateCenterDialog::updateSubHeaderVisibility()
{
	ui->subHeaderWidget->setVisible(m_b2bMode && m_mainTabGroup->checkedId() == static_cast<int>(PLSNoticeFilter::PrismOnly));
}

void PLSNoticeUpdateCenterDialog::onB2BTabClicked()
{
	updateSubHeaderVisibility();
	if (m_isOnlyPrismNotice) {
		onNoticeSubClicked();
		return;
	}
	const bool sameFilter = m_filter == PLSNoticeFilter::B2BOnly;
	m_filter = PLSNoticeFilter::B2BOnly;
	m_category = PLSNoticeCategory::Notice;
	ui->noticeSubButton->setChecked(true);
	const bool shouldRequest = sameFilter;
	refreshList(m_filter, m_category, shouldRequest, false, sameFilter);
}

void PLSNoticeUpdateCenterDialog::onPrismTabClicked()
{
	updateSubHeaderVisibility();
	if (m_isOnlyPrismNotice) {
		onUpdateSubClicked();
		return;
	}
	const bool sameFilter = m_filter == PLSNoticeFilter::PrismOnly;
	m_filter = PLSNoticeFilter::PrismOnly;
	m_category = PLSNoticeCategory::Notice;
	ui->noticeSubButton->setChecked(true);
	const bool shouldRequest = sameFilter;
	refreshList(m_filter, m_category, shouldRequest, false, sameFilter);
}

void PLSNoticeUpdateCenterDialog::onNoticeSubClicked()
{
	const bool sameCategory = m_category == PLSNoticeCategory::Notice;
	m_category = PLSNoticeCategory::Notice;
	m_filter = PLSNoticeFilter::PrismOnly;
	const bool shouldRequest = sameCategory;
	refreshList(m_filter, m_category, shouldRequest, false, sameCategory);
}

void PLSNoticeUpdateCenterDialog::onUpdateSubClicked()
{
	const bool sameCategory = m_category == PLSNoticeCategory::Update;
	m_category = PLSNoticeCategory::Update;
	m_filter = PLSNoticeFilter::PrismOnly;
	const bool shouldRequest = sameCategory;
	refreshList(m_filter, m_category, shouldRequest, false, sameCategory);
}

void PLSNoticeUpdateCenterDialog::onCloseClicked()
{
	accept();
	emit centerClosed();
}

void PLSNoticeUpdateCenterDialog::refreshList(PLSNoticeFilter filter, PLSNoticeCategory category, bool isApiRequest, bool prefetchPeerCache, bool preserveCurrentList)
{
	if (!m_repository) {
		showEmptyView();
		return;
	}
	m_visibleCount = UI_PAGE_SIZE;
	if (!isApiRequest) {
		const bool useB2B = filter == PLSNoticeFilter::B2BOnly && !m_isOnlyPrismNotice;
		// Opening sends B2B + Prism prefetch; tab switch uses cache-only path. If this side is still in-flight, wait — no second HTTP.
		if (m_repository->centerCacheIsFetching(useB2B)) {
			m_loading = true;
			showListTextLoading();
			if (!preserveCurrentList) {
				clearList();
			}
			const int waitToken = ++m_refreshToken;
			m_repository->whenCenterCacheIdle(
				this, useB2B,
				[this, filter, category, waitToken, useB2B](const QString &error) {
					if (waitToken != m_refreshToken) {
						return;
					}
					if (error == QStringLiteral("__superseded__")) {
						return;
					}
					m_loading = false;
					hideListTextLoading();
					if (!error.isEmpty()) {
						showFailedView();
						ui->scrollArea->verticalScrollBar()->setValue(0);
						return;
					}
					auto source = useB2B ? m_repository->getCenterCacheMapB2B() : m_repository->getCenterCacheMap();
					if (source->isEmpty() && !m_repository->centerCacheLastError(useB2B).isEmpty()) {
						showFailedView();
						ui->scrollArea->verticalScrollBar()->setValue(0);
						return;
					}
					displayListFromCache(filter, category);
					ui->scrollArea->verticalScrollBar()->setValue(0);
				});
			return;
		}
		hideListTextLoading();
		auto source = useB2B ? m_repository->getCenterCacheMapB2B() : m_repository->getCenterCacheMap();
		if (source->isEmpty() && !m_repository->centerCacheLastError(useB2B).isEmpty()) {
			showFailedView();
			ui->scrollArea->verticalScrollBar()->setValue(0);
			m_loading = false;
			return;
		}
		displayListFromCache(filter, category);
		ui->scrollArea->verticalScrollBar()->setValue(0);
		m_loading = false;
		return;
	}

	m_loading = true;
	showListTextLoading();
	ui->scrollArea->verticalScrollBar()->setValue(0);
	if (!preserveCurrentList) {
		clearList();
	}
	const int refreshToken = ++m_refreshToken;
	const bool isB2B = filter == PLSNoticeFilter::B2BOnly && !m_isOnlyPrismNotice;
	m_repository->refreshCenterCacheAsync(
		this, isB2B,
		[this, filter, category, refreshToken](const QString &error) {
			if (refreshToken != m_refreshToken) {
				return;
			}
			m_loading = false;
			hideListTextLoading();
			if (!error.isEmpty()) {
				showFailedView();
				return;
			}
			displayListFromCache(filter, category);
		},
		prefetchPeerCache);
}

void PLSNoticeUpdateCenterDialog::displayListFromCache(PLSNoticeFilter filter, PLSNoticeCategory category)
{
	clearList();
	const auto filteredItems = getFilteredItemsFromCache(filter, category);
	if (filteredItems.isEmpty()) {
		showEmptyView();
		return;
	}
	appendVisibleItems(filteredItems, 0, std::min<int>(m_visibleCount, static_cast<int>(filteredItems.size())));
}

void PLSNoticeUpdateCenterDialog::loadMore(PLSNoticeFilter filter, PLSNoticeCategory category)
{
	const auto filteredItems = getFilteredItemsFromCache(filter, category);
	if (filteredItems.isEmpty() || m_visibleCount >= filteredItems.size())
		return;
	const int startIndex = m_visibleCount;
	m_visibleCount = std::min<int>(m_visibleCount + UI_PAGE_SIZE, static_cast<int>(filteredItems.size()));
	appendVisibleItems(filteredItems, startIndex, m_visibleCount);
}

QList<PLSNoticeUpdateItem> PLSNoticeUpdateCenterDialog::getFilteredItemsFromCache(PLSNoticeFilter filter, PLSNoticeCategory category) const
{
	if (!m_repository) {
		return {};
	}
	const bool useB2B = filter == PLSNoticeFilter::B2BOnly && !m_isOnlyPrismNotice;
	auto source = useB2B ? m_repository->getCenterCacheMapB2B() : m_repository->getCenterCacheMap();
	QList<PLSNoticeUpdateItem> filteredItems;
	for (auto it = source->end(); it != source->begin();) {
		--it;
		if (it.value().category == category) {
			filteredItems.append(it.value());
		}
	}
	return filteredItems;
}

void PLSNoticeUpdateCenterDialog::appendVisibleItems(const QList<PLSNoticeUpdateItem> &items, int startIndex, int endIndex)
{
	auto *layout = static_cast<QVBoxLayout *>(ui->scrollAreaWidgetContents->layout());
	QLayoutItem *stretchItem = nullptr;
	if (layout->count() > 0) {
		QLayoutItem *lastItem = layout->itemAt(layout->count() - 1);
		if (lastItem && lastItem->spacerItem()) {
			stretchItem = layout->takeAt(layout->count() - 1);
		}
	}
	for (int index = startIndex; index < endIndex; ++index) {
		const auto &item = items.at(index);
		auto *row = new PLSNoticeUpdateListItem(item, ui->scrollAreaWidgetContents);
		connect(row, &QPushButton::clicked, this, [this, item]() { onListItemClicked(item); });
		layout->addWidget(row, 0, Qt::AlignTop);
	}
	if (stretchItem) {
		layout->addItem(stretchItem);
	} else {
		layout->addStretch(1);
	}
}

void PLSNoticeUpdateCenterDialog::showEmptyView()
{
	clearList();
	auto *layout = static_cast<QVBoxLayout *>(ui->scrollAreaWidgetContents->layout());
	layout->addStretch(1);
	auto *label = new QLabel(tr("Notice.Center.Empty"), ui->scrollAreaWidgetContents);
	label->setObjectName("centerEmptyLabel");
	label->setAlignment(Qt::AlignCenter);
	layout->addWidget(label, 0, Qt::AlignCenter);
	layout->addStretch(1);
}

void PLSNoticeUpdateCenterDialog::showFailedView()
{
	clearList();
	auto *layout = static_cast<QVBoxLayout *>(ui->scrollAreaWidgetContents->layout());
	layout->addStretch(1);
	auto *container = new QWidget(ui->scrollAreaWidgetContents);
	container->setObjectName("centerFailedWidget");
	auto *vbox = new QVBoxLayout(container);
	vbox->setSpacing(12);
	vbox->setContentsMargins(0, 0, 0, 0);
	auto *label = new QLabel(tr("Notice.Center.Load.Failed"), container);
	label->setObjectName("centerFailedLabel");
	label->setAlignment(Qt::AlignCenter);
	label->setWordWrap(true);
	vbox->addWidget(label, 0, Qt::AlignBottom);
	auto *retryBtn = new QPushButton(tr("Retry"), container);
	retryBtn->setObjectName("centerRetryButton");
	connect(retryBtn, &QPushButton::clicked, this, [this]() { refreshList(m_isOnlyPrismNotice ? PLSNoticeFilter::PrismOnly : m_filter, m_category, true); });
	vbox->addWidget(retryBtn, 0, Qt::AlignHCenter | Qt::AlignTop);
	layout->addWidget(container);
	layout->addStretch(1);
}

void PLSNoticeUpdateCenterDialog::clearList()
{
	QLayout *layout = ui->scrollAreaWidgetContents->layout();
	QLayoutItem *child;
	while ((child = layout->takeAt(0)) != nullptr) {
		if (child->widget()) {
			child->widget()->deleteLater();
		}
		delete child;
	}
}

void PLSNoticeUpdateCenterDialog::onListItemClicked(const PLSNoticeUpdateItem &item)
{
	bool hasB2B = (m_filter == PLSNoticeFilter::B2BOnly);
	PLSNoticePopupDialog dialog({item}, item.provider, this, false);
	dialog.exec();
	if (m_repository)
		m_repository->markItemAsRead(item);
	PLSMainView *mv = PLSMainView::instance();
	if (mv)
		mv->setNoticeTips(m_repository ? m_repository->hasUnreadNotice(hasB2B) : false);
}

void PLSNoticeUpdateCenterDialog::showListTextLoading()
{
	QScrollArea *scrollArea = ui ? ui->scrollArea : nullptr;
	QWidget *viewport = scrollArea ? scrollArea->viewport() : nullptr;
	if (!viewport) {
		return;
	}
	hideListTextLoading();
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	// Three-arg ctor + explicit QWidget*: MSVC can mis-resolve pls_new<PLSTextLoadingView>(..., ui->scrollArea->viewport()) as (QString, Ui::*).
	m_listTextLoading = pls_new<PLSTextLoadingView>(tr("ResolutionGuide.LoadingMessage"), static_cast<QWidget *>(viewport), QString());
	m_listTextLoading->setGeometry(viewport->rect());
	m_listTextLoading->show();
	m_listTextLoading->raise();
	viewport->installEventFilter(this);
	for (auto *child : scrollArea->findChildren<QWidget *>()) {
		QPointer<QWidget> safeChild = child;
		pls_async_call(this, [safeChild] {
			if (safeChild)
				safeChild->repaint();
		});
	}
}

void PLSNoticeUpdateCenterDialog::hideListTextLoading()
{
	if (ui && ui->scrollArea) {
		ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	}
	if (ui && ui->scrollArea && ui->scrollArea->viewport()) {
		ui->scrollArea->viewport()->removeEventFilter(this);
	}
	if (m_listTextLoading) {
		pls_delete(m_listTextLoading);
		m_listTextLoading = nullptr;
	}
}

bool PLSNoticeUpdateCenterDialog::eventFilter(QObject *watcher, QEvent *event)
{
	if (m_listTextLoading && ui && ui->scrollArea && watcher == ui->scrollArea->viewport() && event->type() == QEvent::Resize) {
		m_listTextLoading->setGeometry(ui->scrollArea->viewport()->rect());
	}
	return PLSDialogView::eventFilter(watcher, event);
}
