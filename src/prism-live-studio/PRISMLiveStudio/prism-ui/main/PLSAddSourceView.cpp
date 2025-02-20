#include "PLSAddSourceView.h"
#include "ui_PLSAddSourceItem.h"
#include "ui_PLSAddSourceView.h"
#include "pls-common-define.hpp"
#include "window-basic-main.hpp"
#include "liblog.h"
#include "PLSBasic.h"

#include <qdebug.h>
#include <qabstractbutton.h>
#include <qmovie.h>
#include <qscrollarea.h>
#include <qscrollbar.h>
#include <pls-shared-functions.h>
#include <QFontMetrics>
#include <QDesktopServices>
#include "PLSPlatformApi.h"

#define DESCICONPATH QStringLiteral(":/resource/images/add-source-view/ic-addsource-%1.%2")
#define APNGPATHWITHLANG QStringLiteral(":/resource/images/add-source-view/%1_%2.%3")
#define APNGPATH QStringLiteral(":/resource/images/add-source-view/%1.%2")

#define SOURCEDESCTITLE QStringLiteral("sourceMenu.%1.item.title")
#define SOURCEDESCCONTENT QStringLiteral("sourceMenu.%1.item.desc")
constexpr auto WINDOWMONITOR = 5;
constexpr auto REGIONMONITOR = 6;
constexpr auto MARGIN = 12;
constexpr auto PADDING = 7;
constexpr auto ICONWIDTH = 30;
constexpr auto NEWICONWIDTH = 34;
constexpr auto ITEMMAXHEIGHT = 58;
constexpr auto ITEMMINHEIGHT = 40;
constexpr auto OTHERHEIGHT = 20;
constexpr auto ITEMTEXTLAYOUTRIGHTMARGIN = 5;

using namespace common;
extern QString GetIconKey(obs_icon_type type);
extern bool isNewSource(const QString &id);
extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);

PLSAddSourceItem::PLSAddSourceItem(const QString &id, const QString &displayName, bool isThirdPlugin, QWidget *parent) : QPushButton(parent), m_id(id), m_text(displayName)
{
	bool isNew = isNewSource(id);
	setProperty("sourceId", id);
	setProperty("isNew", isNew);
	setProperty("lang", pls_get_current_language());

	ui = pls_new<Ui::PLSAddSourceItem>();
	ui->setupUi(this);
	ui->label_text->setText(displayName);

	if (id == "scene") {
		m_iconKey = "scene";
	} else if (id == "group") {
		m_iconKey = "group";

	} else if (id == DECKLINK_INPUT_SOURCE_ID) {
		m_iconKey = "decklinkinput";
	} else {
		m_iconKey = GetIconKey(obs_source_get_icon_type(id.toUtf8().constData())).toLower();
	}
	m_newLabel = pls_new<QLabel>(this);
	m_newLabel->setVisible(false);
	m_newLabel->setObjectName("label_status");
	m_newLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	connect(this, &QPushButton::toggled, this, &PLSAddSourceItem::statusChanged);
	connect(this, &QPushButton::toggled, this, [this, isNew](bool isChecked) { calculateLabelWidth(isNew, isChecked); });
	QPixmap pix = OBSBasic::Get()->GetSourcePixmap(id, false);
	ui->label_icon->setPixmap(pix);
	ui->label_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
	ui->label_text->setAttribute(Qt::WA_TransparentForMouseEvents);
	QMetaObject::invokeMethod(
		this, [isNew, this]() { calculateLabelWidth(isNew, false); }, Qt::QueuedConnection);
}

PLSAddSourceItem::~PLSAddSourceItem()
{
	pls_delete(ui);
}

QString PLSAddSourceItem::itemId() const
{
	return m_id;
}

QString PLSAddSourceItem::itemIconKey() const
{
	return m_iconKey;
}

QString PLSAddSourceItem::itemDisplayName() const
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

void PLSAddSourceItem::calculateLabelWidth(bool isNew, bool isChecked)
{
	ui->horizontalLayout_2->setContentsMargins(0, 0, isNew ? m_newLabel->width() + ITEMTEXTLAYOUTRIGHTMARGIN : ITEMTEXTLAYOUTRIGHTMARGIN, 0);

	QFontMetrics fontWidth(ui->label_text->font());
	auto availableWidth = this->frameSize().width() - PADDING - ICONWIDTH - MARGIN;
	if (isNew) {
		availableWidth -= NEWICONWIDTH;
	}

	availableWidth -= isChecked ? 5 : 0;

	if (fontWidth.horizontalAdvance(ui->label_text->text()) > availableWidth) {
		ui->label_text->setWordWrap(true);
		this->setFixedHeight(ITEMMAXHEIGHT);

	} else {
		ui->label_text->setWordWrap(false);
		this->setFixedHeight(ITEMMINHEIGHT);
	}
	auto textRec = fontWidth.boundingRect(QRect(0, 0, availableWidth, ITEMMAXHEIGHT), Qt::TextWordWrap, ui->label_text->text());
	m_newLabel->move(textRec.width() + ICONWIDTH + PADDING + MARGIN, (this->height() - m_newLabel->height()) / 2);
	m_newLabel->setVisible(isNew);
}

void PLSAddSourceView::openSourceLink()
{
	QDesktopServices::openUrl(QUrl(m_openLink));
}

void PLSAddSourceItem::statusChanged(bool isChecked)
{
	QPixmap pix = OBSBasic::Get()->GetSourcePixmap(m_id, isChecked);
	ui->label_icon->setPixmap(pix);
	ui->label_text->setProperty("select", isChecked);
	pls_flush_style(ui->label_text);
}

PLSAddSourceView::PLSAddSourceView(QWidget *parent) : PLSDialogView(parent)
{
	pls_set_css(this, {"PLSAddSourceView"});
	ui = pls_new<Ui::PLSAddSourceView>();
	setupUi(ui);
	setWindowTitle(Str("sourceMenu.view.title"));
	setResizeEnabled(false);
#if defined(Q_OS_MACOS)
	initSize(980, 670);
#elif defined(Q_OS_WIN)
	initSize(980, 710);
#endif
	m_movie.setCacheMode(QMovie::CacheAll);

	connect(&m_buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &PLSAddSourceView::sourceItemChanged);
	connect(ui->buttonOK, &QPushButton::clicked, this, &PLSAddSourceView::okHandler);
	connect(ui->buttonCancel, &QPushButton::clicked, [this]() { PLSAddSourceView::done(Rejected); });
	connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, [this](int value) {
		if (value >= ui->scrollArea->verticalScrollBar()->maximum()) {
			ui->frame_baseSource->repaint();
		}
	});
	ui->buttonOK->setEnabled(m_buttonGroup.checkedButton() != nullptr);
	initSourceItems();
	ui->scrollArea->verticalScrollBar()->isVisible() ? ui->verticalLayout_baseSource->setContentsMargins(11, 0, 4, 0) : ui->verticalLayout_baseSource->setContentsMargins(11, 0, 13, 0);
	ui->scrollArea_2->verticalScrollBar()->isVisible() ? ui->verticalLayout_prismSource->setContentsMargins(11, 0, 4, 0) : ui->verticalLayout_prismSource->setContentsMargins(11, 0, 13, 0);
	setSourceDesc(m_buttonGroup.checkedButton());
	setLangShort();

#if defined(Q_OS_MACOS)
	ui->horizontalLayout_2->insertWidget(1, ui->buttonCancel);
#endif
	ui->verticalLayout_4->setAlignment(ui->label_descContent, Qt::AlignTop);

	connect(ui->buttonTip, &QPushButton::clicked, this, &PLSAddSourceView::openSourceLink);

	m_openLink = pls_is_equal(pls_prism_get_locale().toUtf8().constData(), "ko-KR")
			     ? "https://blog.naver.com/prismlivestudio/223402286811"
			     : "https://medium.com/prismlivestudio/windows-guide-guide-for-using-spout2-capture-source-in-prism-live-studio-54b0a72241eb";

	ui->buttonTip->setVisible(false);
}

PLSAddSourceView::~PLSAddSourceView()
{
	pls_delete(ui);
}

QString PLSAddSourceView::selectSourceId() const
{
	auto checkButton = static_cast<PLSAddSourceItem *>(m_buttonGroup.checkedButton());
	if (checkButton) {
		return checkButton->itemId();
	}
	return QString();
}

void PLSAddSourceView::initSourceItems()
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
				displayName = pls_source_get_display_name(cid.constData());
			}
			if (pls_is_equal(cid.constData(), PRISM_CHZZK_SPONSOR_SOURCE_ID)) {
				auto list = PLS_PLATFORM_API->getExistedPlatformsByType(PLSServiceType::ST_CHZZK);
				if (list.empty()) {
					continue;
				}
			}
			m_buttonGroup.addButton(new PLSAddSourceItem(id, displayName));
		}
	}
	//add other source type
	if (!otherTypeList.empty()) {
		for (int i = 0; i < otherTypeList.size(); i++) {
			QString id = otherTypeList[i];
			QByteArray cid = id.toUtf8();
			m_buttonGroup.addButton(new PLSAddSourceItem(id, QString::fromUtf8(obs_source_get_display_name(cid.constData())), true));
		}
	}
	m_buttonGroup.addButton(new PLSAddSourceItem(GROUP_SOURCE_ID, QTStr("Group")));
	//add monitor source type
	int monitorIndex = 4;
	for (const auto &item : monitorPlugins) {
		if (item.second == true) {
			++monitorIndex;
			m_buttonGroup.addButton(new PLSAddSourceItem(item.first, QString::fromUtf8(obs_source_get_display_name(item.first.toUtf8().constData()))), monitorIndex);
		}
	}

	for (auto button : m_buttonGroup.buttons()) {
		auto item = static_cast<PLSAddSourceItem *>(button);
		if (item->itemId() == PRISM_CHAT_SOURCE_ID)
			continue;
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
	ui->buttonTip->setVisible(item->itemIconKey() == "spout2");
	setSourceDesc(button);
}
void PLSAddSourceView::okHandler()
{
	if (selectSourceId() == PRISM_CHZZK_SPONSOR_SOURCE_ID && !PLSBasic::instance()->bSuccessGetChzzkSourceUrl(this)) {
		return;
	}
	done(Accepted);
}
extern int getSourceDisplayType(const QString &id);
void PLSAddSourceView::setSourceDesc(QAbstractButton *button)
{
	auto item = static_cast<PLSAddSourceItem *>(button);
	m_movie.stop();
	int descType = 0;
	QString key = "default";
	QString title = tr(QString(SOURCEDESCTITLE).arg(key).toUtf8().constData());
	QString translatorKey = key;
	if (item) {
		descType = getSourceDisplayType(item->itemId());
		key = item->itemIconKey();
		title = item->itemDisplayName();
		translatorKey = key;

		if (key == "monitor") {
			if (item->itemId() == PRISM_MONITOR_SOURCE_ID) {
				translatorKey = QString("%1Full").arg(key);
			} else if (item->itemId() == PRISM_REGION_SOURCE_ID) {
				translatorKey = QString("%1Part").arg(key);
			}
		}
	}

	ui->label_descTitle->setText(title);
	ui->label_descContent->setText(tr((QString(SOURCEDESCCONTENT).arg(translatorKey)).toUtf8().constData()));

	calculateDescHeight(ui->label_descTitle);
	calculateDescHeight(ui->label_descContent);
	auto margin = ui->verticalLayout_4->contentsMargins();

	if (descType == 0) {
		ui->label_descGif->setVisible(false);
		ui->label_descPic->setVisible(true);
		QSize size = {100, 100};
		QString suffix = "svg";
		ui->label_descPic->setFixedSize(size);
		margin.setBottom(170);
		if (0 == key.compare("bgm", Qt::CaseInsensitive)) {
			margin.setBottom(184);
			size = {250, 71};
			suffix = "png";
			ui->label_descPic->setFixedSize(size);
		} else if (0 == key.compare("default", Qt::CaseInsensitive)) {
			suffix = "png";
		}
		QString imagePath = QString(DESCICONPATH).arg(key).arg(suffix);
		pls_async_invoke([pthis = pls::QObjectPtr<PLSAddSourceView>(this), imagePath, size, suffix]() {
			if (!pthis.valid())
				return;

			QPixmap image;
			if (suffix == "svg") {
				loadPixmap(image, imagePath, size * 4);
			} else {
				image.load(imagePath);
				image = image.scaled(size * 3, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			}

			pls_async_call(pthis, [pthis, image]() { //
				pthis->ui->label_descPic->setPixmap(image);
			});
		});
	} else if (descType == 1) {
		QString suffix = "png";
		QString format = "APNG";
		QSize size = {280, 140};
		ui->label_descGif->setFixedSize(size);
		margin.setBottom(150);
		if (key == "giphy") {
			suffix = "gif";
			format = "GIF";
			size = {140, 126};
			ui->label_descGif->setFixedSize(size);
			margin.setBottom(157);
		} else if (key == "viewercount") {
			size = {152, 220};
			ui->label_descGif->setFixedSize(size);
			margin.setBottom(102);
		} else if (key == "chzzksponsor") {
			suffix = "gif";
			format = "GIF";
			size = {224, 126};
			ui->label_descGif->setFixedSize(size);
		}
		ui->label_descGif->setVisible(true);
		ui->label_descPic->setVisible(false);
		ui->label_descGif->setMovie(&m_movie);
		QString fileName = QString((APNGPATHWITHLANG)).arg(key).arg(m_langShort).arg(suffix);

		if (QFileInfo fileInfo(fileName); fileInfo.exists()) {
			m_movie.setFileName(fileName);
		} else {
			m_movie.setFileName(QString((APNGPATH)).arg(key).arg(suffix));
		}
		m_movie.setFormat(format.toUtf8());
		m_movie.start();
	}
	ui->verticalLayout_4->setContentsMargins(margin);
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

void PLSAddSourceView::calculateDescHeight(QLabel *label)
{
	QMetaObject::invokeMethod(
		this,
		[label]() {
			QFontMetrics fontWidth(label->font());
			auto availableWidth = 260;
			auto textRec = fontWidth.boundingRect(QRect(0, 0, availableWidth, 0), Qt::TextWordWrap, label->text());
			label->setFixedSize(availableWidth, textRec.height());
			label->adjustSize();
		},
		Qt::QueuedConnection);
}
