#include "PLSPropertiesExtraUI.hpp"
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSLabel.h"
#include <qpainter.h>
#include <qpainterpath.h>
#include <qmovie.h>
#include <qbuttongroup.h>
#include <libutils-api.h>
#include "pls-common-define.hpp"
#include "obs-properties.h"
#include "pls/pls-properties.h"
#include "PLSComboBox.h"
#include "PLSPushButton.h"
#include "PLSSpinBox.h"
#include "flowlayout.h"
#include <qdir.h>
#include "PLSNameDialog.hpp"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"

#define CTTEMPLATETEXTWIDTH 100

ChatTemplate::ChatTemplate(QButtonGroup *buttonGroup, int id, bool checked)
{

	setObjectName("chatTemplateList_template");
	setProperty("lang", pls_get_current_language());
	setProperty("style", id);
	setCheckable(true);
	setChecked(checked);
	buttonGroup->addButton(this, id);

	QVBoxLayout *vlayout = pls_new<QVBoxLayout>(this);
	vlayout->setContentsMargins(0, 0, 0, 0);
	vlayout->setSpacing(0);

	QLabel *icon = pls_new<QLabel>(this);
	icon->setObjectName("icon");
	icon->setScaledContents(true);
	vlayout->addWidget(icon, 1);

	QLabel *text = pls_new<QLabel>(this);
	text->setObjectName("text");
	text->setText(QString("Style %1").arg(id));
	vlayout->addWidget(text);
}

ChatTemplate::ChatTemplate(QButtonGroup *buttonGroup, int id, const QString text, const QString iconPath, bool isEditUi, const QString &backgroundColor, bool isPaid)
	: m_isPaid(isPaid), m_iconPath(iconPath)
{
	m_enableLabel = pls_new<QLabel>(this);
	m_enableLabel->setObjectName("enableLabel");
	m_enableLabel->setAttribute(Qt::WA_TransparentForMouseEvents, false);
	m_enableLabel->setMouseTracking(true);
	m_editName = text.toUtf8().constData();
	createFrame(isEditUi);
	createPaidLabel(isPaid);
	setObjectName("chat_template_btn");
	setProperty("lang", pls_get_current_language());
	setProperty("style", id);
	setProperty("ID", id);
	setCheckable(true);
	setChecked(false);
	buttonGroup->addButton(this, id);

	QVBoxLayout *vlayout = pls_new<QVBoxLayout>(this);
	vlayout->setContentsMargins(0, 0, 0, 0);

	QFileInfo info(QDir::toNativeSeparators(iconPath));
	QPixmap pixMap;
	m_icon = pls_new<QLabel>(this);
	m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_icon->setObjectName("icon");
	m_icon->setScaledContents(true);

	if (isEditUi) {
		auto layout = pls_new<QHBoxLayout>(m_icon);
		layout->setContentsMargins(0, 0, 0, 0);
		auto customIcon = pls_new<QLabel>();
		customIcon->setObjectName("icon_custom");
		layout->addWidget(customIcon);
		layout->setAlignment(Qt::AlignHCenter);
		customIcon->setScaledContents(true);
		pixMap = pls_load_svg(info.absoluteFilePath(), QSize(98, 43) * 4);
		customIcon->setPixmap(pixMap);
		m_icon->setStyleSheet(QString("background-color:%1;").arg(backgroundColor));
		m_icon->setObjectName("icon1");
	} else {
		pixMap = QPixmap(info.absoluteFilePath());
		m_icon->setPixmap(pixMap);
	}
	vlayout->addWidget(m_icon);

	m_textLabel = pls_new<QLabel>(this);
	m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_textLabel->setObjectName("chat_text");
	m_textLabel->setText(text);
	QString elidedText = this->fontMetrics().elidedText(text, Qt::ElideRight, CTTEMPLATETEXTWIDTH);
	m_textLabel->setText(elidedText);
	vlayout->addWidget(m_textLabel);
	vlayout->setAlignment(Qt::AlignHCenter);
	m_icon->setAlignment(Qt::AlignHCenter);
	this->setLayout(vlayout);
}

void ChatTemplate::updateTemplateRes(const QString &iconPath)
{
	m_iconPath = iconPath;
	QFileInfo info(QDir::toNativeSeparators(iconPath));
	QPixmap pixMap;
	pixMap = QPixmap(info.absoluteFilePath());
	m_icon->setPixmap(pixMap);
}

bool ChatTemplate::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:
		setEditBtnVisible(true);
		break;
	case QEvent::HoverLeave:
		setEditBtnVisible(false);
		break;
	case QEvent::MouseButtonPress:
		break;
	case QEvent::MouseButtonRelease:
		break;
	default:
		break;
	}
	return QPushButton::event(event);
}

void ChatTemplate::createFrame(bool isEdit)
{
	if (!isEdit)
		return;
	m_borderPixMap = pls_load_svg(":/resource/images/text-template/select.svg", QSize(157, 157) * 3);
	m_frame = pls_new<QFrame>();
	m_frame->installEventFilter(this);
	m_frame->setObjectName("chatTemplateEditFrame");
	auto btn1 = pls_new<QPushButton>();
	btn1->setObjectName("chatTemplateRemoveBtn");
	auto btn2 = pls_new<QPushButton>();
	btn2->setObjectName("chatTemplateEditBtn");
	connect(btn1, &QPushButton::clicked, [this]() {
		if (PLSAlertView::Button::Ok == PLSAlertView::information(this, tr("Alert.Title"), QString(tr("Ct.Remove.Custom.Template")).arg(m_editName.c_str()),
									  PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel, PLSAlertView::Button::Ok)) {
			emit resetSourceProperties(property("ID").toInt());
			pls_get_chat_template_helper_instance()->removeCustomTemplate(property("ID").toInt());
		}
	});

	auto layout = pls_new<QVBoxLayout>(m_frame);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 4, 0, 10);
	auto closeLayout = pls_new<QHBoxLayout>();
	closeLayout->setSpacing(0);
	closeLayout->setContentsMargins(0, 0, 4, 0);
	closeLayout->addWidget(btn1, Qt::AlignRight);
	closeLayout->setAlignment(Qt::AlignRight);
	layout->addLayout(closeLayout);
	layout->setAlignment(btn1, Qt::AlignRight);
	layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));

	auto layout2 = pls_new<QHBoxLayout>();
	layout2->setSpacing(0);
	layout2->setContentsMargins(0, 0, 10, 0);
	layout2->setAlignment(Qt::AlignRight);

	layout2->addWidget(btn2);
	layout->addLayout(layout2);

	connect(btn2, &QPushButton::clicked, [this]() {
		auto tmpEditName = m_editName;
		for (;;) {
			bool accepted = PLSNameDialog::AskForName(this, QObject::tr("ChatTemplate.Rename.Title"), QObject::tr("ChatTemplate.Rename.Content"), m_editName, QT_UTF8(m_editName.c_str()));
			if (!accepted || tmpEditName == m_editName) {
				return;
			}

			if (m_editName.empty()) {
				OBSMessageBox::warning(this, QObject::tr("Alert.Title"), QObject::tr("NoNameEntered.Text"));
				continue;
			}
			bool isExist = isChatTemplateNameExist(m_editName.c_str());

			if (isExist) {
				OBSMessageBox::warning(this, QObject::tr("Alert.Title"), QObject::tr("ChatTemplate.Rename.Exist"));
				continue;
			}
			if (!pls_get_chat_template_helper_instance())
				return;
			pls_get_chat_template_helper_instance()->updateCustomTemplateName(m_editName.c_str(), property("ID").toInt());
			QString elidedText = this->fontMetrics().elidedText(m_editName.c_str(), Qt::ElideRight, CTTEMPLATETEXTWIDTH);

			m_textLabel->setText(elidedText);
			return;
		}
	});
}

void ChatTemplate::createPaidLabel(bool isPaid)
{
	if (isPaid) {
		m_paidIcon = pls_new<QLabel>(this);
		m_paidIcon->setObjectName("PAID");
		m_paidIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
		m_paidIcon->setScaledContents(true);
	}
}

bool ChatTemplate::isChatTemplateNameExist(const QString &editName)
{
	if (!pls_get_chat_template_helper_instance())
		return false;
	auto exitNames = pls_get_chat_template_helper_instance()->getChatTemplateName();
	return exitNames.find(editName) != exitNames.end();
}
void ChatTemplate::showEvent(QShowEvent *event)
{
	QPushButton::showEvent(event);

	if (m_frame) {
		m_frame->setParent(this);
		m_frame->resize(this->size());
		m_frame->show();
		setEditBtnVisible(false);
	}
	if (m_paidIcon) {
		m_paidIcon->move(6, 6);
		m_paidIcon->setVisible(!m_iconPath.isEmpty());
		m_paidIcon->raise();
	}
	if (m_enableLabel) {
		m_enableLabel->resize(this->size());
		auto isNull = property("isEnable").isNull();
		if (!isNull && !property("isEnable").toBool()) {
			m_enableLabel->setVisible(true);
			m_enableLabel->raise();
			setAttribute(Qt::WA_TransparentForMouseEvents, true);
		} else {
			m_enableLabel->setVisible(false);
			setAttribute(Qt::WA_TransparentForMouseEvents, false);
		}
	}
}

bool ChatTemplate::eventFilter(QObject *watch, QEvent *event)
{
	if (watch == m_frame && event->type() == QEvent::Paint) {
		if (isChecked()) {
			QPainter painter(m_frame);
			painter.drawPixmap(m_frame->rect(), m_borderPixMap.scaled(m_frame->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			return true;
		}
	}

	return QPushButton::eventFilter(watch, event);
}
void ChatTemplate::setEditBtnVisible(bool isVisible)
{
	if (pls_object_is_valid(m_frame)) {
		auto btns = m_frame->findChildren<QPushButton *>();
		for (auto btn : btns) {
			btn->setVisible(isVisible);
		}
	}
}

TMTextAlignBtn::TMTextAlignBtn(const QString &labelObjStr, bool isChecked, bool isAutoExcusive, QWidget *parent) : QPushButton(parent), m_isChecked(isChecked)
{
	m_iconLabel = pls_new<QLabel>();
	auto layout = pls_new<QHBoxLayout>();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setAlignment(Qt::AlignCenter);
	m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	m_iconLabel->setObjectName(QString("Label_%1").arg(labelObjStr));
	m_iconLabel->setProperty("checked", false);
	layout->addWidget(m_iconLabel);
	this->setLayout(layout);
	if (isAutoExcusive) {
		connect(this, &QAbstractButton::toggled, [this](bool checked) { pls_flush_style_recursive(m_iconLabel, "checked", checked); });
	} else {
		setProperty("checked", m_isChecked);
		setChecked(isChecked);
		connect(this, &QAbstractButton::clicked, [this]() {
			m_isChecked = !m_isChecked;
			pls_flush_style_recursive(m_iconLabel, "checked", m_isChecked);
			setProperty("checked", m_isChecked);
		});
	}
}

bool TMTextAlignBtn::event(QEvent *e)
{
	switch (e->type()) {
	case QEvent::HoverEnter:
		if (isEnabled()) {
			pls_flush_style_recursive(m_iconLabel, "hovered", true);
		} else {
			setToolTip(QObject::tr("textmotion.align.tooltip"));
		}
		break;
	case QEvent::HoverLeave:
		if (isEnabled()) {
			pls_flush_style_recursive(m_iconLabel, "hovered", false);
		}
		break;
	case QEvent::MouseButtonPress:
		if (isEnabled()) {
			pls_flush_style_recursive(m_iconLabel, "pressed", true);
		}
		break;
	case QEvent::MouseButtonRelease:
		if (isEnabled()) {
			pls_flush_style_recursive(m_iconLabel, "pressed", false);
		}
		break;
	case QEvent::EnabledChange:
		pls_flush_style_recursive(m_iconLabel, "enabled", isEnabled());
		break;
	default:
		break;
	}
	pls_flush_style(m_iconLabel);
	return QPushButton::event(e);
}

ImageButton::ImageButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString pixpath, int id, bool checked)
{
	setObjectName(common::OBJECT_NAME_IMAGE_GROUP);
	setProperty("type", type);
	setProperty("id", id);
	setCheckable(true);
	setChecked(checked);
	pls_flush_style_recursive(this, "checked", checked);
	buttonGroup->addButton(this, id);
	if (!pixpath.isEmpty())
		this->bgPixmap.load(pixpath);
}

void ImageButton::paintEvent(QPaintEvent *event)
{
	do {
		if (bgPixmap.isNull())
			break;
		QPainter painter(this);
		painter.setRenderHints(QPainter::Antialiasing, true);
		painter.save();
		QPainterPath painterPath;
		painterPath.addRoundedRect(this->rect(), 0.0, 0.0);
		painter.setClipPath(painterPath);
		painter.drawPixmap(painterPath.boundingRect().toRect(), bgPixmap.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		painter.setPen(Qt::NoPen);
		painter.drawPath(painterPath);
		painter.restore();
	} while (false);

	QPushButton::paintEvent(event);
}

BorderImageButton::BorderImageButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString extraStr, int id, bool checked, bool isBgImg) : m_isBgImg(isBgImg)
{
	auto horizontalLayout = pls_new<QHBoxLayout>();
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(0);
	m_boderLabel = pls_new<QLabel>("");

	horizontalLayout->addWidget(m_boderLabel);
	this->setLayout(horizontalLayout);
	QString objectName = common::OBJECT_NAME_IMAGE_GROUP;
	if (isBgImg) {
		this->setStyleSheet(getTabButtonCss(objectName, id, extraStr));
	} else {

		setProperty("color", extraStr);
	}

	QString boderName = QString::fromUtf8("boderLabel");
	m_boderLabel->setObjectName(boderName);
	setObjectName(objectName);
	setProperty("type", type);
	setProperty("id", id);
	setCheckable(true);
	setChecked(checked);
	setAutoExclusive(true);
	pls_flush_style_recursive(this, "checked", checked);
	buttonGroup->addButton(this, id);
}

QString BorderImageButton::getTabButtonCss(const QString &objectName, int idx, QString url) const
{
	auto styleSheet = QString("PLSPropertiesView #%1[type=\"3\"][id=\"%2\"] {image:url(%3);}").arg(objectName).arg(idx).arg(url);
	return styleSheet;
}
static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
}
void BorderImageButton::paintEvent(QPaintEvent *event)
{
	QPushButton::paintEvent(event);
	if (m_isBgImg) {
		return;
	}

	QPainter painter(this);
	painter.save();
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	painter.setPen(Qt::white);
	auto rectCircle = this->contentsRect();
	auto adjustDpi = 2;
	rectCircle.adjust(adjustDpi, adjustDpi, -adjustDpi, -adjustDpi);
	auto colorStr = property("color").toString();
	if (colorStr.contains('/')) {
		auto colors = colorStr.split('/');
		QColor color1 = color_from_int(colors[0].toLongLong());
		QColor color2 = color_from_int(colors[1].toLongLong());
		color1.setAlpha(255);
		color2.setAlpha(255);
		QLinearGradient gradient(rectCircle.topLeft(), rectCircle.bottomRight());
		gradient.setColorAt(0, color1);
		gradient.setColorAt(1, color2);

		painter.setBrush(gradient);
		painter.drawEllipse(rectCircle);

	} else {

		QColor color = color_from_int(property("color").toLongLong());
		color.setAlpha(255);
		painter.setBrush(QBrush(color));
		painter.drawEllipse(rectCircle);
	}
	if (!isEnabled()) {
		QColor color("#4d2e2e2e");
		painter.setBrush(QBrush(color));
		painter.drawEllipse(rectCircle);
	}
	painter.restore();
}

ImageAPNGButton::ImageAPNGButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString url, int id, bool checked, QSize scaleSize)
{
	auto horizontalLayout = pls_new<QHBoxLayout>();
	horizontalLayout->setContentsMargins(0, 0, 0, 0);
	horizontalLayout->setSpacing(0);

	auto movieLabel = new PLSApngLabel();
	movieLabel->setScaledContents(true);
	auto movie = pls_new<QMovie>(url, "apng", this);
	movie->setCacheMode(QMovie::CacheAll);
	movieLabel->setMovie(movie);
	movieLabel->setObjectName("movieLabel");
	movieLabel->setFixedSize(scaleSize);
	movie->start();

	horizontalLayout->addWidget(movieLabel);
	this->setLayout(horizontalLayout);

	auto horizontalLayout2 = pls_new<QHBoxLayout>(movieLabel);
	horizontalLayout2->setContentsMargins(0, 0, 0, 0);
	horizontalLayout2->setSpacing(0);
	auto borderLabel = pls_new<QLabel>("");
	borderLabel->setObjectName("movieBorderLabel");
	horizontalLayout2->addWidget(borderLabel);

	QString objectName = common::OBJECT_NAME_IMAGE_GROUP;
	setObjectName(objectName);
	setProperty("type", type);
	setProperty("id", id);
	setCheckable(true);
	setChecked(checked);
	setAutoExclusive(true);
	pls_flush_style_recursive(this, "checked", checked);
	buttonGroup->addButton(this, id);
}

CameraVirtualBackgroundStateButton::CameraVirtualBackgroundStateButton(const QString &buttonText, QWidget *parent, const std::function<void()> &clicked) : QFrame(parent)
{
	setProperty("showHandCursor", true);
	setProperty("ui-step.customButton", true);
	setObjectName("cameraVirtualBackgroundButton");
	setMouseTracking(true);

	QLabel *icon = pls_new<QLabel>(this);
	icon->setObjectName("icon");
	icon->setMouseTracking(true);

	QLabel *text = pls_new<QLabel>(this);
	text->setObjectName("text");
	text->setMouseTracking(true);
	text->setText(buttonText);

	QHBoxLayout *layout = pls_new<QHBoxLayout>(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	layout->addWidget(icon);
	layout->addWidget(text);

	connect(this, &CameraVirtualBackgroundStateButton::clicked, this, clicked);
}

void CameraVirtualBackgroundStateButton::setState(const char *name, bool &state, bool value)
{
	if (state != value) {
		pls_flush_style_recursive(this, name, state = value);
	}
}

bool CameraVirtualBackgroundStateButton::event(QEvent *event)

{
	switch (event->type()) {
	case QEvent::Enter:
		setState("hovered", hovered, true);
		break;
	case QEvent::Leave:
		setState("hovered", hovered, false);
		break;
	case QEvent::MouseButtonPress:
		setState("pressed", pressed, true);
		break;
	case QEvent::MouseButtonRelease:
		setState("pressed", pressed, false);
		if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
			clicked();
		}
		break;
	case QEvent::MouseMove:
		setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
		break;
	default:
		break;
	}
	return QFrame::event(event);
}

AddToListDialog::AddToListDialog(QWidget *parent, OBSSource source) : PLSDialogView(parent)
{
	pls_unused(source);
	pls_set_css(this, {"AddToListDialog"});
	resize(QSize(450, 250));

	QVBoxLayout *vLayout = pls_new<QVBoxLayout>();
	vLayout->setAlignment(Qt::AlignCenter);

	QHBoxLayout *hLayout = pls_new<QHBoxLayout>();
	hLayout->setContentsMargins(30, 10, 30, 10);
	hLayout->setSpacing(20);

	bobox = pls_new<PLSComboBox>();
	hLayout->addWidget(bobox);
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "activation_app");
	//prism_source_get_private_data(source, data);
	obs_data_array_t *appArray = obs_data_get_array(data, "get_activation_app");
	obs_data_release(data);
	size_t item_count = obs_data_array_count(appArray);
	bobox->setDisabled(0 == item_count);
	for (int i = 0; i < item_count; i++) {
		obs_data_t *appData = obs_data_array_item(appArray, i);
		const char *name = obs_data_get_string(appData, "name");
		const char *value = obs_data_get_string(appData, "value");
		bobox->addItem(name, value);
	}
	obs_data_array_release(appArray);

	PLSPushButton *addButton = pls_new<PLSPushButton>();
	addButton->setText(QObject::tr("OK"));
	addButton->setFixedWidth(150);
	addButton->setDisabled(0 == item_count);
	connect(addButton, SIGNAL(clicked()), this, SLOT(accept()));
	PLSPushButton *cancelButton = pls_new<PLSPushButton>();
	cancelButton->setText(QObject::tr("Cancel"));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	cancelButton->setFixedWidth(150);

	QHBoxLayout *hLayout2 = pls_new<QHBoxLayout>();
	hLayout2->setContentsMargins(30, 10, 30, 10);
	hLayout2->setSpacing(20);

	hLayout2->addWidget(addButton);
	hLayout2->addStretch();
	hLayout2->addWidget(cancelButton);
	vLayout->addLayout(hLayout);
	vLayout->addLayout(hLayout2);
	this->content()->setLayout(vLayout);
}

inline QString AddToListDialog::GetText() const
{
	return bobox->itemData(bobox->currentIndex()).toString();
}

FontSelectionWindow::FontSelectionWindow(const QList<ITextMotionTemplateHelper::PLSChatDefaultFamily> &families, const QString &selectFamily, QWidget *parent) : QFrame(parent)
{
	pls_add_css(this, {"FontSelectionWindow"});
	auto vlayout = pls_new<QVBoxLayout>();
	vlayout->setContentsMargins(0, 0, 0, 5);
	vlayout->setSpacing(10);
	auto frame = pls_new<QFrame>();
	frame->setWindowFlags(Qt::FramelessWindowHint);
	vlayout->addWidget(frame);

	QHBoxLayout *hLayout = pls_new<QHBoxLayout>();
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(5);
	frame->setLayout(hLayout);
	int count = families.count();

	auto moreFrame = pls_new<QFrame>();
	moreFrame->setWindowFlags(Qt::FramelessWindowHint);
	vlayout->addWidget(moreFrame);
	auto flowLayout = pls_new<FlowLayout>(0, 5, 10);
	moreFrame->setLayout(flowLayout);
	moreFrame->hide();

	auto btnGroup = pls_new<QButtonGroup>(this);
	connect(btnGroup, &QButtonGroup::idClicked, this, [btnGroup, this](int id) { clickFontBtn(btnGroup->button(id)); });

	if (count < 5) {
		PLS_ERROR("FontSelectionWindow", "init font count error");
	}
	for (int i = 0; i < count; i++) {
		auto family = families[i];
		auto btn = pls_new<FontButton>(family.buttonResourceStr, family.buttonWidth);
		btn->setProperty("qtFamily", family.qtFamilyText);
		btn->setCheckable(true);
		btn->setChecked(selectFamily == family.qtFamilyText);
		if (i < 5) {
			hLayout->addWidget(btn);
		} else {
			flowLayout->addWidget(btn);
		}
		btnGroup->addButton(btn, i);
	}
	flowLayout->showLayoutItemWidget();

	auto moreBtn = pls_new<QPushButton>();
	hLayout->addWidget(moreBtn);
	hLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));

	moreBtn->setText("");
	moreBtn->setObjectName("moreBtn");

	QHBoxLayout *layout = pls_new<QHBoxLayout>(moreBtn);
	layout->setContentsMargins(0, 0, 0, 0);
	QLabel *labelIcon = pls_new<QLabel>(moreBtn);
	labelIcon->setObjectName("moreBtnIcon");
	layout->addWidget(labelIcon);
	layout->setAlignment(labelIcon, Qt::AlignCenter);

	connect(moreBtn, &QPushButton::clicked, this, [vlayout, moreFrame, moreBtn]() {
		vlayout->setContentsMargins(0, 0, 0, 0);

		moreFrame->show();
		moreBtn->hide();
	});

	this->setLayout(vlayout);
}

FontSelectionWindow::~FontSelectionWindow() {}

FontButton::FontButton(const QString &resourceName, int width, int height)
	: m_picLabel(pls_new<QLabel>()),
	  m_normalResPath(QString(":/resource/images/chatv2-source/btn_PRISMChat_%1_normal.svg").arg(resourceName)),
	  m_hoveredResPath(QString(":/resource/images/chatv2-source/btn_PRISMChat_%1_over.svg").arg(resourceName)),
	  m_selectedResPath(QString(":/resource/images/chatv2-source/btn_PRISMChat_%1_select.svg").arg(resourceName))
{
	auto mainLayout = pls_new<QHBoxLayout>(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	m_picLabel->setObjectName("fontBgmLabel");
	mainLayout->addWidget(m_picLabel);
	m_picLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	setLayout(mainLayout);
	updateFontButtonBgm(m_normalResPath);
	setStyleSheet(QString("width:%1px;height:%2px;margin:0;padding:0;border:none;").arg(width).arg(height));
}
bool FontButton::event(QEvent *event)

{
	switch (event->type()) {
	case QEvent::Enter:
		isChecked() ? updateFontButtonBgm(m_selectedResPath) : updateFontButtonBgm(m_hoveredResPath);
		break;
	case QEvent::Leave:
		isChecked() ? updateFontButtonBgm(m_selectedResPath) : updateFontButtonBgm(m_normalResPath);
		break;
	case QEvent::MouseButtonPress:
		updateFontButtonBgm(m_selectedResPath);
		break;
	default:
		break;
	}
	return QPushButton::event(event);
}

void FontButton::checkStateSet()
{
	isChecked() ? updateFontButtonBgm(m_selectedResPath) : updateFontButtonBgm(m_normalResPath);
}

void FontButton::updateFontButtonBgm(const QString &bgmRes)
{
	m_picLabel->setStyleSheet(QString("QLabel#fontBgmLabel{image:url(%1);}").arg(bgmRes));
}

NewFlagButton::NewFlagButton(bool isNew, QWidget *parent) : m_isNewFlag(isNew)
{
	if (isNew) {
		m_newLabel = pls_new<QLabel>(this);
		setObjectName("newFlagButton");
		m_newLabel->setObjectName("newLabel");
	}
	connect(this, &QPushButton::clicked, this, [this]() {
		if (!m_newLabel)
			return;
		config_set_bool(App()->GlobalConfig(), common::CHAT_WIDGET_CONFIG, common::CHAT_WIDGET_NEW_BADGE, true);
		config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
		m_newLabel->setVisible(!m_newLabelShowed);
	});
}

void NewFlagButton::showEvent(QShowEvent *event)
{
	QPushButton::showEvent(event);
	if (m_newLabel) {
		m_newLabelShowed = true;
		m_newLabel->setFixedSize(34, 16);
		m_newLabel->move(width() - m_newLabel->width(), (height() - m_newLabel->height()) / 2);
		m_newLabel->raise();
		m_newLabel->show();
	}
}
