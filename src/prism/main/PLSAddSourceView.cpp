#include "PLSAddSourceView.h"
#include "ui_PLSAddSourceItem.h"
#include "ui_PLSAddSourceView.h"
#include "pls-common-define.hpp"
#include "window-basic-main.hpp"
#include "PLSDpiHelper.h"
#include "channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "window-basic-main.hpp"
#include "log.h"

#include <qdebug.h>
#include <qabstractbutton.h>
#include <qmovie.h>
#include <qscrollarea.h>

#define NORMALICONPATH ":/images/add-source-view/icon-source-%1-normal.svg"
#define SELECTICONPATH ":/images/add-source-view/icon-source-%1-select.svg"
#define DESCICONPATH ":/images/add-source-view/ic-addsource-%1.%2"
#define APNGPATHWITHLANG ":/images/add-source-view/%1_%2.%3"
#define APNGPATH ":/images/add-source-view/%1.%2"

#define SOURCEDESCTITLE "sourceMenu.%1.item.title"
#define SOURCEDESCCONTENT "sourceMenu.%1.item.desc"
#define WINDOWMONITOR 5
#define REGIONMONITOR 6

PLSAddSourceItem::PLSAddSourceItem(const QString &id, const QString &displayName, PLSDpiHelper dpiHelper, bool isThirdPlugin, QWidget *parent)
	: QPushButton(parent), ui(new Ui::PLSAddSourceItem), m_id(id), m_text(displayName)
{
	ui->setupUi(this);
	ui->label_text->setText(displayName);
	extern QString GetIconKey(obs_icon_type type);
	extern bool isNewSource(const QString &id);
	if (id == "scene") {
		m_iconKey = "scene";
	} else if (id == "group") {
		m_iconKey = "group";

	} else {
		m_iconKey = GetIconKey(obs_source_get_icon_type(id.toUtf8().constData())).toLower();
	}
	if (isThirdPlugin) {
		m_iconKey = "plugin";
	}
	ui->label_status->setVisible(isNewSource(id));
	connect(this, &QPushButton::toggled, this, &PLSAddSourceItem::statusChanged);

	dpiHelper.notifyDpiChanged(this, [=](double dpi) {
		QPixmap pix;
		loadPixmap(pix, QString(NORMALICONPATH).arg(m_iconKey), PLSDpiHelper::calculate(dpi, QSize{30, 30}));
		ui->label_icon->setPixmap(pix);
	});
}

PLSAddSourceItem::~PLSAddSourceItem()
{
	delete ui;
}

void PLSAddSourceItem::updateItemStatus(bool /*isSelected*/) {}

QString PLSAddSourceItem::itemId()
{
	return m_id;
}

QString PLSAddSourceItem::itemIconKey()
{
	return m_iconKey;
}

QString PLSAddSourceItem::itemDisplayName()
{
	return m_text;
}

void PLSAddSourceItem::setScrollArea(QScrollArea *scrollArea)
{
	m_scrollArea = scrollArea;
}

QScrollArea *PLSAddSourceItem::getScrollArea()
{
	return m_scrollArea;
}

void PLSAddSourceItem::statusChanged(bool isChecked)
{
	QString iconPath;
	if (isChecked) {
		iconPath = QString(SELECTICONPATH).arg(m_iconKey);
	} else {
		iconPath = QString(NORMALICONPATH).arg(m_iconKey);
	}
	QPixmap pix;
	loadPixmap(pix, iconPath, PLSDpiHelper::calculate(this, QSize{30, 30}));
	ui->label_icon->setPixmap(pix);
	ui->label_text->setProperty("select", isChecked);
	pls_flush_style(ui->label_text);
}

PLSAddSourceView::PLSAddSourceView(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSAddSourceView), m_langShortList({"en", "ko"})
{
	ui->setupUi(this->content());
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
	setWindowState(Qt::WindowActive);
	setWindowTitle(Str("sourceMenu.view.title"));
	setResizeEnabled(false);

	m_movie.setCacheMode(QMovie::CacheAll);

	dpiHelper.setCss(this, {PLSCssIndex::PLSAddSourceView});
	connect(&m_buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &PLSAddSourceView::sourceItemChanged);
	connect(ui->buttonOK, &QPushButton::clicked, this, &PLSAddSourceView::okHandler);
	connect(ui->buttonCancel, &QPushButton::clicked, [=]() {
		done(Rejected);
		PLS_UI_STEP("AddSourceView", "add source view click cancel button.", ACTION_CLICK);
	});
	ui->buttonOK->setEnabled(m_buttonGroup.checkedButton() != nullptr);
	initSourceItems(dpiHelper);
	dpiHelper.notifyDpiChanged(this, [=](double dpi) {
		ui->scrollArea->verticalScrollBar()->isVisible() ? ui->verticalLayout_baseSource->setContentsMargins(11 * dpi, 0, 4 * dpi, 0)
								 : ui->verticalLayout_baseSource->setContentsMargins(11 * dpi, 0, 13 * dpi, 0);
		ui->scrollArea_2->verticalScrollBar()->isVisible() ? ui->verticalLayout_prismSource->setContentsMargins(11 * dpi, 0, 4 * dpi, 0)
								   : ui->verticalLayout_prismSource->setContentsMargins(11 * dpi, 0, 13 * dpi, 0);
		setSourceDesc(m_buttonGroup.checkedButton());
	});
	setLangShort();
}

PLSAddSourceView::~PLSAddSourceView()
{
	delete ui;
}

QString PLSAddSourceView::selectSourceId()
{
	auto checkButton = static_cast<PLSAddSourceItem *>(m_buttonGroup.checkedButton());
	if (checkButton) {
		return checkButton->itemId();
	}
	return QString();
}

void PLSAddSourceView::initSourceItems(const PLSDpiHelper &dpiHelper)
{
	std::vector<std::vector<QString>> presetTypeList;
	std::vector<QString> otherTypeList;
	std::map<QString, bool> monitorPlugins;
	monitorPlugins[PRISM_MONITOR_SOURCE_ID] = false;
	monitorPlugins[PRISM_REGION_SOURCE_ID] = false;
	PLSBasic::GetSourceTypeList(presetTypeList, otherTypeList, monitorPlugins);

	for (auto iter = presetTypeList.begin(); iter != presetTypeList.end(); ++iter) {
		std::vector<QString> &subList = *iter;
		for (unsigned i = 0; i < subList.size(); ++i) {
			QString id = subList[i];
			QByteArray cid = id.toUtf8();
			QString displayName;
			if (strcmp(cid.constData(), SCENE_SOURCE_ID) == 0) {
				displayName = Str("Basic.Scene");
			} else {
				displayName = obs_source_get_display_name(cid.constData());
			}
			m_buttonGroup.addButton(new PLSAddSourceItem(id, displayName, dpiHelper));
		}
	}
	//add other source type
	if (!otherTypeList.empty()) {
		for (int i = 0; i < otherTypeList.size(); i++) {
			QString id = otherTypeList[i];
			QByteArray cid = id.toUtf8();
			m_buttonGroup.addButton(new PLSAddSourceItem(id, QString::fromUtf8(obs_source_get_display_name(cid.constData())), dpiHelper, true));
		}
	}
	m_buttonGroup.addButton(new PLSAddSourceItem(GROUP_SOURCE_ID, QTStr("Group"), dpiHelper));
	//add monitor source type
	int monitorIndex = 4;
	for (const auto &item : monitorPlugins) {
		if (item.second == true) {
			m_buttonGroup.addButton(new PLSAddSourceItem(item.first, QString::fromUtf8(obs_source_get_display_name(item.first.toUtf8().constData())), dpiHelper), ++monitorIndex);
		}
	}

	for (auto button : m_buttonGroup.buttons()) {
		auto item = static_cast<PLSAddSourceItem *>(button);
		if (item) {
			if (item->itemId().contains("prism") && !(item->itemId() == PRISM_MONITOR_SOURCE_ID || item->itemId() == PRISM_REGION_SOURCE_ID)) {
				ui->verticalLayout_prismSource->addWidget(item);
				item->setScrollArea(ui->scrollArea_2);
			} else {
				ui->verticalLayout_baseSource->addWidget(item);
				item->setScrollArea(ui->scrollArea);
			}
		}
	}
	ui->verticalLayout_baseSource->insertWidget(WINDOWMONITOR, m_buttonGroup.button(REGIONMONITOR));
	ui->verticalLayout_baseSource->insertWidget(WINDOWMONITOR, m_buttonGroup.button(WINDOWMONITOR));

	ui->verticalLayout_prismSource->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
	ui->verticalLayout_baseSource->addSpacerItem(new QSpacerItem(0, 15, QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
}
void PLSAddSourceView::sourceItemChanged(QAbstractButton *button)
{
	//change desc content
	ui->buttonOK->setEnabled(true);
	auto item = static_cast<PLSAddSourceItem *>(button);
	item->getScrollArea()->ensureWidgetVisible(button);
	PLS_UI_STEP("AddSourceView", item->itemDisplayName().toUtf8().constData(), ACTION_CLICK);
	setSourceDesc(button);
}
void PLSAddSourceView::okHandler()
{
	PLS_UI_STEP("AddSourceView", "add source view click ok button.", ACTION_CLICK);

	done(Accepted);
}

void PLSAddSourceView::setSourceDesc(QAbstractButton *button)
{
	extern int getSourceDisplayType(const QString &id);
	auto item = static_cast<PLSAddSourceItem *>(button);
	m_movie.stop();
	int descType = 0;
	QString key = "default";
	QString title = tr(QString(SOURCEDESCTITLE).arg(key).toUtf8().constData());
	if (item) {
		descType = getSourceDisplayType(item->itemId());
		key = item->itemIconKey();
		title = item->itemDisplayName();
	}
	QString translatorKey = key;
	if (key == "monitor") {
		if (item->itemId() == PRISM_MONITOR_SOURCE_ID) {
			translatorKey = QString("%1Full").arg(key);
		} else if (item->itemId() == PRISM_REGION_SOURCE_ID) {
			translatorKey = QString("%1Part").arg(key);
		}
		title = titleLineFeed(title);
	}
	ui->label_descTitle->setText(title);

	ui->label_descContent->setText(tr((QString(SOURCEDESCCONTENT).arg(translatorKey)).toUtf8().constData()));

	switch (descType) {
	case 0: {
		ui->label_descGif->setVisible(false);
		ui->label_descPic->setVisible(true);
		QSize size = {100, 100};
		QString suffix = "svg";
		QPixmap pix;
		if (0 == key.compare("bgm", Qt::CaseInsensitive)) {
			size = {250, 71};
			suffix = "png";
			pix.load(QString(DESCICONPATH).arg(key).arg(suffix));
			pix = pix.scaled(PLSDpiHelper::calculate(this, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		} else if (0 == key.compare("default", Qt::CaseInsensitive)) {
			suffix = "png";
			pix.load(QString(DESCICONPATH).arg(key).arg(suffix));
			pix = pix.scaled(PLSDpiHelper::calculate(this, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		} else {
			loadPixmap(pix, QString(DESCICONPATH).arg(key).arg(suffix), PLSDpiHelper::calculate(this, size));
		}
		ui->label_descPic->setPixmap(pix);

	} break;
	case 1: {
		QString suffix = "png";
		QString format = "APNG";
		QSize size = {280, 140};
		if (key == "giphy") {
			suffix = "gif";
			format = "GIF";
			size = {145, 140};
		}
		ui->label_descGif->setVisible(true);
		ui->label_descPic->setVisible(false);
		ui->label_descGif->setMovie(&m_movie);
		m_movie.setScaledSize(PLSDpiHelper::calculate(this, size));
		QString fileName = QString((APNGPATHWITHLANG)).arg(key).arg(m_langShort).arg(suffix);
		QFileInfo fileInfo(fileName);
		if (fileInfo.exists()) {
			m_movie.setFileName(fileName);
		} else {
			m_movie.setFileName(QString((APNGPATH)).arg(key).arg(suffix));
		}
		m_movie.setFormat(format.toUtf8());
		m_movie.start();
	} break;
	default:
		break;
	}
}

void PLSAddSourceView::setLangShort()
{
	auto langShortStr = pls_get_current_language_short_str();
	for (auto lang : m_langShortList) {
		if (langShortStr == lang) {
			m_langShort = lang;
			break;
		}
	}
}

QString PLSAddSourceView::titleLineFeed(const QString &title)
{
	QString _title;
	if (!title.contains('('))
		return title;
	auto titles = title.split('(');
	_title.append(titles.first());
	_title.append("\r\n(");
	_title.append(titles.last());
	return _title;
}
