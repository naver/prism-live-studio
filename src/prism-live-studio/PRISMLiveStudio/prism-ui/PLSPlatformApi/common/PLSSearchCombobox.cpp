#include "PLSSearchCombobox.h"
#include <QListWidget>
#include "libutils-api.h"
#include <QHBoxLayout>
#include <QLabel>
#include "PLSAPICommon.h"
#include <QPushButton>
#include <QApplication>
#include "libui.h"

PLSSearchCombobox::PLSSearchCombobox(QWidget *parent) : QLineEdit(parent)
{
	pls_add_css(this, {"PLSSearchCombobox"});
	pls_flush_style(this, "showPlaceholder", text().isEmpty());
	setMaxLength(100);

	m_timer = new QTimer(this);
	m_timer->setInterval(300);
	m_timer->setSingleShot(true);

	installEventFilter(this);

	pushButtonClear = pls_new<QPushButton>(this);
	pushButtonClear->setObjectName("pushButtonClear");
	pushButtonClear->setCursor(Qt::ArrowCursor);
	pushButtonClear->setFocusPolicy(Qt::NoFocus);

	pushButtonSearch = pls_new<QPushButton>(this);
	pushButtonSearch->setObjectName("pushButtonSearch");
	pushButtonSearch->setFocusPolicy(Qt::NoFocus);

	auto layoutCategory = pls_new<QHBoxLayout>(this);
	layoutCategory->setSpacing(9);
	layoutCategory->setContentsMargins(0, 0, 9, 0);
	layoutCategory->addStretch();

	layoutCategory->addWidget(pushButtonClear);
	layoutCategory->addWidget(pushButtonSearch);
	pushButtonClear->hide();

	connect(m_timer, &QTimer::timeout, this, [this]() {
		auto text = this->text();
		if (!text.isEmpty()) {
			emit startSearchText(this, text);
		}
	});
	connect(this, &QLineEdit::textChanged, this, &PLSSearchCombobox::onTextChanged, Qt::QueuedConnection);

	connect(pushButtonClear, &QPushButton::clicked, this, [this]() { this->setText(QString()); });
	connect(pushButtonSearch, &QPushButton::clicked, this, [this]() {
		if (!this->text().isEmpty()) {
			startSearch(false);
		}
	});

	connect(qApp, &QApplication::focusChanged, this, [this](const QWidget *, const QWidget *now) {
		if (!m_listWidget) {
			return;
		}
		if (this == now) {
			if (!this->text().isEmpty()) {
				pushButtonClear->show();
			}
			if (m_listWidget->currentDataSize() > 0) {
				showListWidget(true);
			} else if (m_isFirstSearch) {
				receiveSearchData({m_selectData}, text());
				m_isFirstSearch = false;
				startSearch(true);
			} else if (!this->text().isEmpty()) {
				startSearch(false);
			}
		} else if (nullptr == now || (m_listWidget != now && m_listWidget != now->parent())) {
			pushButtonClear->hide();
			showListWidget(false);
			this->clearFocus();
		}
	});
}
void PLSSearchCombobox::setupListUI()
{
	if (m_listWidget) {
		return;
	}
	topLevelWidget()->setFocusPolicy(Qt::StrongFocus);
	m_listWidget = pls_new<PLSLiveInfoSearchListWidget>(topLevelWidget());
	m_listWidget->setObjectName("searchList");
	m_listWidget->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
	m_listWidget->setAttribute(Qt::WA_ShowWithoutActivating, true);
	showListWidget(false);

	connect(m_listWidget, &QListWidget::itemClicked, this, [this](const QListWidgetItem *item) {
		PLSSearchData data = item->data(Qt::UserRole).value<PLSSearchData>();
		showListWidget(false, true);
		if (data.id.isEmpty()) {
			m_selectData.resetData();
			return;
		}
		setSelectData(data);
		emit itemSelect(data);
	});
}
void PLSSearchCombobox::showEvent(QShowEvent *event)
{
	QLineEdit::showEvent(event);

	if (!m_listWidget) {
		setupListUI();
	}
}
void PLSSearchCombobox::onTextChanged(const QString &text)
{
	m_selectData.resetData();
	pls_flush_style(this, "showPlaceholder", text.isEmpty());
	if (!m_listWidget) {
		return;
	}

	if (text.isEmpty()) {
		pushButtonClear->hide();

		showListWidget(false, true);
		m_timer->stop();

	} else {
		if (this->hasFocus() && pushButtonClear->isHidden()) {
			pushButtonClear->show();
		}
		startSearch(false);
	}
}

void PLSSearchCombobox::showListWidget(bool show, bool isClear)
{
	if (!m_listWidget) {
		return;
	}
	pls_flush_style(pushButtonSearch, "searchOn", show ? true : false);
	if (!show) {
		if (isClear) {
			m_listWidget->clear();
		}
		m_listWidget->hide();

		return;
	}
	QPoint buttonPos = this->mapToGlobal(QPoint(0, height()));
	QPoint buttonPosInParent = m_listWidget->parentWidget()->mapFromGlobal(buttonPos);

	m_listWidget->move(buttonPosInParent);

	int itemHeight = m_listWidget->currentDataSize() == 0 ? 40 : 60;
	m_listWidget->resize(width(), itemHeight * qMin(5, m_listWidget->count()) + 3);
	m_listWidget->show();
}

void PLSSearchCombobox::receiveSearchData(const std::vector<PLSSearchData> &datas, const QString &keyword, const QString &emptyShowMsg)
{

	if (this->text() != keyword) {
		return;
	}
	if (this->text().isEmpty()) {
		return;
	}
	if (!m_listWidget) {
		setupListUI();
	}
	m_listWidget->showList(datas, emptyShowMsg);

	if (QApplication::focusWidget() == this) {
		showListWidget(true);
	}
}
void PLSSearchCombobox::setSelectData(const PLSSearchData &data)
{
	m_selectData = data;

	blockSignals(true);
	setText(data.name);
	pls_flush_style(this, "showPlaceholder", text().isEmpty());
	blockSignals(false);
}
PLSSearchData PLSSearchCombobox::getSelectData()
{
	return m_selectData;
}

void PLSSearchCombobox::startSearch(bool immediately)
{
	if (immediately) {
		auto text = this->text();
		if (!text.isEmpty()) {
			emit startSearchText(this, text);
		}
	} else {
		m_timer->start();
	}
}

PLSLiveInfoSearchListWidget::PLSLiveInfoSearchListWidget(QWidget *parent) : QListWidget(parent)
{
	pls_add_css(this, {"PLSSearchCombobox"});
}

void PLSLiveInfoSearchListWidget::showList(const std::vector<PLSSearchData> &datas, const QString &emptyShowMsg)
{

	m_datas = datas;
	this->clear();

	pls_flush_style_recursive(this, "isEmpty", datas.empty() ? true : false, 1);

	if (datas.empty()) {
		auto msg = emptyShowMsg.isEmpty() ? tr("facebook.liveinfo.game.empty.list") : emptyShowMsg;
		auto item = pls_new<QListWidgetItem>(msg);
		this->addItem(item);
		item->setData(Qt::UserRole, QVariant::fromValue<PLSSearchData>(PLSSearchData()));
		return;
	}

	for (auto data : datas) {
		auto item = pls_new<QListWidgetItem>(data.name);

		auto rootWidget = pls_new<QWidget>();
		rootWidget->setObjectName("rootWidget");
		auto layout = pls_new<QHBoxLayout>(rootWidget);

		layout->setContentsMargins({0, 0, 0, 0});

		auto iconLabel = pls_new<QLabel>();
		iconLabel->setObjectName("categoryIconLabel");
		iconLabel->setAttribute(Qt::WA_Hover, false);
		auto sizetmp = this->iconSize();
		iconLabel->setScaledContents(true);
		static QPixmap tmp(":/resource/images/noimg_chzzk_defaulf.svg");
		iconLabel->setPixmap(tmp);
		layout->addWidget(iconLabel, 0);

		auto textLbael = pls_new<QLabel>(data.name);
		textLbael->setObjectName("categoryLabel");
		textLbael->setAttribute(Qt::WA_Hover, false);
		layout->addWidget(textLbael, 1);

		this->addItem(item);
		this->setItemWidget(item, rootWidget);
		item->setData(Qt::UserRole, QVariant::fromValue<PLSSearchData>(data));

		auto url = data.url;
		if (url.isEmpty())
			continue;

		QString localPath = PLSAPICommon::getMd5ImagePath(url);

		auto _callBack = [icon = QPointer<QLabel>(iconLabel), url](bool ok, const QString &imagePath) {
			if (!ok) {
				return;
			}
			if (icon) {
				QPixmap pixmap;
				if (pixmap.load(imagePath)) {
					icon->setPixmap(pixmap);
				}
			}
		};
		if (!localPath.isEmpty() && QFile(localPath).exists()) {
			_callBack(true, localPath);
			continue;
		}
		PLSAPICommon::downloadImageAsync(iconLabel, url, _callBack, false, localPath);
	}
}
int PLSLiveInfoSearchListWidget::currentDataSize() const
{
	return (int)m_datas.size();
}
