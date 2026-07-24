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
#include "PLSAnimationLabel.h"
#include <QGroupBox>
#include <QTextLayout>
#include <QTextOption>
#include <QFile>
#include <QEvent>
#include <algorithm>
#include <flowlayout.h>
#include <libutils-api.h>
#include "pls-performance.h"

#define DESCICONPATH QStringLiteral(":/resource/images/add-source-view/ic-addsource-%1.%2")
#define APNGPATHWITHLANG QStringLiteral(":/resource/images/add-source-view/%1_%2.%3")
#define APNGPATH QStringLiteral(":/resource/images/add-source-view/%1.%2")

#define SOURCEDESCTITLE QStringLiteral("sourceMenu.%1.item.title")
#define SOURCEDESCCONTENT QStringLiteral("sourceMenu.%1.item.desc")
#define THIRDPLUGINKEY QStringLiteral("plugin")

constexpr auto ICONWIDTH = 36;
constexpr auto NEWICONWIDTH = 34;
constexpr auto BETAICONWIDTH = 30;
constexpr auto ITEMMAXHEIGHT = 88;
constexpr auto ITEMMIDHEIGHT = 72;
constexpr auto ITEMMINHEIGHT = 56;
constexpr auto ITEMTEXTLAYOUTRIGHTMARGIN = 5;
constexpr auto DEFAULT_CREATE_GROUP_COUNT = 3;
constexpr auto TEXT_TO_BADGE_GAP = 5;
constexpr auto BADGE_BETWEEN_GAP = 5;
using namespace common;
extern QString GetIconKey(obs_icon_type type);
extern bool isNewSource(const QString &id);
extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);
extern bool isBetaSource(const QString &id);
namespace {
struct TextLineEnd {
	qreal lineWidth = 0.0;
	qreal lineTop = 0.0;
	qreal lineHeight = 0.0;
	int lineCount = 0;
	qreal totalHeight = 0.0;
};

static TextLineEnd calculateTextLineEnd(const QString &text, const QFont &font, int maxWidth)
{
	TextLineEnd info;
	if (text.isEmpty() || maxWidth <= 0) {
		QFontMetrics metrics(font);
		info.lineHeight = metrics.height();
		return info;
	}

	QTextLayout layout(text, font);
	QTextOption option;
	option.setWrapMode(QTextOption::WordWrap);
	layout.setTextOption(option);
	layout.beginLayout();
	qreal nextLineY = 0.0;
	int lineIndex = 0;
	while (true) {
		QTextLine line = layout.createLine();
		if (!line.isValid())
			break;
		line.setLineWidth(maxWidth);
		line.setPosition(QPointF(0, nextLineY));
		nextLineY = line.y() + line.height();

		info.lineWidth = std::min<qreal>(line.naturalTextWidth(), maxWidth);
		info.lineTop = line.position().y();
		info.lineHeight = line.height();
		info.lineCount = ++lineIndex;
	}
	layout.endLayout();
	info.totalHeight = nextLineY;

	if (info.lineCount == 0) {
		QFontMetrics metrics(font);
		info.lineHeight = metrics.height();
		info.totalHeight = info.lineHeight;
	}
	return info;
}

class PLSRecommendationBanner : public QPushButton {
	Q_OBJECT
public:
	explicit PLSRecommendationBanner(QWidget *parent = nullptr) : QPushButton(parent)
	{
		setObjectName("RecommendationBanner");
		setCursor(Qt::PointingHandCursor);
		setCheckable(false);
		setFlat(true);
		setAttribute(Qt::WA_Hover, true);
		setProperty("hovered", false);
		setProperty("pressed", false);
		auto *mainLayout = new QHBoxLayout(this);
		mainLayout->setSpacing(7);
		mainLayout->setContentsMargins(7, 9, 8, 9);

		// Avatar
		QLabel *avatar = new QLabel(this);
		avatar->setScaledContents(true);
		avatar->setObjectName("Avatar");
		mainLayout->addWidget(avatar);
		avatar->setPixmap(pls_load_pixmap_with_mode(":/resource/images/add-source-view/img_source_recommendedsources.png", QSize(44, 27) * 3));
		PLSLabel *messageLabel = new PLSLabel(tr("sourceMenu.guide.button"), this);
		messageLabel->setObjectName("Message");
		mainLayout->addWidget(messageLabel);

		// Arrow icon
		QLabel *arrowIcon = new QLabel(this);
		arrowIcon->setScaledContents(true);
		arrowIcon->setObjectName("Arrow");
		mainLayout->addWidget(arrowIcon);
		setLayout(mainLayout);
	}

protected:
	bool event(QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::Enter:
			setProperty("hovered", true);
			pls_flush_style_recursive(this);
			break;
		case QEvent::Leave:
			setProperty("hovered", false);
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
		return QPushButton::event(event);
	}
};
}

PLSAddSourceItem::PLSAddSourceItem(const QString &id, const QString &displayName, bool isThirdPlugin, bool isBeta, QWidget *parent)
	: QPushButton(parent), m_id(id), m_text(displayName), m_isBeta(isBeta)
{
	PLS_PERFORMANCE_FUNCTION();
	m_isNew = isNewSource(id);
	m_isBeta = isBetaSource(id);
	setProperty("sourceId", id);
	setProperty("isNew", m_isNew);
	setProperty("lang", pls_get_current_language());
	setContentsMargins(0, 0, 0, 0);
	ui = pls_new<Ui::PLSAddSourceItem>();
	ui->setupUi(this);
	ui->label_text->setText(displayName);
	m_iconKey = GetIconKey(obs_source_get_icon_type(id.toUtf8().constData())).toLower();
	if (id == "scene") {
		m_iconKey = "scene";
	} else if (id == "group") {
		m_iconKey = "group";
	}
	if (isThirdPlugin) {
		m_iconKey = THIRDPLUGINKEY;
	}
	ui->label_short_desc->setVisible(!isThirdPlugin);

	m_shortDesc = PLSAddSourceView::getSourceShortDesc(id.toUtf8().constData(), m_iconKey.toUtf8().constData());
	ui->label_short_desc->setText(m_shortDesc);
	if (m_isNew) {
		m_newLabel = pls_new<QLabel>(this);
		m_newLabel->setVisible(false);
		m_newLabel->setObjectName("label_status");
		m_newLabel->setFixedSize(NEWICONWIDTH, 16);
		m_newLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
	if (m_isBeta) {
		m_betaLabel = pls_new<QLabel>(this);
		m_betaLabel->setVisible(false);
		m_betaLabel->setObjectName("betaLabel");
		m_betaLabel->setFixedSize(BETAICONWIDTH, 16);
		m_betaLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	}
	setLayout(ui->horizontalLayout);

	connect(this, &QPushButton::toggled, this, &PLSAddSourceItem::statusChanged);
	connect(this, &QPushButton::toggled, this, [this](bool isChecked) { calculateLabelWidth(m_isNew, isChecked); });
	QPixmap pix = OBSBasic::Get()->GetSourcePixmap(id, false);
	ui->label_icon->setPixmap(pix);
	ui->label_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
	ui->label_text->setAttribute(Qt::WA_TransparentForMouseEvents);
	auto iconMargin = 0;
	if (m_isNew)
		iconMargin += NEWICONWIDTH + ITEMTEXTLAYOUTRIGHTMARGIN;
	if (m_isBeta)
		iconMargin += BETAICONWIDTH + ITEMTEXTLAYOUTRIGHTMARGIN;
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

QString PLSAddSourceItem::itemShortDesc() const
{
	return m_shortDesc;
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
	auto margin = ui->horizontalLayout->contentsMargins();
	auto spacing = ui->horizontalLayout->spacing();
	auto iconMargin = 0;
	if (m_isNew)
		iconMargin += NEWICONWIDTH + ITEMTEXTLAYOUTRIGHTMARGIN;
	if (m_isBeta)
		iconMargin += BETAICONWIDTH + ITEMTEXTLAYOUTRIGHTMARGIN;

	auto availableWidth = this->frameSize().width() - margin.left() - ICONWIDTH - spacing - margin.right() - iconMargin;
	auto availableShortDescWidth = this->frameSize().width() - margin.left() - ICONWIDTH - spacing - margin.right();

	const bool hasNew = m_isNew && m_newLabel;
	const bool hasBeta = m_isBeta && m_betaLabel;
	QFontMetrics fontWidth(ui->label_text->font());
	QFontMetrics shortDescFontWidth(ui->label_short_desc->font());
	const QString nameText = m_text;
	const TextLineEnd naturalLineInfo = calculateTextLineEnd(nameText, ui->label_text->font(), availableWidth);
	bool isWarp = naturalLineInfo.lineCount > 1;
	bool isShortDescWarp = shortDescFontWidth.horizontalAdvance(m_shortDesc) > availableShortDescWidth;
	if (isWarp && isShortDescWarp)
		setFixedHeight(ITEMMAXHEIGHT);
	else if (isWarp || isShortDescWarp) {
		setFixedHeight(ITEMMIDHEIGHT);
	} else {
		setFixedHeight(ITEMMINHEIGHT);
	}

	ui->label_text->setWordWrap(isWarp);
	ui->label_short_desc->setWordWrap(isShortDescWarp);

	const QRect textRect = ui->label_text->geometry();
	const int textWidth = textRect.width() > 0 ? textRect.width() : availableWidth;
	const TextLineEnd lineEnd = calculateTextLineEnd(nameText, ui->label_text->font(), textWidth);

	const bool showNew = hasNew;
	const bool showBeta = hasBeta;

	if (!showNew && m_newLabel)
		m_newLabel->setVisible(false);
	if (!showBeta && m_betaLabel)
		m_betaLabel->setVisible(false);
	if (!showNew && !showBeta)
		return;

	const int lineHeight = lineEnd.lineHeight > 0 ? static_cast<int>(std::round(lineEnd.lineHeight)) : fontWidth.height();
	const int lineTop = static_cast<int>(std::round(lineEnd.lineTop));
	const int contentHeight = lineEnd.totalHeight > 0 ? static_cast<int>(std::round(lineEnd.totalHeight)) : lineHeight;
	const int contentTop = textRect.top() + std::max(0, (textRect.height() - contentHeight) / 2);
	int badgeY = contentTop + lineTop + (lineHeight - 16) / 2;
	const int maxBadgeY = std::max(textRect.top(), textRect.bottom() - 16);
	badgeY = std::clamp(badgeY, textRect.top(), maxBadgeY);

	const int textRight = textRect.left() + textRect.width();
	int badgeCursorX = textRect.left() + static_cast<int>(std::round(lineEnd.lineWidth));
	bool placedOne = false;
	constexpr int BETA_BADGE_OFFSET_Y = 3;
	auto placeBadge = [&](QPointer<QLabel> &label, int badgeWidth, bool shouldShow, int offsetY = 0) {
		if (!label)
			return;
		if (!shouldShow) {
			label->setVisible(false);
			return;
		}

		if (!placedOne) {
			badgeCursorX += TEXT_TO_BADGE_GAP;
			placedOne = true;
		} else {
			badgeCursorX += BADGE_BETWEEN_GAP;
		}

		int maxX = textRight - badgeWidth;
		int badgeX = std::min(badgeCursorX, maxX);
		label->move(badgeX, badgeY + offsetY);
		label->setVisible(true);
		badgeCursorX = badgeX + badgeWidth;
	};

	placeBadge(m_betaLabel, BETAICONWIDTH, showBeta, BETA_BADGE_OFFSET_Y);
	placeBadge(m_newLabel, NEWICONWIDTH, showNew);
}

void PLSAddSourceItem::showEvent(QShowEvent *event)
{
	QPushButton::showEvent(event);
	pls_async_call_mt(this, [this]() { calculateLabelWidth(m_isNew, false); });
}

void PLSAddSourceView::openSourceGuideDialog()
{
	done(GUID_EVIEW_RESULT_VALUE);
}

void PLSAddSourceItem::statusChanged(bool isChecked)
{
	QPixmap pix = OBSBasic::Get()->GetSourcePixmap(m_id, isChecked);
	ui->label_icon->setPixmap(pix);
	ui->label_text->setProperty("select", isChecked);
	ui->label_short_desc->setProperty("select", isChecked);
	pls_flush_style(ui->label_text);
	pls_flush_style(ui->label_short_desc);
}

PLSAddSourceView::PLSAddSourceView(QWidget *parent) : PLSDialogView(parent)
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_PERFORMANCE_START(PLSAddSourceView_SetupUi);
	pls_add_css(this, {"PLSAddSourceView"});
	ui = pls_new<Ui::PLSAddSourceView>();
	setupUi(ui);
	PLS_PERFORMANCE_END(PLSAddSourceView_SetupUi);

	setWindowTitle(Str("sourceMenu.view.title"));
	setResizeEnabled(false);
#if defined(Q_OS_MACOS)
	setFixedSize(1005, 680 - PLS_TITLE_BAR_HEIGHT);
#elif defined(Q_OS_WIN)
	setFixedSize(1005, 680);
#endif
	m_movie.setCacheMode(QMovie::CacheAll);
	ui->verticalLayout_baseSource->setAlignment(Qt::AlignTop);
	initSourceItems();
	initSourceLayout(0, DEFAULT_CREATE_GROUP_COUNT);
	connect(&m_buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &PLSAddSourceView::sourceItemChanged);
	connect(ui->buttonOK, &QPushButton::clicked, this, &PLSAddSourceView::okHandler);
	connect(ui->buttonCancel, &QPushButton::clicked, [this]() { PLSAddSourceView::done(Rejected); });
	connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, [this](int value) {
		if (value >= ui->scrollArea->verticalScrollBar()->maximum()) {
			ui->frame_baseSource->repaint();
		}
		auto groupSize = m_sourceItems.size();
		if (m_displayCount < groupSize) {
			initSourceLayout(DEFAULT_CREATE_GROUP_COUNT, groupSize);
		}
	});
	ui->buttonOK->setEnabled(m_buttonGroup.checkedButton() != nullptr);
	setLangShort();
	m_descVideoLabel = new PLSAnimationLabel(this);
	m_descVideoLabel->hide();
	ui->verticalLayout_4->insertWidget(ui->verticalLayout_4->indexOf(ui->label_descGif), m_descVideoLabel, 0, Qt::AlignHCenter | Qt::AlignVCenter);

#if defined(Q_OS_MACOS)
	ui->horizontalLayout_2->insertWidget(1, ui->buttonCancel);
#endif
	ui->verticalLayout_4->setAlignment(ui->label_descContent, Qt::AlignTop);
	auto guideButton = new PLSRecommendationBanner(this);
	pls_add_css(guideButton, {"PLSAddSourceView"});
	ui->horizontalLayout_4->addWidget(guideButton);

	connect(guideButton, &QPushButton::clicked, this, &PLSAddSourceView::openSourceGuideDialog);
	connect(ui->label_descContent, &QLabel::linkActivated, this, [this](const QString &url) {
		PLS_UI_ACTION("click How to use it");
		pls_async_invoke([url]() { QDesktopServices::openUrl(url); });
		PLS_UI_ACTION("addsource view open detail url");
	});

	// Initialize search functionality
	m_searchLineEdit = ui->searchLineEdit;
	if (m_searchLineEdit) {
		m_searchLineEdit->setProperty("usedFor", "addsource");
		m_searchLineEdit->setProperty("leftMargin", 10);
		m_searchLineEdit->setSearchButtonPosition(PLSSearchLineEdit::SearchButtonLeft);
		connect(m_searchLineEdit, &QLineEdit::textChanged, this, &PLSAddSourceView::onSearchTextChanged);
		connect(m_searchLineEdit, &PLSSearchLineEdit::SearchTrigger, this, &PLSAddSourceView::onSearchTextChanged);
	}

	// Create empty state widget
	m_emptyStateWidget = new QWidget(this);
	m_emptyStateWidget->setObjectName("emptyStateWidget");
	pls_add_css(m_emptyStateWidget, {"PLSAddSourceView"});

	auto emptyLayout = new QVBoxLayout(m_emptyStateWidget);
	emptyLayout->setAlignment(Qt::AlignCenter);
	emptyLayout->setSpacing(16);
	emptyLayout->setContentsMargins(0, 0, 0, 0);

	auto emptyIcon = new QLabel(m_emptyStateWidget);
	emptyIcon->setObjectName("emptyStateIcon");
	emptyIcon->setAlignment(Qt::AlignCenter);
	emptyIcon->setScaledContents(true);
	QPixmap emptyIconPixmap = pls_load_pixmap_with_mode(":/resource/images/add-source-view/img_search_empty.svg", QSize(90, 90) * 3);
	emptyIcon->setPixmap(emptyIconPixmap);

	auto emptyText = new QLabel(m_emptyStateWidget);
	emptyText->setObjectName("emptyStateText");
	emptyText->setText(tr("sourceMenu.search.empty"));
	emptyText->setAlignment(Qt::AlignCenter);

	emptyLayout->addStretch();
	emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);
	emptyLayout->addWidget(emptyText, 0, Qt::AlignHCenter);
	emptyLayout->addStretch();
	ui->verticalLayout_2->addWidget(m_emptyStateWidget);
	updateEmptyState(false);

	PLS_PERFORMANCE_START(PLSAddSourceView_uistep_v2_title);
	pls_uistep_v2_set_title(this, QStringLiteral("Add source"));
	for (auto btn : m_buttonGroup.buttons()) {
		if (auto item = dynamic_cast<PLSAddSourceItem *>(btn); item) {
			pls_uistep_v2_add_to_english_cb(item->itemId().toUtf8(), [](const QByteArray &plugin_id, const QString &text) {
				const char *out = nullptr;
				if (text_lookup_get_plugin_english_str(text.toUtf8().constData(), plugin_id.constData(), &out)) {
					return QString::fromUtf8(out);
				}
				return QString();
			});
		}
	}
	PLS_PERFORMANCE_END(PLSAddSourceView_uistep_v2_title);
}

PLSAddSourceView::~PLSAddSourceView()
{
	stopDescVideo();
	for (auto btn : m_buttonGroup.buttons()) {
		if (auto item = dynamic_cast<PLSAddSourceItem *>(btn); item) {
			pls_uistep_v2_remove_to_english_cb(item->itemId().toUtf8());
		}
	}
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

QStringList PLSAddSourceView::getNeedAddSourceList()
{
	return m_needAddSourceList;
}

QString PLSAddSourceView::getSourceShortDesc(const char *sourceId, const char *iconKey)
{
	QString shortDescKey(iconKey);
	if (pls_is_equal(iconKey, THIRDPLUGINKEY)) {
		return QString();
	}
	if (pls_is_equal(iconKey, "monitor")) {
		if (pls_is_equal(sourceId, PRISM_MONITOR_SOURCE_ID)) {
			shortDescKey = QString("%1Full").arg(iconKey);
		} else if (pls_is_equal(sourceId, PRISM_REGION_SOURCE_ID)) {
			shortDescKey = QString("%1Part").arg(iconKey);
		}
	} else if (pls_is_equal(iconKey, "media")) {
		if (pls_is_equal(sourceId, MEDIA_SOURCE_ID)) {
			shortDescKey = iconKey;
		} else if (pls_is_equal(sourceId, VLC_SOURCE_ID)) {
			shortDescKey = "vlc";
		}
	}
	return tr(QString(SOURCSHORTEDESCCONTENT).arg(shortDescKey).toUtf8().constData());
}

void PLSAddSourceView::showEvent(QShowEvent *event)
{
	PLS_PERFORMANCE_FUNCTION();
	PLSDialogView::showEvent(event);
#if defined(Q_OS_MACOS)
	// UX: search box should not be focused by default on Mac
	pls_async_call(this, [this]() {
		if (m_searchLineEdit && m_searchLineEdit->hasFocus()) {
			m_searchLineEdit->clearFocus();
			setFocus();
		}
	});
#endif
	pls_async_call_mt([this]() { setSourceDesc(m_buttonGroup.checkedButton()); });
}

void PLSAddSourceView::initSourceItems()
{
	PLS_PERFORMANCE_FUNCTION();

	std::vector<std::pair<std::string, std::vector<QString>>> presetTypeList;
	std::vector<QString> otherTypeList;
	std::map<QString, bool> monitorPlugins;
	PLS_PERFORMANCE_START(PLSAddSourceView_GetSourceTypeList);
	PLSBasic::GetSourceTypeList(presetTypeList, otherTypeList, monitorPlugins);
	PLS_PERFORMANCE_END(PLSAddSourceView_GetSourceTypeList);
	PLS_PERFORMANCE_START(PLSAddSourceView_create_sourceItem);

	for (auto iter = presetTypeList.begin(); iter != presetTypeList.end(); ++iter) {
		std::vector<QString> &subList = iter->second;
		QVector<PLSAddSourceItem *> items;
		auto groupName = iter->first;
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
			auto sourceItem = new PLSAddSourceItem(id, displayName);
			m_buttonGroup.addButton(sourceItem);
			items.append(sourceItem);
		}
		m_sourceItems.append({groupName, items});
	}
	PLS_PERFORMANCE_END(PLSAddSourceView_create_sourceItem);

	//add other source type
	PLS_PERFORMANCE_START(PLSAddSourceView_create_otherSourceItem);

	QVector<PLSAddSourceItem *> otherItems;
	for (int i = 0; i < otherTypeList.size(); i++) {
		QString id = otherTypeList[i];
		QByteArray cid = id.toUtf8();
		if (pls_is_equal(id, PRISM_CHAT_SOURCE_ID))
			continue;
		auto sourceItem = new PLSAddSourceItem(id, QString::fromUtf8(obs_source_get_display_name(cid.constData())), true);
		m_buttonGroup.addButton(sourceItem);
		otherItems.append(sourceItem);
	}
	PLS_PERFORMANCE_END(PLSAddSourceView_create_otherSourceItem);

	if (!otherItems.isEmpty()) {
		m_sourceItems.append({"sourceMenu.Source.Group.Advance", otherItems});
	}

	for (auto sourceItem : m_sourceItems) {
		auto groupBox = new QGroupBox(tr(sourceItem.first.c_str()));
		auto gridLayout = new QGridLayout();
		groupBox->setLayout(gridLayout);
		gridLayout->setHorizontalSpacing(3);
		gridLayout->setVerticalSpacing(0);
		gridLayout->setContentsMargins(0, 0, 0, 0);
		gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		m_layout.append(gridLayout);
		m_groupBoxes.append(groupBox);

		// Set GroupBox sizePolicy to allow height to shrink based on content
		groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
		groupBox->setMinimumHeight(0);

		ui->verticalLayout_baseSource->addWidget(groupBox, 0, Qt::AlignTop);
	}
}
void PLSAddSourceView::initSourceLayout(int startIndex, int groupCount)
{
	PLS_PERFORMANCE_FUNCTION();

	PLS_PERFORMANCE_START(PLSAddSourceView_sourceItem_addFlowLayout);
	if (startIndex >= groupCount)
		return;
	PLS_PERFORMANCE_END(PLSAddSourceView_sourceItem_addFlowLayout);

	for (auto itemIndex = startIndex; itemIndex < groupCount; ++itemIndex) {
		auto sourceItems = m_sourceItems.value(itemIndex).second;
		auto count = sourceItems.count();

		for (int index = 0; index < count; ++index) {
			m_layout.value(itemIndex)->addWidget(sourceItems.at(index), index / 3, index % 3, Qt::AlignTop);
		}
	}
	m_displayCount = groupCount;
}
void PLSAddSourceView::sourceItemChanged(QAbstractButton *button)
{
	//change desc content
	ui->buttonOK->setEnabled(true);
	setSourceDesc(button);
}
void PLSAddSourceView::okHandler()
{
	PLS_PERFORMANCE_GLOBAL_START("Sources_Click_ShowSource1");
	if (selectSourceId() == PRISM_CHZZK_SPONSOR_SOURCE_ID && !PLSBasic::instance()->bSuccessGetChzzkSourceUrl(this)) {
		return;
	}
	PLS_PERFORMANCE_GLOBAL_START("Sources_Click_ShowSource1-done");
	done(Accepted);
}
extern int getSourceDisplayType(const QString &id);
void PLSAddSourceView::setSourceDesc(QAbstractButton *button)
{
	PLS_PERFORMANCE_FUNCTION();

	auto item = static_cast<PLSAddSourceItem *>(button);
	m_movie.stop();
	stopDescVideo();
	int descType = 0;
	QString key = "default";
	QString title = tr(QString(SOURCEDESCTITLE).arg(key).toUtf8().constData());
	QString translatorKey = key;
	if (item) {
		descType = getSourceDisplayType(item->itemId());
		key = item->itemIconKey();
		title = item->itemDisplayName();
		translatorKey = key;

		if (item->itemId() == PRISM_MONITOR_SOURCE_ID) {
			translatorKey = QString("%1Full").arg(key);
		} else if (item->itemId() == PRISM_REGION_SOURCE_ID) {
			translatorKey = QString("%1Part").arg(key);
		} else if (item->itemId() == MEDIA_SOURCE_ID) {
			translatorKey = key;
		}
#if defined(Q_OS_MACOS)
		else if (item->itemId() == GAME_SOURCE_ID) {
			translatorKey = QStringLiteral("syphon");
		}
#endif
	}

	ui->label_descTitle->setText(title);
	ui->label_descContent->setVisible(!pls_is_equal(translatorKey, THIRDPLUGINKEY));
	auto content = tr((QString(SOURCEDESCCONTENT).arg(translatorKey)).toUtf8().constData());
	ui->label_descContent->setText(content);
	if (key == "spout2") {
		ui->label_descContent->setText(content.arg("https://guide.prismlive.com/desktop/guides/sources/spout2-capture-source/using-spout2-capture-source"));
	}
	calculateDescHeight(ui->label_descTitle);
	calculateDescHeight(ui->label_descContent, true);

	if (descType == 0) {
		ui->label_descGif->setVisible(false);
		ui->label_descPic->setVisible(true);

		QSize size = {280, 140}; // Default size
		QString suffix = "png";
		QString imagePath;

		// Determine size based on key
		if (key == "viewercount" || key == "timer" || key == "prismmobile") {
			// Wide format sources: 153*220
			size = {153, 220};
		} else {
			// Default size: 280*140
			size = {280, 140};
		}

		// Build image path using macros
		if (key == "monitor") {
			// Special handling for monitor sources: map to display/displayall based on source ID
			QString resourceKey = "display"; // Default to display
			if (item && item->itemId() == PRISM_MONITOR_SOURCE_ID) {
				resourceKey = "displayall"; // Full monitor capture uses displayall
			} else if (item && item->itemId() == PRISM_REGION_SOURCE_ID) {
				resourceKey = "display"; // Region capture uses display
			}
#ifdef Q_OS_MACOS
			else if (item && item->itemId() == OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID) {
				// macOS screen capture source uses mac resource
				resourceKey = "mac";
			}
#endif
			imagePath = QString(DESCICONPATH).arg(resourceKey).arg(suffix);
		}
#if defined(Q_OS_WIN)
		else if (key == "audiooutput") {
			imagePath = QString(DESCICONPATH).arg("appaudio").arg(suffix);
		}
#endif
#if defined(Q_OS_MACOS)
		else if (key == "appaudio") {
			imagePath = QString(DESCICONPATH).arg("audiooutput").arg(suffix);
		}
#endif
		else if (key == "textmotion") {
			imagePath = QString(APNGPATHWITHLANG).arg(key).arg(m_langShort).arg(suffix);

		} else {
			// Use DESCICONPATH macro for all other sources
			imagePath = QString(DESCICONPATH).arg(key).arg(suffix);
		}

		ui->label_descPic->setFixedSize(size);

		pls_async_invoke([pthis = pls::QObjectPtr<PLSAddSourceView>(this), imagePath, size]() {
			if (!pthis.valid())
				return;
			QPixmap image = pls_load_pixmap_with_mode(imagePath, size * 3);
			pls_async_call(pthis, [pthis, image]() { pthis->ui->label_descPic->setPixmap(image); });
		});
	} else if (descType == 1) {
		QString suffix = "png";
		QString format = "APNG";
		QSize size = {280, 140};
		ui->label_descGif->setFixedSize(size);
		if (key == "giphy") {
			suffix = "gif";
			format = "GIF";
			size = {140, 126};
			ui->label_descGif->setFixedSize(size);
		} else if (key == "viewercount") {
			size = {152, 220};
			ui->label_descGif->setFixedSize(size);
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
	} else if (descType == 2) {
		QSize size = {280, 140};
		ui->label_descGif->setVisible(false);
		ui->label_descPic->setVisible(false);
		QString fileName = QString((APNGPATHWITHLANG)).arg(key).arg(m_langShort).arg("mp4");
		if (!QFileInfo(fileName).exists()) {
			fileName = QString((APNGPATH)).arg(key).arg("mp4");
		}
		playDescVideo(fileName, size);
	}
	PLS_UI_ACTION("%s information view show", pls_uistep_v2_to_english(title).toUtf8().constData());
}

void PLSAddSourceView::stopDescVideo()
{
	if (m_descVideoLabel) {
		m_descVideoLabel->stop();
		m_descVideoLabel->setVisible(false);
	}
}

void PLSAddSourceView::playDescVideo(const QString &resourcePath, const QSize &size)
{
	if (!m_descVideoLabel) {
		return;
	}

	stopDescVideo();
	m_descVideoLabel->setFixedSize(size);
	m_descVideoLabel->setVisible(true);
	m_descVideoLabel->play(QStringLiteral("qrc%1").arg(resourcePath));
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

void PLSAddSourceView::calculateDescHeight(QLabel *label, bool isHtml)
{
	auto width = 260;
	auto size = pls_calculate_size_for_width(label->text(), label->font(), width);
	label->setFixedSize(width, size.height());
}
void PLSAddSourceView::onSearchTextChanged(const QString &text)
{
	filterSourceItems(text);
}

void PLSAddSourceView::filterSourceItems(const QString &searchText)
{
	if (!ui) {
		return;
	}

	QString trimmedSearch = searchText.trimmed();
	if (trimmedSearch == m_lastTrimmedSearch) {
		return;
	}
	m_lastTrimmedSearch = trimmedSearch;

	bool hasMatch = false;
	QString lowerSearchText = trimmedSearch.toLower();

	// Clear all layouts
	for (auto &layout : m_layout) {
		while (auto item = layout->takeAt(0)) {
			if (item->widget()) {
				item->widget()->setParent(nullptr);
				item->widget()->setVisible(false);
			}
			delete item;
		}
	}
	while (auto item = ui->verticalLayout_baseSource->takeAt(0)) {
		if (item->widget()) {
			item->widget()->setParent(nullptr);
			item->widget()->setVisible(false);
		}
		delete item;
	}

	// Check matches and set visibility/highlight (match against display name and short description)
	for (auto &sourceItem : m_sourceItems) {
		for (auto &item : sourceItem.second) {
			if (!item) {
				continue;
			}

			QString displayName = item->itemDisplayName();
			QString shortDesc = item->itemShortDesc();
			bool itemMatches = lowerSearchText.isEmpty() || displayName.toLower().contains(lowerSearchText) || shortDesc.toLower().contains(lowerSearchText);

			item->setProperty("isVisable", itemMatches);
			if (itemMatches) {
				if (!trimmedSearch.isEmpty()) {
					item->setHighlightText(trimmedSearch);
					hasMatch = true;
				} else {
					item->setHighlightText(QString());
					hasMatch = true;
				}
			} else {
				item->setHighlightText(QString());
			}
		}
	}

	// Re-add matching items to layouts
	for (auto groupIndex = 0; groupIndex < m_sourceItems.size() && groupIndex < m_layout.size(); groupIndex++) {
		QGridLayout *layout = m_layout.at(groupIndex);
		const auto &sourceItems = m_sourceItems.at(groupIndex).second;
		int index = 0;
		for (auto &sourceItem : sourceItems) {
			if (sourceItem && sourceItem->property("isVisable").toBool()) {
				int layoutRow = index / 3;
				int layoutCol = (index++) % 3;
				layout->addWidget(sourceItem, layoutRow, layoutCol, Qt::AlignTop);
			}
		}
	}

	// Handle GroupBox visibility
	for (auto groupIndex = 0; groupIndex < m_sourceItems.size() && groupIndex < m_layout.size(); groupIndex++) {
		QGridLayout *layout = m_layout.at(groupIndex);
		if (layout && layout->count() > 0) {
			auto groupBox = m_groupBoxes.value(groupIndex);
			ui->verticalLayout_baseSource->addWidget(groupBox, 0, Qt::AlignTop);
			groupBox->setVisible(true);
			auto sourceItems = m_sourceItems.at(groupIndex).second;
			for (auto item : sourceItems) {
				item->setVisible(item->property("isVisable").toBool());
			}
		}
	}

	ui->verticalLayout_baseSource->addStretch();

	updateEmptyState(!hasMatch && !trimmedSearch.isEmpty());
}

QString PLSAddSourceView::highlightText(const QString &text, const QString &searchText)
{
	if (searchText.isEmpty()) {
		return text;
	}

	QString escapedText = text.toHtmlEscaped();
	QString escapedSearch = searchText.toHtmlEscaped();

	QString lowerEscapedText = escapedText.toLower();
	QString lowerEscapedSearch = escapedSearch.toLower();

	// Replace from end to avoid index shifting
	QString result = escapedText;
	int searchIndex = lowerEscapedText.length();

	while ((searchIndex = lowerEscapedText.lastIndexOf(lowerEscapedSearch, searchIndex)) != -1) {
		QString before = result.left(searchIndex);
		QString match = result.mid(searchIndex, escapedSearch.length());
		QString after = result.mid(searchIndex + escapedSearch.length());

		QString highlightedMatch = QString("<span style='color:#effc35;'>%1</span>").arg(match);
		result = before + highlightedMatch + after;

		searchIndex--;
		if (searchIndex < 0) {
			break;
		}
	}

	return result;
}

void PLSAddSourceView::updateEmptyState(bool isEmpty)
{
	ui->scrollArea->setVisible(!isEmpty);
	m_emptyStateWidget->setVisible(isEmpty);
}

void PLSAddSourceItem::setHighlightText(const QString &searchText)
{
	if (searchText.isEmpty()) {
		ui->label_text->setText(m_text);
		ui->label_text->setTextFormat(Qt::PlainText);
		ui->label_short_desc->setText(m_shortDesc);
		ui->label_short_desc->setTextFormat(Qt::PlainText);
	} else {
		ui->label_text->setTextFormat(Qt::RichText);
		ui->label_text->setText(PLSAddSourceView::highlightText(m_text, searchText));
		ui->label_short_desc->setTextFormat(Qt::RichText);
		ui->label_short_desc->setText(PLSAddSourceView::highlightText(m_shortDesc, searchText));
	}
}

#include "PLSAddSourceView.moc"