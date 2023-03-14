#include <QFormLayout>
#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QFont>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QStandardItem>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QMenu>
#include <QStackedWidget>
#include <QDir>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMovie>
#include "double-slider.hpp"
#include "slider-ignorewheel.hpp"
#include "spinbox-ignorewheel.hpp"
#include "combobox.hpp"
#include "qt-wrappers.hpp"
#include "properties-view.hpp"
#include "properties-view.moc.hpp"
#include "pls-app.hpp"
#include "spinbox.hpp"
#include "dialog-view.hpp"
#include "color-dialog-view.hpp"
#include "font-dialog-view.hpp"
#include "pls-common-define.hpp"
#include "PLSAction.h"
#include "PLSCommonScrollBar.h"
#include "ChannelCommonFunctions.h"
#include "PLSRegionCapture.h"
#include "PLSPushButton.h"
#include "TextMotionTemplateDataHelper.h"
#include "frontend-api.h"
#include "PLSLabel.hpp"
#include "main-view.hpp"

#include <cstdlib>
#include <initializer_list>
#include <string>
#include <QScrollArea>

using namespace std;

#define MAX_TEXT_LENGTH 100000
#define MAX_TM_TEXT_CONTENT_LENGTH 200

enum MusicListRole { NameRole = Qt::UserRole + 1, ProducerRole, DurationRole, DurationTypeRole, UrlRole };

static inline QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
}

static inline long long color_to_int(QColor color)
{
	auto shift = [&](unsigned val, int shift) { return ((val & 0xff) << shift); };

	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}

namespace {

struct frame_rate_tag {
	enum tag_type {
		SIMPLE,
		RATIONAL,
		USER,
	} type = SIMPLE;
	const char *val = nullptr;

	frame_rate_tag() = default;

	explicit frame_rate_tag(tag_type type) : type(type) {}

	explicit frame_rate_tag(const char *val) : type(USER), val(val) {}

	static frame_rate_tag simple() { return frame_rate_tag{SIMPLE}; }
	static frame_rate_tag rational() { return frame_rate_tag{RATIONAL}; }
};

struct common_frame_rate {
	const char *fps_name;
	media_frames_per_second fps;
};
}

Q_DECLARE_METATYPE(frame_rate_tag);
Q_DECLARE_METATYPE(media_frames_per_second);

class TMTextAlignBtn : public QPushButton {
public:
	explicit TMTextAlignBtn(const QString &labelObjStr, bool isChecked = false, bool isAutoExcusive = true, QWidget *parent = nullptr)
		: QPushButton(parent), m_iconLabel(new QLabel()), m_isChecked(isChecked)
	{

		QHBoxLayout *layout = new QHBoxLayout;
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
	~TMTextAlignBtn() {}

protected:
	bool event(QEvent *e) override
	{
		switch (e->type()) {
		case QEvent::HoverEnter:
			if (isEnabled()) {
				pls_flush_style_recursive(m_iconLabel, "hovered", true);
			} else {
				setToolTip(QTStr("textmotion.align.tooltip"));
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

private:
	QLabel *m_iconLabel;
	bool m_isChecked;
};
class TMCheckBox : public QCheckBox {
public:
	TMCheckBox(QWidget *parent = nullptr) : QCheckBox(parent) {}
	~TMCheckBox() {}

protected:
	bool event(QEvent *e) override
	{
		switch (e->type()) {
		case QEvent::HoverEnter: {
			if (!isEnabled() && !isChecked() && (0 == objectName().compare("checkBox", Qt::CaseInsensitive))) {
				setToolTip(QTStr("textmotion.background.tooltip"));
			}
		} break;
		default:
			break;
		}
		return QCheckBox::event(e);
	}
};

class ImageButton : public QPushButton {
public:
	ImageButton(QButtonGroup *buttonGroup, obs_image_style_type type, QString pixpath, int id, bool checked)
	{
		setObjectName(OBJECT_NAME_IMAGE_GROUP);
		setProperty("type", type);
		setProperty("id", id);
		setCheckable(true);
		setChecked(checked);
		pls_flush_style_recursive(this, "checked", checked);
		buttonGroup->addButton(this, id);

		if (!this->bgPixmap.load(pixpath)) {
			char name[256];
			os_extract_file_name(pixpath.toUtf8().constData(), name, ARRAY_SIZE(name) - 1);
			PLS_ERROR(MAIN_SPECTRALIZER_MODULE, "Load pixmap [%s] failed", name);
		}
	}
	~ImageButton() {}

protected:
	virtual void paintEvent(QPaintEvent *event) override
	{
		do {
			if (bgPixmap.isNull())
				break;
			QPainter painter(this);
			painter.setRenderHints(QPainter::Antialiasing, true);
			painter.save();
			double dpi = PLSDpiHelper::getDpi(this);
			QPainterPath painterPath;
			painterPath.addRoundedRect(this->rect(), PLSDpiHelper::calculate(dpi, 0.0), PLSDpiHelper::calculate(dpi, 0.0));
			painter.setClipPath(painterPath);
			painter.drawPixmap(painterPath.boundingRect().toRect(), bgPixmap.scaled(this->width(), this->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
			painter.setPen(Qt::NoPen);
			painter.drawPath(painterPath);
			painter.restore();
		} while (0);

		QPushButton::paintEvent(event);
	}

private:
	QPixmap bgPixmap;
};

class BorderImageButton : public QPushButton {
public:
	explicit BorderImageButton::BorderImageButton(QButtonGroup *buttonGroup, obs_image_style_type type, QString url, int id, bool checked, const char * /*subName*/)
	{
		QHBoxLayout *horizontalLayout = new QHBoxLayout();
		horizontalLayout->setContentsMargins(0, 0, 0, 0);
		horizontalLayout->setSpacing(0);
		m_boderLabel = new QLabel("");

		horizontalLayout->addWidget(m_boderLabel);
		this->setLayout(horizontalLayout);
		QString objectName = OBJECT_NAME_IMAGE_GROUP;
		this->setStyleSheet(getTabButtonCss(objectName, id, url));
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

	~BorderImageButton(){};
	const QString BorderImageButton::getTabButtonCss(const QString &objectName, int idx, QString url)
	{
		auto styleSheet = QString("PLSPropertiesView #%1[type=\"3\"][id=\"%2\"] {image:url(%3);}").arg(objectName).arg(idx).arg(url);
		return styleSheet;
	}

private:
	QLabel *m_boderLabel;
};

class ImageAPNGButton : public QPushButton {
public:
	explicit ImageAPNGButton::ImageAPNGButton(QButtonGroup *buttonGroup, obs_image_style_type type, QString url, int id, bool checked, const char * /*subName*/, double dpi, QSize scaleSize)
	{
		QHBoxLayout *horizontalLayout = new QHBoxLayout();
		horizontalLayout->setContentsMargins(0, 0, 0, 0);
		horizontalLayout->setSpacing(0);
		auto movieLabel = new QLabel("");
		m_movie = new QMovie(url, "apng", this);
		setMovieSize(dpi, scaleSize);
		movieLabel->setMovie(m_movie);
		movieLabel->setObjectName("movieLabel");
		m_movie->start();
		horizontalLayout->addWidget(movieLabel);
		this->setLayout(horizontalLayout);

		QHBoxLayout *horizontalLayout2 = new QHBoxLayout(movieLabel);
		horizontalLayout2->setContentsMargins(0, 0, 0, 0);
		horizontalLayout2->setSpacing(0);
		auto borderLabel = new QLabel("");
		borderLabel->setObjectName("movieBorderLabel");
		horizontalLayout2->addWidget(borderLabel);

		QString objectName = OBJECT_NAME_IMAGE_GROUP;
		setObjectName(objectName);
		setProperty("type", type);
		setProperty("id", id);
		setCheckable(true);
		setChecked(checked);
		setAutoExclusive(true);
		pls_flush_style_recursive(this, "checked", checked);
		buttonGroup->addButton(this, id);
	}
	void setMovieSize(double dpi, QSize _size)
	{
		if (!m_movie) {
			return;
		}
		m_originalSize = _size;
		m_movie->setScaledSize(PLSDpiHelper::calculate(dpi, _size));
	}
	const QSize &getOriginSize() const { return m_originalSize; };
	~ImageAPNGButton(){};

private:
	QSize m_originalSize;
	QMovie *m_movie{};
};

void PLSPropertiesView::ReloadProperties()
{
	ReloadProperties(true);
}

void PLSPropertiesView::ReloadProperties(bool refreshProperties)
{
	//textmotion need create templateButton
	m_tmTabChanged = true;

	if (obj) {
		properties.reset(reloadCallback(obj));
	} else {
		properties.reset(reloadCallback((void *)type.c_str()));
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

	if (refreshProperties) {
		RefreshProperties();
	}
}

#define NO_PROPERTIES_STRING QTStr("Basic.PropertiesWindow.NoProperties")

void PLSPropertiesView::RefreshProperties()
{
	RefreshProperties(std::function<void(QWidget *)>(), true);
}

void PLSPropertiesView::RefreshProperties(std::function<void(QWidget *)> callback, bool update)
{
	int h, v;
	GetScrollPos(h, v);

	{
		children.clear();
		if (contentWidget) {
			contentWidget->deleteLater();
			m_tmLabels.clear();
			m_movieButtons.clear();
		}

		contentWidget = new QWidget(this);
		contentWidget->setFocus();
		contentWidget->setObjectName(OBJECT_NAME_PROPERTIES_CONTENT_WIDGET);

		PLSDpiHelper dpiHelper;
		if (setCustomContentMargins || setCustomContentWidth) {
			int scrollWidth = 0;
			if (scroll && scroll->isVisible()) {
				scrollWidth = scroll->width();
			}

			auto isPrismMobileSource = false;
			if (const auto sourceId = getSourceId(); nullptr != sourceId && strcmp(sourceId, PRISM_MOBILE_SOURCE_ID) == 0) {
				isPrismMobileSource = true;
			}

			double dpi = PLSDpiHelper::getDpi(this);

			if (setCustomContentMargins) {
				contentWidget->setContentsMargins(PLSDpiHelper::calculate(dpi, 10), 0, PLSDpiHelper::calculate(dpi, 19) - scrollWidth,
								  PLSDpiHelper::calculate(dpi, isPrismMobileSource ? 15 : 50));
				dpiHelper.setDynamicContentsMargins(contentWidget, true);
			}

			if (setCustomContentWidth) {
				contentWidget->setMaximumWidth(width() - PLSDpiHelper::calculate(dpi, PLSDialogView::ORIGINAL_RESIZE_BORDER_WIDTH) - (PLSDpiHelper::calculate(dpi, 19) - scrollWidth));
				dpiHelper.setDynamicMaximumWidth(contentWidget, true);
			}

			dpiHelper.notifyDpiChanged(contentWidget, [=](double dpi) {
				int scrollWidth = 0;
				if (scroll && scroll->isVisible()) {
					scrollWidth = scroll->width();
				}

				if (setCustomContentMargins) {
					contentWidget->setContentsMargins(PLSDpiHelper::calculate(dpi, 10), 0, PLSDpiHelper::calculate(dpi, 19) - scrollWidth,
									  PLSDpiHelper::calculate(dpi, isPrismMobileSource ? 15 : 50));
				}
				QMetaObject::invokeMethod(
					this,
					[=]() {
						if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TEXT_MOTION_ID)) {
							int maxSize = 0;
							for (auto w : m_tmLabels) {
								QSize wSize = w->size();
								if (w && (wSize.width() > maxSize)) {
									maxSize = wSize.width();
								}
							}
							for (auto w : m_tmLabels) {
								if (w) {
									w->setMinimumWidth(maxSize);
								}
							}
						}
						if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
							for (QPointer<QPushButton> btn : m_movieButtons) {
								if (!btn) {
									continue;
								}
								auto apngBtn = reinterpret_cast<ImageAPNGButton *>(btn.data());
								if (!apngBtn) {
									continue;
								}
								apngBtn->setMovieSize(dpi, apngBtn->getOriginSize());
							}
						}
					},
					Qt::QueuedConnection);
			});
		}

		const char *id = "";
		// if id is dshow_input, buttons are in the same row according to ux
		//PRISM/Xiewei/20210506/#6666/Add unavailable tips if filter is not available for the source.
		bool filterAvailable = true;
		QString unavailableFilterTips;
		if (obj) {
			auto source = pls_get_source_by_pointer_address(obj);
			if (source) {
				id = obs_source_get_id(source);
				if (id && strcmp(id, FILTER_TYPE_ID_VIDEODELAY_ASYNC) == 0) {
					const char *parantId = obs_source_get_id(obs_filter_get_parent(source));
					if (parantId && strcmp(parantId, DSHOW_SOURCE_ID) == 0) {
						filterAvailable = false;
						unavailableFilterTips = QTStr("Basic.Filters.UnavailableForCamera");
					}
				}
			}
		}

		if (!id) {
			id = "";
		}

		obs_property_t *property = obs_properties_first(properties.get());
		bool hasNoProperties = !property;
		isColorFilter = false;

		bool filterButtonOneRow = false;

		bool isVirtualBackgroundSource = !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID);

		QLayout *layout = nullptr;
		QFormLayout *formLayout = nullptr;
		if (hasNoProperties || showFiltersBtn || !isVirtualBackgroundSource) { // no properties or others
			layout = formLayout = new QFormLayout;
			formLayout->setContentsMargins(0, 0, 0, 0);
			if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
				formLayout->setHorizontalSpacing(10);
			} else {
				formLayout->setHorizontalSpacing(20);
			}
			formLayout->setVerticalSpacing(0);
			formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
			formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		} else { // virtual background
			layout = new QVBoxLayout;
			layout->setContentsMargins(9, 0, 5, 0);
			layout->setSpacing(0);
		}

		contentWidget->setLayout(layout);

		QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		contentWidget->setSizePolicy(policy);

		if (0 == strcmp(id, FILTER_TYPE_ID_APPLYLUT)) {
			showColorFilterPath = false;
			isColorFilter = true;
		}

		if (0 == strcmp(id, PRISM_CHAT_SOURCE_ID) || 0 == strcmp(id, PRISM_TEXT_MOTION_ID) || 0 == strcmp(id, BGM_SOURCE_ID) || 0 == strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID) ||
		    0 == strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) || 0 == strcmp(id, PRISM_TIMER_SOURCE_ID)) {
			filterButtonOneRow = true;
		}

		if (m_tmHelper && m_tmTabChanged && 0 == strcmp(id, PRISM_TEXT_MOTION_ID)) {
			m_tmTabChanged = false;
			m_tmHelper->initTemplateButtons();
		}

		lastPropertyType = OBS_PROPERTY_INVALID;

		while (property) {
			AddProperty(property, layout);
			obs_property_next(&property);
		}

		if (hasNoProperties) {
			// no properties title
			QLabel *noPropertiesLabel = new QLabel(NO_PROPERTIES_STRING);
			noPropertiesLabel->setObjectName("noPropertiesLabel");
			noPropertiesLabel->setAlignment(Qt::AlignLeft);
			formLayout->addRow(noPropertiesLabel);
		}

		//PRISM/Xiewei/20210506/#6666/Add unavailable tips if filter is not available for the source.
		if (!filterAvailable) {
			QLabel *spaceLabel = new QLabel(this);
			spaceLabel->setObjectName(OBJECT_NAME_SPACELABEL);
			formLayout->addRow(nullptr, spaceLabel);
			QLabel *unavailableTips = new QLabel(this);
			unavailableTips->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			unavailableTips->setText(QString("* ").append(unavailableFilterTips));
			unavailableTips->setObjectName("songTipsLabel");
			unavailableTips->setAlignment(Qt::AlignLeft);
			unavailableTips->setWordWrap(true);
			formLayout->addRow(unavailableTips);
		}

		if (showFiltersBtn) {
			//add preview button
			QPushButton *previewButton = new QPushButton(QTStr("Filters"));
			previewButton->setObjectName(OBJECT_NMAE_PREVIEW_BUTTON);
			QLabel *spaceLabel = new QLabel(this);
			spaceLabel->setObjectName(OBJECT_NAME_SPACELABEL);
			connect(previewButton, &QPushButton::clicked, this, [this]() {
				PLS_UI_STEP(PROPERTY_MODULE, "Filter", ACTION_CLICK);
				emit OpenFilters();
			});

			if (hasNoProperties) {
				dpiHelper.setFixedSize(spaceLabel, {10, 22});
				formLayout->addRow(nullptr, spaceLabel);
				formLayout->addRow(previewButton);
			} else {
				dpiHelper.setFixedSize(spaceLabel, {10, 30});
				formLayout->addRow(nullptr, spaceLabel);
				if (filterButtonOneRow || (PROPERTY_FLAG_NO_LABEL_HEADER & obs_data_get_flags(settings))) {
					formLayout->addRow(previewButton);
				} else {
					formLayout->addRow(nullptr, previewButton);
				}
			}
		}
	}

	if (update) {
		PLSDpiHelper::dpiDynamicUpdate(this, false);
	}

	if (callback) {
		callback(contentWidget);
	}

	setWidgetResizable(true);
	setWidget(contentWidget);
	SetScrollPos(h, v);
	QSizePolicy mainPolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setSizePolicy(mainPolicy);

	lastFocused.clear();
	if (lastWidget) {
		lastWidget->setFocus(Qt::OtherFocusReason);
		lastWidget = nullptr;
	}

	emit PropertiesRefreshed();
}

bool PLSPropertiesView::isFirstAddSource() const
{
	return false;
}

const char *PLSPropertiesView::getSourceId() const
{
	if (!obj) {
		return nullptr;
	} else if (auto source = pls_get_source_by_pointer_address(obj); source) {
		return obs_source_get_id(source);
	}
	return nullptr;
}

void PLSPropertiesView::SetScrollPos(int h, int v)
{
	scroll = qobject_cast<PLSCommonScrollBar *>(horizontalScrollBar());
	if (scroll)
		scroll->setValue(h);

	scroll = qobject_cast<PLSCommonScrollBar *>(verticalScrollBar());
	if (scroll)
		scroll->setValue(v);
}

void PLSPropertiesView::GetScrollPos(int &h, int &v)
{
	h = v = 0;

	scroll = qobject_cast<PLSCommonScrollBar *>(horizontalScrollBar());
	if (scroll)
		h = scroll->value();

	scroll = qobject_cast<PLSCommonScrollBar *>(verticalScrollBar());
	if (scroll)
		v = scroll->value();

	connect(scroll, &PLSCommonScrollBar::isShowScrollBar, this, &PLSPropertiesView::OnShowScrollBar);
}

PLSPropertiesView::PLSPropertiesView(OBSData settings_, void *obj_, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback_, int minSize_, int maxSize_, bool showFiltersBtn_,
				     bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties, PLSDpiHelper dpiHelper, bool reloadPropertyOnInit)
	: PLSPropertiesView(nullptr, settings_, obj_, reloadCallback, callback_, minSize_, maxSize_, showFiltersBtn_, showColorFilterPath_, colorFilterOriginalPressed_, refreshProperties, dpiHelper,
			    reloadPropertyOnInit)
{
}

PLSPropertiesView::PLSPropertiesView(OBSData settings_, const char *type_, PropertiesReloadCallback reloadCallback_, int minSize_, int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_,
				     bool colorFilterOriginalPressed_, bool refreshProperties, PLSDpiHelper dpiHelper, bool reloadPropertyOnInit)
	: PLSPropertiesView(nullptr, settings_, type_, reloadCallback_, minSize_, maxSize_, showFiltersBtn_, showColorFilterPath_, colorFilterOriginalPressed_, refreshProperties, dpiHelper,
			    reloadPropertyOnInit)
{
}

PLSPropertiesView::PLSPropertiesView(QWidget *parent, OBSData settings_, void *obj_, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback_, int minSize_, int maxSize_,
				     bool showFiltersBtn_, bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties, PLSDpiHelper dpiHelper, bool reloadPropertyOnInit)
	: VScrollArea(parent),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  obj(obj_),
	  reloadCallback(reloadCallback),
	  callback(callback_),
	  minSize(minSize_),
	  maxSize(maxSize_),
	  showFiltersBtn(showFiltersBtn_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_),
	  m_tmHelper(pls_get_text_motion_template_helper_instance())
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSPropertiesView});
	setFrameShape(QFrame::NoFrame);
	if (reloadPropertyOnInit)
		ReloadProperties(refreshProperties);
}

PLSPropertiesView::PLSPropertiesView(QWidget *parent, OBSData settings_, const char *type_, PropertiesReloadCallback reloadCallback_, int minSize_, int maxSize_, bool showFiltersBtn_,
				     bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties, PLSDpiHelper dpiHelper, bool reloadPropertyOnInit)
	: VScrollArea(parent),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  type(type_),
	  reloadCallback(reloadCallback_),
	  minSize(minSize_),
	  maxSize(maxSize_),
	  showFiltersBtn(showFiltersBtn_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSPropertiesView});
	setFrameShape(QFrame::NoFrame);
	if (reloadPropertyOnInit)
		ReloadProperties(refreshProperties);
}

void PLSPropertiesView::CheckValues()
{
	std::vector<std::shared_ptr<WidgetInfo>>::iterator itr = children.begin();
	while (itr != children.end()) {
		(*itr)->CheckValue();
		++itr;
	}
}

void PLSPropertiesView::textColorChanged(const QByteArray &_id, const QColor &color)
{
	std::shared_ptr<WidgetInfo> info = nullptr;
	std::vector<std::shared_ptr<WidgetInfo>>::iterator itr = children.begin();
	while (itr != children.end()) {
		const char *_settingID = obs_property_name((*itr)->property);
		if (_settingID && _settingID[0] && !strcmp(_id.constData(), _settingID)) {

			info = (*itr);
			break;
		}
		++itr;
	}

	if (info == nullptr) {
		return;
	}
	QLabel *fontLabel = dynamic_cast<QLabel *>(info->widget);
	if (fontLabel == nullptr) {
		return;
	}

	fontLabel->setText(color.name(QColor::HexRgb));
	QPalette palette = QPalette(color);
	fontLabel->setPalette(palette);
	fontLabel->setStyleSheet(QString("background-color :%1; color: %2;").arg(palette.color(QPalette::Window).name(QColor::HexRgb)).arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

	long long color_int = color_to_int(color);

	PLS_INFO(PROPERTY_MODULE, "color is %lld", color_int);

	obs_property_type type = obs_property_get_type(info->property);
	if (type == OBS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(settings, _id.constData());
		obs_data_set_int(color_obj, "color_val", color_int);
		obs_data_release(color_obj);
	} else {
		obs_data_set_int(settings, _id.constData(), color_int);
	}

	refreshViewAfterUIChanged(info->property);
}

void PLSPropertiesView::refreshViewAfterUIChanged(obs_property_t *p)
{
	if (callback && !deferUpdate)
		callback(obj, settings);

	SignalChanged();

	if (obs_property_modified(p, settings)) {
		lastFocused = obs_property_name(p);
		QMetaObject::invokeMethod(this, "RefreshProperties", Qt::QueuedConnection);
	}
}

void PLSPropertiesView::createTMSlider(obs_property_t *prop, int propertyValue, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix, bool, bool isShowSliderIcon,
				       const QString &sliderName)
{
	QHBoxLayout *sliderLayout = new QHBoxLayout;
	sliderLayout->setSpacing(6);
	if (!sliderName.isEmpty()) {
		QLabel *sliderNameLabel = new QLabel;
		sliderNameLabel->setText(sliderName);
		sliderNameLabel->setObjectName("sliderLabel");
		hLayout->addWidget(sliderNameLabel);
	}
	if (isShowSliderIcon) {
		QLabel *leftSliderLabel = new QLabel;
		leftSliderLabel->setObjectName("leftSliderIcon");
		sliderLayout->addWidget(leftSliderLabel);
	}
	SliderIgnoreScroll *slider = new SliderIgnoreScroll();
	slider->setObjectName("slider");
	slider->setProperty("index", propertyValue);
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);
	sliderLayout->addWidget(slider);
	if (isShowSliderIcon) {
		QLabel *rigthSliderLabel = new QLabel;
		rigthSliderLabel->setObjectName("rightSliderIcon");
		sliderLayout->addWidget(rigthSliderLabel);
	}
	PLSSpinBox *spinBox = new PLSSpinBox();
	spinBox->setObjectName("spinBox");
	spinBox->setProperty("index", propertyValue);
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);
	if (isSuffix) {

		spinBox->setSuffix("%");
	}
	hLayout->addLayout(sliderLayout, 1);
	hLayout->addWidget(spinBox);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));
	WidgetInfo *w = new WidgetInfo(this, prop, slider);
	connect(slider, &SliderIgnoreScroll::valueChanged, w, &WidgetInfo::ControlChanged);
	children.emplace_back(w);

	WidgetInfo *w1 = new WidgetInfo(this, prop, spinBox);
	connect(spinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), w1, &WidgetInfo::ControlChanged);
	children.emplace_back(w1);
}

void PLSPropertiesView::createTMSlider(SliderIgnoreScroll *&slider, PLSSpinBox *&spinBox, obs_property_t *prop, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix,
				       bool)
{
	slider = new SliderIgnoreScroll();
	slider->setObjectName("slider");
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);

	spinBox = new PLSSpinBox();
	spinBox->setObjectName("spinBox");
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);
	if (isSuffix) {

		spinBox->setSuffix("%");
	}
	hLayout->addWidget(slider, 1);
	hLayout->addWidget(spinBox);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));
	WidgetInfo *w = new WidgetInfo(this, prop, slider);
	connect(slider, &SliderIgnoreScroll::valueChanged, w, &WidgetInfo::ControlChanged);
	children.emplace_back(w);

	WidgetInfo *w1 = new WidgetInfo(this, prop, spinBox);
	connect(spinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), w1, &WidgetInfo::ControlChanged);
	children.emplace_back(w1);
}

void PLSPropertiesView::createTMColorCheckBox(QCheckBox *&controlCheckBox, obs_property_t *prop, QFrame *&frame, int index, const QString &labelName, QHBoxLayout *layout, bool isControlOn,
					      bool isControl)
{

	QHBoxLayout *bkLabelLalyout = new QHBoxLayout;
	bkLabelLalyout->setSpacing(6);
	bkLabelLalyout->setMargin(0);
	QLabel *bkLabel = new QLabel(labelName);
	bkLabel->setObjectName("bk_outlineLabel");
	TMCheckBox *bkColorCheck = new TMCheckBox();
	controlCheckBox = bkColorCheck;
	bkColorCheck->setObjectName("checkBox");
	bkColorCheck->setProperty("index", index);
	bkLabelLalyout->addWidget(bkLabel);
	bkLabelLalyout->addWidget(bkColorCheck);
	bkColorCheck->setChecked(isControlOn);
	bkColorCheck->setEnabled(isControl);
	bkLabelLalyout->setAlignment(Qt::AlignLeft);

	WidgetInfo *checkInfo = new WidgetInfo(this, prop, bkColorCheck);
	connect(bkColorCheck, &QCheckBox::stateChanged, [checkInfo, layout](int) { checkInfo->ControlChanged(); });

	frame->setLayout(bkLabelLalyout);
}

void PLSPropertiesView::createColorButton(obs_property_t *prop, QGridLayout *&gLayout, QCheckBox *checkBox, const QString &opationName, int index, bool isSuffix, bool)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	QPushButton *button = new QPushButton;
	button->setObjectName("textColorBtn");
	QLabel *colorLabel = new QLabel;
	colorLabel->setProperty("index", index);
	colorLabel->setObjectName("colorLabel");
	colorLabel->setAlignment(Qt::AlignCenter);

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(colorLabel);
	layout->addWidget(button);
	layout->setSpacing(0);
	gLayout->addLayout(layout, index, 1);

	//hLayout->addLayout(layout);

	bool isControlOn;
	bool colorControl;
	bool isAlaph;
	long long colorValue;
	int alaphValue;
	getTmColor(val, index, isControlOn, colorControl, colorValue, isAlaph, alaphValue);
	setLabelColor(colorLabel, colorValue, alaphValue);

	WidgetInfo *info = new WidgetInfo(this, prop, colorLabel);
	connect(button, &QPushButton::clicked, info, &WidgetInfo::ControlChanged);
	children.emplace_back(info);

	QLabel *opationLabel = new QLabel(opationName);
	opationLabel->setObjectName("sliderLabel");
	gLayout->addWidget(opationLabel, index, 2);
	//hLayout->addWidget(opationLabel);

	int minVal = obs_property_tm_text_min(prop, OBS_PROPERTY_TM_COLOR);
	int maxVal = obs_property_tm_text_max(prop, OBS_PROPERTY_TM_COLOR);
	int stepVal = obs_property_tm_text_step(prop, OBS_PROPERTY_TM_COLOR);

	if (2 == index) {
		//outline slider value
		minVal = 1;
		maxVal = 20;
		stepVal = 1;
		alaphValue = obs_data_get_int(val, "outline-color-line");
	}
	QHBoxLayout *sliderHLayout = new QHBoxLayout;
	sliderHLayout->setSpacing(20);
	sliderHLayout->setMargin(0);
	gLayout->addLayout(sliderHLayout, index, 3);
	createTMSlider(prop, index, minVal, maxVal, stepVal, alaphValue, sliderHLayout, isSuffix, colorControl && isControlOn, false);

	setLayoutEnable(layout, colorControl);
	setLayoutEnable(sliderHLayout, colorControl);
	opationLabel->setEnabled(colorControl);

	if (checkBox) {
		connect(checkBox, &QCheckBox::stateChanged, [this, gLayout, button, layout, opationLabel, sliderHLayout](int check) {
			setLayoutEnable(layout, Qt::CheckState::Checked == check);
			setLayoutEnable(sliderHLayout, Qt::CheckState::Checked == check);
			opationLabel->setEnabled(Qt::CheckState::Checked == check);
		});
	}
	obs_data_release(val);
}

void PLSPropertiesView::setLabelColor(QLabel *label, const long long colorValue, const int)
{
	QColor color = color_from_int(colorValue);
	color.setAlpha(255);
	QPalette palette = QPalette(color);
	label->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	label->setText(color.name(QColor::HexRgb));
	label->setPalette(palette);
	label->setStyleSheet(
		QString("font-weight: normal;background-color :%1; color: %2;").arg(palette.color(QPalette::Window).name(QColor::HexRgb)).arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));
	label->setAutoFillBackground(true);
	pls_flush_style(label);
}

void PLSPropertiesView::getTmColor(obs_data_t *textData, int tabIndex, bool &isControlOn, bool &isColor, long long &color, bool &isAlaph, int &alaph)
{
	switch (tabIndex) {
	case 0:
		isColor = obs_data_get_bool(textData, "is-color");
		isControlOn = true;
		color = obs_data_get_int(textData, "text-color");
		isAlaph = obs_data_get_bool(textData, "is-text-color-alpha");
		alaph = obs_data_get_int(textData, "text-color-alpha");
		return;
	case 1:
		isControlOn = obs_data_get_bool(textData, "is-bk-color-on");
		isColor = obs_data_get_bool(textData, "is-bk-color");
		color = obs_data_get_int(textData, "bk-color");
		isAlaph = obs_data_get_bool(textData, "is-bk-color-alpha");
		alaph = obs_data_get_int(textData, "bk-color-alpha");
		return;
	case 2:
		isControlOn = obs_data_get_bool(textData, "is-outline-color-on");
		isColor = obs_data_get_bool(textData, "is-outline-color");
		color = obs_data_get_int(textData, "outline-color");
		isAlaph = true;
		alaph = obs_data_get_int(textData, "outline-color-line");
		return;
	default:
		break;
	}
}

void PLSPropertiesView::createTMButton(const int buttonCount, obs_data_t *, QHBoxLayout *&hLayout, QButtonGroup *&group, ButtonType buttonType, const QStringList &buttonObjs, bool isShowText, bool)
{
	for (int index = 0; index != buttonCount; ++index) {
		QAbstractButton *button = nullptr;
		switch (buttonType) {
		case PLSPropertiesView::RadioButon:
			button = new QRadioButton;
			break;
		case PLSPropertiesView::PushButton: {
			button = new QPushButton;

		} break;
		case PLSPropertiesView::CustomButton: {
			button = new TMTextAlignBtn(buttonObjs.value(index));

		} break;
		case PLSPropertiesView::LetterButton: {
			button = new TMTextAlignBtn(buttonObjs.value(index), false, false);

		} break;
		default:
			break;
		}

		button->setAutoExclusive(true);
		button->setCheckable(true);
		if (buttonObjs.count() == buttonCount) {
			button->setObjectName(buttonObjs.value(index));
			if (isShowText) {
				button->setText(buttonObjs.value(index));
			}
		}

		group->addButton(button, index);
		hLayout->addWidget(button);
	}
}

void PLSPropertiesView::creatTMTextWidget(obs_property_t *prop, const int textCount, obs_data_t *textData, QHBoxLayout *&hLayout)
{
	bool isTmControlOkEnable = true;
	for (int index = 0; index != textCount; ++index) {
		QString text = QT_UTF8(obs_data_get_string(textData, QString("text-content-%1").arg(index + 1).toUtf8()));
		QPlainTextEdit *edit = new QPlainTextEdit(text);
		edit->horizontalScrollBar()->setVisible(false);
		edit->setObjectName(QString("%1_%2").arg(OBJECT_NAME_PLAINTEXTEDIT).arg(index + 1));
		edit->setFrameShape(QFrame::NoFrame);
		WidgetInfo *wi = new WidgetInfo(this, prop, edit);

		connect(edit, &QPlainTextEdit::textChanged, [prop, edit, wi, this]() {
			if (edit->toPlainText().length() > MAX_TM_TEXT_CONTENT_LENGTH) {
				QSignalBlocker signalBlocker(edit);
				edit->setPlainText(edit->toPlainText().left(MAX_TM_TEXT_CONTENT_LENGTH));
				edit->moveCursor(QTextCursor::End);
			}
			const char *name = obs_property_name(prop);
			obs_data_t *val = obs_data_get_obj(settings, name);
			bool isTmControlOkEnable = true;
			if (edit->objectName() == "plainTextEdit_1" && (2 == obs_data_get_int(val, "text-count"))) {
				isTmControlOkEnable = obs_data_get_string(val, "text-content-2")[0] != '\0';
			} else if (edit->objectName() == "plainTextEdit_2" && edit->isVisible()) {
				isTmControlOkEnable = obs_data_get_string(val, "text-content-1")[0] != '\0';
			}
			isTmControlOkEnable = isTmControlOkEnable && (!edit->toPlainText().trimmed().isEmpty());
			okButtonControl(isTmControlOkEnable);

			wi->ControlChanged();
		});

		children.emplace_back(wi);
		hLayout->addWidget(edit);
		if (isTmControlOkEnable && !text.trimmed().isEmpty()) {
			isTmControlOkEnable = true;
		} else {
			isTmControlOkEnable = false;
		}
	}
	QMetaObject::invokeMethod(
		this, [isTmControlOkEnable, this]() { okButtonControl(isTmControlOkEnable); }, Qt::QueuedConnection);
}

void PLSPropertiesView::updateTMTemplateButtons(const int, const QString &templateTabName, QGridLayout *gLayout)
{

	if (!gLayout) {
		return;
	}
	QLayoutItem *child = nullptr;
	while ((child = gLayout->takeAt(0)) != 0) {
		QWidget *w = child->widget();
		w->setParent(nullptr);
	}
	QButtonGroup *buttonGroup = m_tmHelper->getTemplateButtons(templateTabName);
	if (nullptr == buttonGroup) {
		return;
	}
	auto buttons = buttonGroup->buttons();
	for (int index = 0; index != buttons.count(); ++index) {
		QAbstractButton *button = buttons.value(index);
		int Id = button->property("ID").toInt();
		buttonGroup->setId(button, Id);
		gLayout->addWidget(button, index / 4, (index % 4));
	}
}

void PLSPropertiesView::updateFontSytle(const QString &family, PLSComboBox *fontStyleBox)
{
	if (fontStyleBox) {
		fontStyleBox->clear();
		fontStyleBox->addItems(m_fontDatabase.styles(family));
	}
}

void PLSPropertiesView::setLayoutEnable(QLayout *layout, bool isEnable)
{
	if (!layout)
		return;
	int count = layout->count();
	for (auto index = 0; index != count; ++index) {
		QWidget *w = layout->itemAt(index)->widget();
		if (w) {
			w->setEnabled(isEnable);
		} else {
			setLayoutEnable(layout->itemAt(index)->layout(), isEnable);
		}
	}
}

void PLSPropertiesView::createColorTemplate(obs_property_t *prop, QLabel *colorLabel, QPushButton *button, QHBoxLayout *subLayout)
{
	colorLabel->setObjectName("baseColorLabel");
	const char *name = obs_property_name(prop);

	obs_property_type type = obs_property_get_type(prop);
	long long val = 0;
	if (type == OBS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(settings, name);
		val = obs_data_get_int(color_obj, "color_val");
		obs_data_release(color_obj);
	} else {
		val = obs_data_get_int(settings, name);
	}

	QColor color = color_from_int(val);

	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectColor"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	button->setStyleSheet("font-weight:bold;");

	color.setAlpha(255);

	QPalette palette = QPalette(color);
	colorLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	colorLabel->setText(color.name(QColor::HexRgb));
	colorLabel->setPalette(palette);
	colorLabel->setStyleSheet(QString("background-color :%1; color: %2;").arg(palette.color(QPalette::Window).name(QColor::HexRgb)).arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));
	colorLabel->setAutoFillBackground(true);
	colorLabel->setAlignment(Qt::AlignCenter);
	colorLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(colorLabel);
	subLayout->addSpacing(10);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, colorLabel);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);
}

void PLSPropertiesView::setPlaceholderColor_666666(QWidget *widget)
{
	QPalette palette;
	palette.setColor(QPalette::All, QPalette::PlaceholderText, QColor("#666666"));
	widget->setPalette(palette);
}

void PLSPropertiesView::resizeEvent(QResizeEvent *event)
{
	emit PropertiesResized();

	if (!obj) {
		VScrollArea::resizeEvent(event);
	} else if (obs_source_t *source = pls_get_source_by_pointer_address(obj); !source) {
		VScrollArea::resizeEvent(event);
	} else if (const char *id = obs_source_get_id(source); id && !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
		//if (QWidget *widget = VScrollArea::widget(); widget) {
		//	widget->setFixedSize(event->size());
		//}
		//QScrollArea::resizeEvent(event);
		VScrollArea::resizeEvent(event);
	} else {
		VScrollArea::resizeEvent(event);
	}
}

QWidget *PLSPropertiesView::NewWidget(obs_property_t *prop, QWidget *widget, const char *signal)
{
	const char *long_desc = obs_property_long_description(prop);

	WidgetInfo *info = new WidgetInfo(this, prop, widget);
	connect(widget, signal, info, SLOT(UserOperation()));
	connect(widget, signal, info, SLOT(ControlChanged()));
	children.emplace_back(info);

	widget->setToolTip(QT_UTF8(long_desc));
	return widget;
}

QWidget *PLSPropertiesView::AddCheckbox(obs_property_t *prop, QFormLayout *, Qt::LayoutDirection layoutDirection)
{
	const char *name = obs_property_name(prop);
	const char *desc = obs_property_description(prop);
	bool val = obs_data_get_bool(settings, name);

	QCheckBox *checkbox = new QCheckBox(QT_UTF8(desc), this);
	checkbox->setObjectName(OBJECT_NAME_CHECKBOX);
	checkbox->setCheckState(val ? Qt::Checked : Qt::Unchecked);

	if (layoutDirection == Qt::RightToLeft) {
		checkbox->setLayoutDirection(layoutDirection);
		checkbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		checkbox->setObjectName(OBJECT_NAME_FORMLABEL);
	}

	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (source) {
		const char *id = obs_source_get_id(source);
		if (id && id[0] && !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)) {
			checkbox->setObjectName("tmLabel");
		}
	}

	return NewWidget(prop, checkbox, SIGNAL(stateChanged(int)));
}

QWidget *PLSPropertiesView::AddSwitch(obs_property_t *prop, QFormLayout *)
{
	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);
	bool isEnabled = obs_property_enabled(prop);

	QWidget *widget = new QWidget(this);

	QHBoxLayout *hlayout = new QHBoxLayout(widget);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(0);

	QCheckBox *checkbox = new QCheckBox(widget);
	checkbox->setObjectName("switch");
	checkbox->setCheckState(val ? Qt::Checked : Qt::Unchecked);
	checkbox->setEnabled(isEnabled);

	hlayout->addWidget(checkbox, 0, Qt::AlignLeft | Qt::AlignVCenter);

	NewWidget(prop, checkbox, SIGNAL(stateChanged(int)));

	return widget;
}

void PLSPropertiesView::AddRadioButtonGroup(obs_property_t *prop, QFormLayout *layout)
{
	auto name = obs_property_name(prop);
	const auto value = obs_data_get_int(settings, name);
	size_t count = obs_property_bool_group_item_count(prop);

	QHBoxLayout *hBtnLayout = new QHBoxLayout();
	hBtnLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hBtnLayout->setSpacing(20);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	auto buttonGroup = new QButtonGroup(hBtnLayout);

	for (size_t i = 0; i < count; ++i) {
		const char *desc = obs_property_bool_group_item_text(prop, i);
		QRadioButton *radiobutton = new QRadioButton(QT_UTF8(desc), this);
		buttonGroup->addButton(radiobutton);
		radiobutton->setChecked(size_t(value) == i);
		radiobutton->setProperty("idx", i);

		auto enabled = obs_property_bool_group_item_enabled(prop, i);
		radiobutton->setEnabled(enabled);

		auto tooltip = obs_property_bool_group_item_tooltip(prop, i);
		if (nullptr != tooltip) {
			radiobutton->setToolTip(QString::fromUtf8(tooltip));
			if (!enabled) {
				radiobutton->setAttribute(Qt::WA_AlwaysShowToolTips);
			}
		}

		WidgetInfo *info = new WidgetInfo(this, prop, radiobutton);
		connect(radiobutton, SIGNAL(clicked()), info, SLOT(UserOperation()));
		connect(radiobutton, SIGNAL(clicked()), info, SLOT(ControlChanged()));
		children.emplace_back(info);
		hBtnLayout->addWidget(radiobutton);
	}
	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (source) {
		const char *id = obs_source_get_id(source);
		if (id && id[0] && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
			layout->addItem(new QSpacerItem(10, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
		}
	}

	AddSpacer(obs_property_get_type(prop), layout);
	QLabel *nameLabel = new QLabel(QT_UTF8(obs_property_description(prop)));
	nameLabel->setObjectName(OBJECT_NAME_FORMLABEL);
	if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		nameLabel->setWordWrap(true);
	}
	layout->addRow(nameLabel, hBtnLayout);
}

QWidget *PLSPropertiesView::AddText(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	obs_text_type type = obs_property_text_type(prop);

	int maxTextLength = MAX_TEXT_LENGTH;
	int length = obs_property_get_length_limit(prop);
	if (length > 0 && length < MAX_TEXT_LENGTH) {
		maxTextLength = length;
	}

	if (type == OBS_TEXT_MULTILINE) {
		QPlainTextEdit *edit = new QPlainTextEdit(QT_UTF8(val));
		edit->setObjectName(OBJECT_NAME_PLAINTEXTEDIT);
		edit->setFrameShape(QFrame::NoFrame);
		connect(
			edit, &QPlainTextEdit::textChanged, edit,
			[edit, maxTextLength]() {
				if (edit->toPlainText().length() > maxTextLength) {
					QSignalBlocker signalBlocker(edit);
					edit->setPlainText(edit->toPlainText().left(maxTextLength));
					edit->moveCursor(QTextCursor::End);
				}
			},
			Qt::QueuedConnection);

		return NewWidget(prop, edit, SIGNAL(textChanged()));

	} else if (type == OBS_TEXT_PASSWORD) {
		QLayout *subLayout = new QHBoxLayout();
		QLineEdit *edit = new QLineEdit();
		edit->setObjectName(OBJECT_NAME_LINEEDIT);
		QPushButton *show = new QPushButton();

		show->setText(QTStr("Show"));
		show->setCheckable(true);
		edit->setText(QT_UTF8(val));
		edit->setEchoMode(QLineEdit::Password);

		subLayout->addWidget(edit);
		subLayout->addWidget(show);

		WidgetInfo *info = new WidgetInfo(this, prop, edit);
		connect(show, &QAbstractButton::toggled, info, &WidgetInfo::TogglePasswordText);
		connect(show, &QAbstractButton::toggled, [=](bool hide) { show->setText(hide ? QTStr("Hide") : QTStr("Show")); });
		children.emplace_back(info);

		label = new QLabel(QT_UTF8(obs_property_description(prop)));
		layout->addRow(label, subLayout);
		AddSpacer(obs_property_get_type(prop), layout);
		edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

		connect(edit, SIGNAL(textEdited(const QString &)), info, SLOT(ControlChanged()));
		return nullptr;
	}

	QLineEdit *edit = new QLineEdit();
	edit->setObjectName(OBJECT_NAME_LINEEDIT);
	edit->setText(QT_UTF8(val));
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (source) {
		const char *id = obs_source_get_id(source);
		if (id && id[0] && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
			setPlaceholderColor_666666(edit);
		}
	}
	const char *placeholder = obs_property_placeholder(prop);
	if (placeholder && placeholder[0]) {
		edit->setPlaceholderText(QT_UTF8(placeholder));
	}

	return NewWidget(prop, edit, SIGNAL(textEdited(const QString &)));
}

void PLSPropertiesView::AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	PLSDpiHelper dpiHelper;
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	QHBoxLayout *subLayout = new QHBoxLayout();
	subLayout->setSpacing(10);
	QLineEdit *edit = new QLineEdit();
	edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	edit->setObjectName(OBJECT_NAME_LINEEDIT);
	QPushButton *button = new QPushButton(QTStr("Browse"), this);
	dpiHelper.setFixedSize(button, {136, 40});
	//button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	button->setObjectName(OBJECT_NAME_BROWSE);
	if (!obs_property_enabled(prop)) {
		edit->setEnabled(false);
		button->setEnabled(false);
	}

	button->setProperty("themeID", "settingsButtons");
	edit->setText(QT_UTF8(val));
	edit->setReadOnly(true);
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	edit->setStyleSheet("border:none;");

	subLayout->addWidget(edit);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, edit);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	AddSpacer(obs_property_get_type(prop), layout);
	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(*label, subLayout);
}

void PLSPropertiesView::AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	obs_number_type type = obs_property_int_type(prop);
	QHBoxLayout *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	int val = (int)obs_data_get_int(settings, name);
	bool isEnabled = obs_property_enabled(prop);

	spinsView = new PLSSpinBox(this);
	spinsView->makeTextVCenter();
	spinsView->setObjectName(OBJECT_NAME_SPINBOX);

	spinsView->setEnabled(isEnabled);

	int minVal = obs_property_int_min(prop);
	int maxVal = obs_property_int_max(prop);
	int stepVal = obs_property_int_step(prop);
	const char *suffix = obs_property_int_suffix(prop);
	spinsView->setSuffix(QT_UTF8(suffix));

	if (isColorFilter) {
		spinsView->setSuffix("%");
	}

	spinsView->setMinimum(minVal);
	spinsView->setMaximum(maxVal);
	spinsView->setSingleStep(stepVal);
	spinsView->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	if (isColorFilter && colorFilterOriginalPressed) {
		spinsView->setEnabled(false);
		spinsView->setValue(0);
		spinsView->setProperty(STATUS_HANDLE, false);
	} else {
		spinsView->setValue(val);
	}
	infoView = new WidgetInfo(this, prop, spinsView, (colorFilterOriginalPressed && isColorFilter));
	children.emplace_back(infoView);

	if (type == OBS_NUMBER_SLIDER) {
		sliderView = new SliderIgnoreScroll(this);

		sliderView->setObjectName(OBJECT_NAME_SLIDER);
		sliderView->setMinimum(minVal);
		sliderView->setMaximum(maxVal);
		sliderView->setPageStep(stepVal);
		sliderView->setOrientation(Qt::Horizontal);
		sliderView->setEnabled(isEnabled);
		subLayout->addWidget(sliderView);
		subLayout->addSpacing(20);

		if (isColorFilter && colorFilterOriginalPressed) {
			sliderView->setProperty(STATUS_HANDLE, false);
			sliderView->setEnabled(false);
			sliderView->setValue(0);
		} else {
			sliderView->setValue(val);
		}

		connect(sliderView, SIGNAL(valueChanged(int)), spinsView, SLOT(setValue(int)));
		connect(spinsView, SIGNAL(valueChanged(int)), sliderView, SLOT(setValue(int)));
	}

	connect(spinsView, SIGNAL(valueChanged(int)), infoView, SLOT(UserOperation()));
	connect(spinsView, SIGNAL(valueChanged(int)), infoView, SLOT(ControlChanged()));
	connect(spinsView, SIGNAL(valueChanged(int)), this, SLOT(OnIntValueChanged(int)));

	subLayout->addWidget(spinsView);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(*label, subLayout);
}

void PLSPropertiesView::AddFloat(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	obs_number_type type = obs_property_float_type(prop);
	QHBoxLayout *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	double val = obs_data_get_double(settings, name);
	bool isEnabled = obs_property_enabled(prop);

	QDoubleSpinBox *spins = new PLSDoubleSpinBox(this);
	spins->setObjectName(OBJECT_NAME_SPINBOX);

	spins->setEnabled(isEnabled);

	double minVal = obs_property_float_min(prop);
	double maxVal = obs_property_float_max(prop);
	double stepVal = obs_property_float_step(prop);
	const char *suffix = obs_property_float_suffix(prop);

	spins->setMinimum(minVal);
	spins->setMaximum(maxVal);
	spins->setSingleStep(stepVal);
	spins->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spins->setSuffix(QT_UTF8(suffix));
	spins->setValue(val);

	WidgetInfo *info = new WidgetInfo(this, prop, spins);
	children.emplace_back(info);

	if (type == OBS_NUMBER_SLIDER) {
		DoubleSlider *slider = new DoubleSlider(this);
		slider->setObjectName(OBJECT_NAME_SLIDER);
		slider->setDoubleConstraints(minVal, maxVal, stepVal, val);
		slider->setOrientation(Qt::Horizontal);
		slider->setEnabled(isEnabled);
		subLayout->addWidget(slider);
		if (!obs_property_enabled(prop))
			slider->setEnabled(false);
		connect(slider, SIGNAL(doubleValChanged(double)), spins, SLOT(setValue(double)));
		connect(spins, SIGNAL(valueChanged(double)), slider, SLOT(setDoubleVal(double)));
		subLayout->addSpacing(20);
	}

	connect(spins, SIGNAL(valueChanged(double)), info, SLOT(UserOperation()));
	connect(spins, SIGNAL(valueChanged(double)), info, SLOT(ControlChanged()));

	subLayout->addWidget(spins);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(*label, subLayout);
}

static void AddComboItem(QComboBox *combo, obs_property_t *prop, obs_combo_format format, size_t idx)
{
	const char *name = obs_property_list_item_name(prop, idx);
	const char *tips = obs_property_list_item_get_tips(prop, idx);
	QVariant var;

	if (format == OBS_COMBO_FORMAT_INT) {
		long long val = obs_property_list_item_int(prop, idx);
		var = QVariant::fromValue<long long>(val);

	} else if (format == OBS_COMBO_FORMAT_FLOAT) {
		double val = obs_property_list_item_float(prop, idx);
		var = QVariant::fromValue<double>(val);

	} else if (format == OBS_COMBO_FORMAT_STRING) {
		var = QByteArray(obs_property_list_item_string(prop, idx));
	}

	combo->addItem(QT_UTF8(name), var);
	combo->setItemData(idx, tips ? tips : "", Qt::ToolTipRole);

	if (!obs_property_list_item_disabled(prop, idx))
		return;

	//int index = combo->findText(QT_UTF8(name)); // findText() may return incorrect index when more items are using same text
	int index = (combo->count() - 1);
	if (index < 0)
		return;

	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(combo->model());
	if (!model)
		return;

	QStandardItem *item = model->item(index);
	item->setFlags(Qt::NoItemFlags);
}

template<long long get_int(obs_data_t *, const char *), double get_double(obs_data_t *, const char *), const char *get_string(obs_data_t *, const char *)>
static string from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	switch (format) {
	case OBS_COMBO_FORMAT_INT:
		return to_string(get_int(data, name));
	case OBS_COMBO_FORMAT_FLOAT:
		return to_string(get_double(data, name));
	case OBS_COMBO_FORMAT_STRING:
		return get_string(data, name);
	default:
		return "";
	}
}

static string from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	return from_obs_data<obs_data_get_int, obs_data_get_double, obs_data_get_string>(data, name, format);
}

static string from_obs_data_autoselect(obs_data_t *data, const char *name, obs_combo_format format)
{
	return from_obs_data<obs_data_get_autoselect_int, obs_data_get_autoselect_double, obs_data_get_autoselect_string>(data, name, format);
}

static void SetComboBoxStyle(QComboBox *combo, int currentIndex)
{
	if (!combo || -1 == currentIndex) {
		return;
	}
	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(combo->model());
	if (!model) {
		return;
	}
	QStandardItem *item = model->item(currentIndex);
	if (!item) {
		return;
	}
	if (Qt::NoItemFlags == item->flags()) {
		combo->setProperty(STATUS, STATUS_ERROR);
	} else {
		combo->setProperty(STATUS, STATUS_NORMAL);
	}
	pls_flush_style(combo);
}

QWidget *PLSPropertiesView::AddList(obs_property_t *prop, bool &warning)
{
	const char *name = obs_property_name(prop);
	QComboBox *combo = new PLSComboBox(this);
	combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);

	combo->setObjectName(OBJECT_NAME_COMBOBOX);
	obs_combo_type type = obs_property_list_type(prop);
	obs_combo_format format = obs_property_list_format(prop);
	size_t count = obs_property_list_item_count(prop);
	int idx = -1;

	for (size_t i = 0; i < count; i++)
		AddComboItem(combo, prop, format, i);

	if (type == OBS_COMBO_TYPE_EDITABLE) {
		combo->setEditable(true);

		QLineEdit *edit = combo->lineEdit();
		edit->setReadOnly(obs_property_list_readonly(prop));

		// placeholder for no items
		if (combo->count() <= 0) {
			const char *placeholder = obs_property_placeholder(prop);
			if (placeholder && placeholder[0]) {
				edit->setPlaceholderText(QT_UTF8(placeholder));

				PLSDpiHelper dpiHelper;
				dpiHelper.setStyleSheet(edit, "QLineEdit:focus { border: none; }"
							      "QLineEdit { background-color: #1e1e1e; border: none; min-height: /*hdpi*/ 40px; max-height: /*hdpi*/ 40px; }");
			}
		}
	}

	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	string value = from_obs_data(settings, name, format);

	bool checkResolutionChanged = false;

	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (source) {
		const char *id = obs_source_get_id(source);
		if (id && id[0]) {
			if (!strcmp(id, PRISM_NDI_SOURCE_ID)) {
				if (count > 0) {
					combo->setEnabled(true);
				} else if (obs_data_has_user_value(settings, name)) {
					combo->setEnabled(true);
					if (!value.empty() && combo->findData(QByteArray(value.c_str())) < 0) {
						QString qvalue = QString::fromUtf8(value.c_str());
						combo->addItem(qvalue, qvalue);
					}
				} else {
					combo->setEnabled(false);
				}
			} else if (isForPropertyWindow && !strcmp(id, DSHOW_SOURCE_ID)) {
				checkResolutionChanged = true;
			}
		}
	}

	if (format == OBS_COMBO_FORMAT_STRING && type == OBS_COMBO_TYPE_EDITABLE) {
		// Warning : Here we must invoke setText() after setCurrentIndex()
		combo->setCurrentIndex(combo->findData(QByteArray(value.c_str())));
		combo->lineEdit()->setText(QT_UTF8(value.c_str()));
	} else {
		idx = combo->findData(QByteArray(value.c_str()));
	}

	if (isForPropertyWindow) {
		if (0 == strcmp(name, "resolution") && combo->currentIndex() < 0 && combo->count() > 0 && type == OBS_COMBO_TYPE_EDITABLE && format == OBS_COMBO_FORMAT_STRING) {
			combo->setCurrentIndex(0);
			QVariant value = combo->currentText().toUtf8();
			obs_data_set_string(settings, name, value.toByteArray().constData());
		}
	}

	if (type == OBS_COMBO_TYPE_EDITABLE)
		return NewWidget(prop, combo, SIGNAL(editTextChanged(const QString &)));

	if (idx != -1) {
		SetComboBoxStyle(combo, idx);
		combo->setCurrentIndex(idx);
	}

	if (obs_data_has_autoselect_value(settings, name)) {
		string autoselect = from_obs_data_autoselect(settings, name, format);
		int id = combo->findData(QT_UTF8(autoselect.c_str()));

		if (id != -1 && id != idx) {
			QString actual = combo->itemText(id);
			QString selected = combo->itemText(idx);
			QString combined = QTStr("Basic.PropertiesWindow.AutoSelectFormat");
			combo->setItemText(idx, combined.arg(selected).arg(actual));
		}
	}

	QAbstractItemModel *model = combo->model();
	warning = idx != -1 && model->flags(model->index(idx, 0)) == Qt::NoItemFlags;

	WidgetInfo *info = new WidgetInfo(this, prop, combo);
	connect(combo, SIGNAL(currentIndexChanged(int)), info, SLOT(UserOperation()));
	connect(combo, SIGNAL(currentIndexChanged(int)), info, SLOT(ControlChanged()));

	children.emplace_back(info);

	if (checkResolutionChanged && (!strcmp(name, "resolution") || !strcmp(name, "res_type"))) {
		connect(combo, &QComboBox::currentTextChanged, this, [this]() { resolutionChanged = true; });
	}

	/* trigger a settings update if the index was not found */
	if (idx == -1)
		info->ControlChanged();

	return combo;
}

void PLSPropertiesView::AddMusicList(obs_property_t *prop, QFormLayout *layout)
{
	QFrame *frameContianer = new QFrame(this);
	frameContianer->setObjectName("frameContianer");
	QVBoxLayout *layoutContainer = new QVBoxLayout(frameContianer);
	layoutContainer->setContentsMargins(0, 3, 0, 3);
	layoutContainer->setSpacing(0);

	QListWidget *listwidget = new QListWidget(this);
	listwidget->setObjectName("propertyMusicList");
	listwidget->setFrameShape(QFrame::NoFrame);
	listwidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	listwidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	listwidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	listwidget->setContentsMargins(0, 0, 0, 0);
	WidgetInfo *info = new WidgetInfo(this, prop, listwidget);
	connect(listwidget, SIGNAL(currentRowChanged(int)), info, SLOT(UserOperation()));

	size_t count = obs_property_music_group_item_count(prop);
	for (size_t i = 0; i < count; i++) {
		QString name = obs_property_music_group_item_name(prop, i);
		QString producer = obs_property_music_group_item_producer(prop, i);
		if (name.isEmpty() || producer.isEmpty()) {
			continue;
		}
		QString text = name + " - " + producer;
		QListWidgetItem *item = new QListWidgetItem(text);
		item->setToolTip(text);
		listwidget->blockSignals(true);
		listwidget->addItem(item);
		listwidget->setStyleSheet("font-weight: bold;");
		listwidget->blockSignals(false);
	}

	children.emplace_back(info);

	AddSpacer(obs_property_get_type(prop), layout);
	layoutContainer->addWidget(listwidget);
	layout->addRow(frameContianer);
}

QWidget *PLSPropertiesView::AddSelectRegion(obs_property_t *prop, bool &)
{
	QWidget *selectRegionRow = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout(selectRegionRow);
	layout->setContentsMargins(0, 0, 0, 4);
	layout->setSpacing(12);
	QPushButton *btnSelectRegion = new QPushButton(this);
	WidgetInfo *info = new WidgetInfo(this, prop, selectRegionRow);
	connect(btnSelectRegion, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(btnSelectRegion, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	btnSelectRegion->setObjectName("selectRegionBtn");
	btnSelectRegion->setText(QTStr("Basic.PropertiesView.Capture.selectRegion"));
	QLabel *labelTips = new QLabel(selectRegionRow);
	labelTips->setObjectName("selectCaptureRegionTip");
	labelTips->setText(QTStr("Basic.PropertiesView.Capture.tips"));
	layout->addWidget(btnSelectRegion);
	layout->addWidget(labelTips);
	return selectRegionRow;
}

void PLSPropertiesView::AddTips(obs_property_t *prop, QFormLayout *layout)
{
	QFrame *container = new QFrame(this);
	container->setFrameShape(QFrame::NoFrame);
	QHBoxLayout *hLayout = new QHBoxLayout(container);
	hLayout->setAlignment(Qt::AlignLeft);

	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(10);

	const char *name = obs_property_name(prop);
	const char *des = obs_property_description(prop);

	QLabel *songCountLabel{};
	if (name && strlen(name) > 0) {
		songCountLabel = new QLabel(name, this);
		songCountLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		songCountLabel->setObjectName("songCountLabel");

		OBSSource source = pls_get_source_by_pointer_address(obj);
		if (source) {
			const char *id = obs_source_get_id(source);
			if (id && id[0] && !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)) {
				songCountLabel->setObjectName("tmLabel");
			}
		}
	}

	PLSLabel *songTipsLabel{};
	if (des && strlen(des) > 0) {
		songTipsLabel = new PLSLabel(this);
		songTipsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		songTipsLabel->SetText(des);
		songTipsLabel->setObjectName("songTipsLabel");
	}

	if (songCountLabel)
		hLayout->addWidget(songCountLabel);
	if (songTipsLabel)
		hLayout->addWidget(songTipsLabel);

	AddSpacer(obs_property_get_type(prop), layout);

	if ((PROPERTY_FLAG_NO_LABEL_HEADER & obs_data_get_flags(settings)) || (PROPERTY_FLAG_NO_LABEL_SINGLE & obs_property_get_flags(prop))) {
		layout->addRow(container);
	} else {
		layout->addRow(NULL, container);
	}
}

#define PROPERTY_BTN_ADD "propertyButtonAdd"
#define PROPERTY_BTN_REMOVE "propertyButtonRemove"
#define PROPERTY_BTN_CONFIG "propertyButtonConfige"
#define PROPERTY_BTN_MOVE_UP "propertyButtonMoveUp"
#define PROPERTY_BTN_MOVE_DOWN "propertyButtonMoveDown"
#define PROPERTY_FILE_LIST_CTRL "propertyFileListContrl"

static void NewButton(QWidget *pw, QLayout *layout, WidgetInfo *info, const char *objectName, void (WidgetInfo::*method)())
{
	PLSDpiHelper dpiHelper;
	QPushButton *button = new QPushButton(pw);
	button->setObjectName(objectName);
	button->setFlat(true);
	dpiHelper.setMaximumSize(button, {22, 22});
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	button->setStyleSheet("QPushButton{padding:0px;min-height:22px; max-height:22px;}");

	QObject::connect(button, &QPushButton::clicked, info, method);

	layout->addWidget(button);
}

void PLSPropertiesView::AddEditableList(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_array_t *array = obs_data_get_array(settings, name);
	QListWidget *list = new QListWidget();
	list->setObjectName(OBJECT_NAME_EDITABLELIST);
	list->setFrameShape(QFrame::NoFrame);
	size_t count = obs_data_array_count(array);

	if (!obs_property_enabled(prop))
		list->setEnabled(false);

	list->setSortingEnabled(false);
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		list->addItem(QT_UTF8(obs_data_get_string(item, "value")));
		list->setItemSelected(list->item((int)i), obs_data_get_bool(item, "selected"));
		list->setItemHidden(list->item((int)i), obs_data_get_bool(item, "hidden"));
		obs_data_release(item);
	}

	WidgetInfo *info = new WidgetInfo(this, prop, list);

	QWidget *btnUI = new QWidget();
	btnUI->setObjectName(PROPERTY_FILE_LIST_CTRL);

	QVBoxLayout *sideLayout = new QVBoxLayout(btnUI);
	sideLayout->setContentsMargins(9, 9, 9, 9);
	sideLayout->setAlignment(Qt::AlignRight);
	sideLayout->setSpacing(3);

	NewButton(btnUI, sideLayout, info, PROPERTY_BTN_ADD, &WidgetInfo::EditListAdd);
	NewButton(btnUI, sideLayout, info, PROPERTY_BTN_REMOVE, &WidgetInfo::EditListRemove);
	NewButton(btnUI, sideLayout, info, PROPERTY_BTN_CONFIG, &WidgetInfo::EditListEdit);
	NewButton(btnUI, sideLayout, info, PROPERTY_BTN_MOVE_UP, &WidgetInfo::EditListUp);
	NewButton(btnUI, sideLayout, info, PROPERTY_BTN_MOVE_DOWN, &WidgetInfo::EditListDown);
	sideLayout->addStretch(0);

	QHBoxLayout *subLayout = new QHBoxLayout();
	subLayout->addWidget(list);
	subLayout->addWidget(btnUI);

	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);

	obs_data_array_release(array);
}

QWidget *PLSPropertiesView::AddButton(obs_property_t *prop, QFormLayout *layout)
{
	const char *desc = obs_property_description(prop);
	QPushButton *button = new QPushButton(QT_UTF8(desc), this);
	button->setProperty("themeID", "settingsButtons");
	button->setObjectName(OBJECT_NAME_BUTTON);
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	if (0 == strcmp("open_button", obs_property_name(prop))) {
		button->setObjectName("open_button");
		NewWidget(prop, button, SIGNAL(clicked()));
		AddSpacer(obs_property_get_type(prop), layout);
		layout->addRow(button);
		return nullptr;
	}

	return NewWidget(prop, button, SIGNAL(clicked()));
}

void PLSPropertiesView::AddButtonGroup(obs_property_t *prop, QFormLayout *layout)
{
	size_t count = obs_property_button_group_item_count(prop);

	QHBoxLayout *hBtnLayout = new QHBoxLayout;
	hBtnLayout->setSpacing(10);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	bool isFixedWidth = PROPERTY_FLAG_BUTTON_WIDTH_FIXED & obs_property_get_flags(prop);

	for (size_t i = 0; i < count; i++) {
		const char *desc = obs_property_button_group_item_text(prop, i);
		bool enabled = obs_property_button_group_item_enable(prop, i);
		bool hightlight = obs_property_button_group_get_item_highlight(prop, i);
		QPushButton *button = new PLSPushButton(QT_UTF8(desc), this);
		button->setEnabled(enabled);

		WidgetInfo *info = new WidgetInfo(this, prop, button);
		connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
		connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
		children.emplace_back(info);

		button->setProperty("themeID", "settingsButtons");
		button->setProperty("idx", i);
		button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		button->setProperty("yellowText", hightlight);
		if (isFixedWidth) {
			button->setObjectName("FixedButton");
		} else {
			button->setObjectName(OBJECT_NAME_BUTTON + QString::number(i + 1));
		}

		hBtnLayout->addWidget(button);
	}

	AddSpacer(obs_property_get_type(prop), layout);
	QLabel *nameLabel = new QLabel(QT_UTF8(obs_property_description(prop)));
	nameLabel->setObjectName(OBJECT_NAME_FORMLABEL);
	if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		nameLabel->setWordWrap(true);
	}
	layout->addRow(nameLabel, hBtnLayout);
}

void PLSPropertiesView::AddColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	QPushButton *button = new QPushButton;
	QLabel *colorLabel = new QLabel;
	QHBoxLayout *subLayout = new QHBoxLayout;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		colorLabel->setEnabled(false);
	}

	createColorTemplate(prop, colorLabel, button, subLayout);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);
}

static void MakeQFont(obs_data_t *font_obj, QFont &font, bool limit = false)
{
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	int size = (int)obs_data_get_int(font_obj, "size");
	uint32_t flags = (uint32_t)obs_data_get_int(font_obj, "flags");

	if (face) {
		font.setFamily(face);
		font.setStyleName(style);
	}

	if (size) {
		if (limit) {
			int max_size = font.pointSize();
			if (max_size < 28)
				max_size = 28;
			if (size > max_size)
				size = max_size;
		}
		font.setPointSize(size);
	}

	if (flags & OBS_FONT_BOLD)
		font.setBold(true);
	if (flags & OBS_FONT_ITALIC)
		font.setItalic(true);
	if (flags & OBS_FONT_UNDERLINE)
		font.setUnderline(true);
	if (flags & OBS_FONT_STRIKEOUT)
		font.setStrikeOut(true);
}

void PLSPropertiesView::AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *font_obj = obs_data_get_obj(settings, name);
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	QPushButton *button = new QPushButton;
	button->setObjectName(OBJECT_NAME_FONTBUTTON);

	QLabel *fontLabel = new QLabel(this);
	fontLabel->setObjectName(OBJECT_NAME_FONTLABEL);
	QFont font;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		fontLabel->setEnabled(false);
	}

	font = fontLabel->font();
	MakeQFont(font_obj, font, true);

	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectFont"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	//button->setStyleSheet("font-weight:bold;padding-left:10px;padding-right:10px;min-width:130px;");

	fontLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	fontLabel->setFont(font);
	fontLabel->setStyleSheet(QString("font-size:%1px;").arg(font.pointSize()));
	fontLabel->setText(QString("%1 %2").arg(face, style));
	fontLabel->setAlignment(Qt::AlignCenter);
	fontLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	QHBoxLayout *subLayout = new QHBoxLayout;
	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(fontLabel);
	subLayout->addSpacing(10);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, fontLabel);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);

	obs_data_release(font_obj);
}

void PLSPropertiesView::AddFontSimple(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{

	const char *name = obs_property_name(prop);
	obs_data_t *font_obj = obs_data_get_obj(settings, name);
	const char *family = obs_data_get_string(font_obj, "font-family");
	const char *style = obs_data_get_string(font_obj, "font-weight");
	obs_data_release(font_obj);

	QHBoxLayout *hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(10);

	PLSComboBox *fontCbx = new PLSComboBox();
	fontCbx->setObjectName("FontCheckedFamilyBox");
	fontCbx->addItem(family);
	fontCbx->setCurrentText(family);

	PLSComboBox *weightCbx = new PLSComboBox();
	weightCbx->blockSignals(true);
	updateFontSytle(family, weightCbx);
	weightCbx->setCurrentText(style);
	weightCbx->blockSignals(false);

	weightCbx->setObjectName("FontCheckedWidgetBox");
	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), [this, weightCbx](const QString &text) {
		weightCbx->blockSignals(true);
		updateFontSytle(text, weightCbx);
		weightCbx->blockSignals(false);
	});

	QMetaObject::invokeMethod(
		fontCbx,
		[family, fontCbx, this]() {
			QSignalBlocker block(fontCbx);
			fontCbx->clear();
			fontCbx->addItems(m_fontDatabase.families());
			fontCbx->setCurrentText(family);
		},
		Qt::QueuedConnection);
	hlayout->addWidget(fontCbx);
	hlayout->addWidget(weightCbx);
	hlayout->setStretch(0, 292);
	hlayout->setStretch(1, 210);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, hlayout);

	WidgetInfo *fontWidgetInfo = new WidgetInfo(this, prop, fontCbx);
	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), fontWidgetInfo, &WidgetInfo::ControlChanged);
	children.emplace_back(fontWidgetInfo);

	WidgetInfo *fontStyleWidgetInfo = new WidgetInfo(this, prop, weightCbx);
	connect(weightCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), fontStyleWidgetInfo, &WidgetInfo::ControlChanged);
	children.emplace_back(fontStyleWidgetInfo);

	if (!obs_property_enabled(prop)) {
		fontCbx->setEnabled(false);
		weightCbx->setEnabled(false);
	}
}

void PLSPropertiesView::AddColorCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *& /*label*/)
{
	QPushButton *button = new QPushButton;
	QLabel *colorLabel = new QLabel;
	QCheckBox *checkBox = new QCheckBox(QT_UTF8(obs_property_description(prop)));

	QHBoxLayout *subLayout = new QHBoxLayout;
	createColorTemplate(prop, colorLabel, button, subLayout);
	checkBox->setEnabled(true);
	checkBox->setLayoutDirection(Qt::RightToLeft);
	checkBox->setObjectName(OBJECT_NAME_FORMCHECKBOX);

	obs_data_t *color_obj = obs_data_get_obj(settings, obs_property_name(prop));
	bool isEnable = obs_data_get_bool(color_obj, "is_enable");
	obs_data_release(color_obj);

	colorLabel->setEnabled(isEnable);
	button->setEnabled(isEnable);
	checkBox->setChecked(isEnable);

	connect(checkBox, &QCheckBox::stateChanged, [this, button, colorLabel](int state) {
		bool isCheck = static_cast<Qt::CheckState>(state) == Qt::Checked;
		button->setEnabled(isCheck);
		colorLabel->setEnabled(isCheck);
	});

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(checkBox, subLayout);

	WidgetInfo *info = new WidgetInfo(this, prop, checkBox);
	connect(checkBox, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(checkBox, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);
}

namespace std {
template<> struct default_delete<obs_data_t> {
	void operator()(obs_data_t *data) { obs_data_release(data); }
};

template<> struct default_delete<obs_data_item_t> {
	void operator()(obs_data_item_t *item) { obs_data_item_release(&item); }
};
}

template<typename T> static double make_epsilon(T val)
{
	return val * 0.00001;
}

static bool matches_range(media_frames_per_second &match, media_frames_per_second fps, const frame_rate_range_t &pair)
{
	auto val = media_frames_per_second_to_frame_interval(fps);
	auto max_ = media_frames_per_second_to_frame_interval(pair.first);
	auto min_ = media_frames_per_second_to_frame_interval(pair.second);

	if (min_ <= val && val <= max_) {
		match = fps;
		return true;
	}

	return false;
}

static bool matches_ranges(media_frames_per_second &best_match, media_frames_per_second fps, const frame_rate_ranges_t &fps_ranges, bool exact = false)
{
	auto convert_fn = media_frames_per_second_to_frame_interval;
	auto val = convert_fn(fps);
	auto epsilon = make_epsilon(val);

	bool match = false;
	constexpr auto best_dist = numeric_limits<double>::max();
	for (auto &pair : fps_ranges) {
		auto max_ = convert_fn(pair.first);
		auto min_ = convert_fn(pair.second);
		/*plog(LOG_INFO, "%lg ≤ %lg ≤ %lg? %s %s %s",
				min_, val, max_,
				fabsl(min_ - val) < epsilon ? "true" : "false",
				min_ <= val && val <= max_  ? "true" : "false",
				fabsl(min_ - val) < epsilon ? "true" :
				"false");*/

		if (matches_range(best_match, fps, pair))
			return true;

		if (exact)
			continue;

		auto min_dist = fabsl(min_ - val);
		auto max_dist = fabsl(max_ - val);
		if (min_dist < epsilon && min_dist < best_dist) {
			best_match = pair.first;
			match = true;
			continue;
		}

		if (max_dist < epsilon && max_dist < best_dist) {
			best_match = pair.second;
			match = true;
			continue;
		}
	}

	return match;
}

static media_frames_per_second make_fps(uint32_t num, uint32_t den)
{
	media_frames_per_second fps{};
	fps.numerator = num;
	fps.denominator = den;
	return fps;
}

static const common_frame_rate common_fps[] = {
	{"60", {60, 1}}, {"59.94", {60000, 1001}}, {"50", {50, 1}}, {"48", {48, 1}}, {"30", {30, 1}}, {"29.97", {30000, 1001}}, {"25", {25, 1}}, {"24", {24, 1}}, {"23.976", {24000, 1001}},
};

static void UpdateSimpleFPSSelection(PLSFrameRatePropertyWidget *fpsProps, const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->simpleFPS->setCurrentIndex(0);
		return;
	}

	auto combo = fpsProps->simpleFPS;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<media_frames_per_second>())
			continue;

		auto fps = variant.value<media_frames_per_second>();
		if (fps != *current_fps)
			continue;

		combo->setCurrentIndex(i);
		return;
	}

	combo->setCurrentIndex(0);
}

static void AddFPSRanges(vector<common_frame_rate> &items, const frame_rate_ranges_t &ranges)
{
	auto InsertFPS = [&](media_frames_per_second fps) {
		auto fps_val = media_frames_per_second_to_fps(fps);

		auto end_ = end(items);
		auto i = begin(items);
		for (; i != end_; i++) {
			auto i_fps_val = media_frames_per_second_to_fps(i->fps);
			if (fabsl(i_fps_val - fps_val) < 0.01)
				return;

			if (i_fps_val > fps_val)
				continue;

			break;
		}

		items.insert(i, {nullptr, fps});
	};

	for (auto &range : ranges) {
		InsertFPS(range.first);
		InsertFPS(range.second);
	}
}

static QWidget *CreateSimpleFPSValues(PLSFrameRatePropertyWidget *fpsProps, bool &selected, const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QVBoxLayout{};
	layout->setContentsMargins(0, 0, 0, 0);

	auto items = vector<common_frame_rate>{};
	items.reserve(sizeof(common_fps) / sizeof(common_frame_rate));

	auto combo = fpsProps->simpleFPS = new PLSComboBox{};

	combo->addItem("", QVariant::fromValue(make_fps(0, 0)));
	for (const auto &fps : common_fps) {
		media_frames_per_second best_match{};
		if (!matches_ranges(best_match, fps.fps, fpsProps->fps_ranges))
			continue;

		items.push_back({fps.fps_name, best_match});
	}

	AddFPSRanges(items, fpsProps->fps_ranges);

	for (const auto &item : items) {
		auto var = QVariant::fromValue(item.fps);
		const auto &name = item.fps_name ? QString(item.fps_name) : QString("%1").arg(media_frames_per_second_to_fps(item.fps));
		combo->addItem(name, var);

		bool select = current_fps && *current_fps == item.fps;
		if (select) {
			combo->setCurrentIndex(combo->count() - 1);
			selected = true;
		}
	}

	layout->addWidget(combo, 0, Qt::AlignTop);
	widget->setLayout(layout);

	return widget;
}

static void UpdateRationalFPSWidgets(PLSFrameRatePropertyWidget *fpsProps, const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->numEdit->setValue(0);
		fpsProps->denEdit->setValue(0);
		return;
	}

	auto combo = fpsProps->fpsRange;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<size_t>())
			continue;

		auto idx = variant.value<size_t>();
		if (fpsProps->fps_ranges.size() < idx)
			continue;

		media_frames_per_second match{};
		if (!matches_range(match, *current_fps, fpsProps->fps_ranges[idx]))
			continue;

		combo->setCurrentIndex(i);
		break;
	}

	fpsProps->numEdit->setValue(current_fps->numerator);
	fpsProps->denEdit->setValue(current_fps->denominator);
}

static QWidget *CreateRationalFPS(PLSFrameRatePropertyWidget *fpsProps, bool &selected, const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QFormLayout{};
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	auto str = QTStr("Basic.PropertiesView.FPS.ValidFPSRanges");
	auto rlabel = new QLabel{str};

	auto combo = fpsProps->fpsRange = new PLSComboBox{};
	auto convert_fps = media_frames_per_second_to_fps;
	//auto convert_fi  = media_frames_per_second_to_frame_interval;

	for (size_t i = 0; i < fpsProps->fps_ranges.size(); i++) {
		auto &pair = fpsProps->fps_ranges[i];
		combo->addItem(QString{"%1 - %2"}.arg(convert_fps(pair.first)).arg(convert_fps(pair.second)), QVariant::fromValue(i));

		media_frames_per_second match;
		if (!current_fps || !matches_range(match, *current_fps, pair))
			continue;

		combo->setCurrentIndex(combo->count() - 1);
		selected = true;
	}

	layout->addRow(rlabel, combo);

	auto num_edit = fpsProps->numEdit = new SpinBoxIgnoreScroll{};
	auto den_edit = fpsProps->denEdit = new SpinBoxIgnoreScroll{};

	num_edit->setRange(0, INT_MAX);
	den_edit->setRange(0, INT_MAX);

	if (current_fps) {
		num_edit->setValue(current_fps->numerator);
		den_edit->setValue(current_fps->denominator);
	}

	layout->addRow(QTStr("Basic.Settings.Video.Numerator"), num_edit);
	layout->addRow(QTStr("Basic.Settings.Video.Denominator"), den_edit);

	widget->setLayout(layout);

	return widget;
}

static PLSFrameRatePropertyWidget *CreateFrameRateWidget(obs_property_t *prop, bool &warning, const char *option, media_frames_per_second *current_fps, frame_rate_ranges_t &fps_ranges)
{
	auto widget = new PLSFrameRatePropertyWidget{};
	auto hlayout = new QHBoxLayout{};
	hlayout->setContentsMargins(0, 0, 0, 0);

	swap(widget->fps_ranges, fps_ranges);

	auto combo = widget->modeSelect = new PLSComboBox{};
	combo->addItem(QTStr("Basic.PropertiesView.FPS.Simple"), QVariant::fromValue(frame_rate_tag::simple()));
	combo->addItem(QTStr("Basic.PropertiesView.FPS.Rational"), QVariant::fromValue(frame_rate_tag::rational()));

	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto num = obs_property_frame_rate_options_count(prop);
	if (num)
		combo->insertSeparator(combo->count());

	bool option_found = false;
	for (size_t i = 0; i < num; i++) {
		auto name = obs_property_frame_rate_option_name(prop, i);
		auto desc = obs_property_frame_rate_option_description(prop, i);
		combo->addItem(desc, QVariant::fromValue(frame_rate_tag{name}));

		if (!name || !option || string(name) != option)
			continue;

		option_found = true;
		combo->setCurrentIndex(combo->count() - 1);
	}

	hlayout->addWidget(combo, 0, Qt::AlignTop);

	auto stack = widget->modeDisplay = new QStackedWidget{};

	bool match_found = option_found;
	auto AddWidget = [&](decltype(CreateRationalFPS) func) {
		bool selected = false;
		stack->addWidget(func(widget, selected, current_fps));

		if (match_found || !selected)
			return;

		match_found = true;

		stack->setCurrentIndex(stack->count() - 1);
		combo->setCurrentIndex(stack->count() - 1);
	};

	AddWidget(CreateSimpleFPSValues);
	AddWidget(CreateRationalFPS);
	stack->addWidget(new QWidget{});

	if (option_found)
		stack->setCurrentIndex(stack->count() - 1);
	else if (!match_found) {
		int idx = current_fps ? 1 : 0; // Rational for "unsupported"
					       // Simple as default
		stack->setCurrentIndex(idx);
		combo->setCurrentIndex(idx);
		warning = true;
	}

	hlayout->addWidget(stack, 0, Qt::AlignTop);

	auto label_area = widget->labels = new QWidget{};
	label_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto vlayout = new QVBoxLayout{};
	vlayout->setContentsMargins(0, 0, 0, 0);

	auto fps_label = widget->currentFPS = new QLabel{"FPS: 22"};
	auto time_label = widget->timePerFrame = new QLabel{"Frame Interval: 0.123 ms"};
	auto min_label = widget->minLabel = new QLabel{"Min FPS: 1/1"};
	auto max_label = widget->maxLabel = new QLabel{"Max FPS: 2/1"};

	min_label->setHidden(true);
	max_label->setHidden(true);

	auto flags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;
	min_label->setTextInteractionFlags(flags);
	max_label->setTextInteractionFlags(flags);

	vlayout->addWidget(fps_label);
	vlayout->addWidget(time_label);
	vlayout->addWidget(min_label);
	vlayout->addWidget(max_label);
	label_area->setLayout(vlayout);

	hlayout->addWidget(label_area, 0, Qt::AlignTop);

	widget->setLayout(hlayout);

	return widget;
}

static void UpdateMinMaxLabels(PLSFrameRatePropertyWidget *w)
{
	auto Hide = [&](bool hide) {
		w->minLabel->setHidden(hide);
		w->maxLabel->setHidden(hide);
	};

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>() || variant.value<frame_rate_tag>().type != frame_rate_tag::RATIONAL) {
		Hide(true);
		return;
	}

	variant = w->fpsRange->currentData();
	if (!variant.canConvert<size_t>()) {
		Hide(true);
		return;
	}

	auto idx = variant.value<size_t>();
	if (idx >= w->fps_ranges.size()) {
		Hide(true);
		return;
	}

	Hide(false);

	auto min = w->fps_ranges[idx].first;
	auto max = w->fps_ranges[idx].second;

	w->minLabel->setText(QString("Min FPS: %1/%2").arg(min.numerator).arg(min.denominator));
	w->maxLabel->setText(QString("Max FPS: %1/%2").arg(max.numerator).arg(max.denominator));
}

static void UpdateFPSLabels(PLSFrameRatePropertyWidget *w)
{
	UpdateMinMaxLabels(w);

	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(w->settings, w->name)};

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_autoselect_frames_per_second(obj.get(), &fps, nullptr) || obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	if (!valid_fps) {
		w->currentFPS->setHidden(true);
		w->timePerFrame->setHidden(true);
		if (!option)
			w->warningLabel->setStyleSheet("QLabel { color: red; }");

		return;
	}

	w->currentFPS->setHidden(false);
	w->timePerFrame->setHidden(false);

	media_frames_per_second match{};
	if (!option && !matches_ranges(match, *valid_fps, w->fps_ranges, true))
		w->warningLabel->setStyleSheet("QLabel { color: red; }");
	else
		w->warningLabel->setStyleSheet("");

	auto convert_to_fps = media_frames_per_second_to_fps;
	auto convert_to_frame_interval = media_frames_per_second_to_frame_interval;

	w->currentFPS->setText(QString("FPS: %1").arg(convert_to_fps(*valid_fps)));
	w->timePerFrame->setText(QString("Frame Interval: %1 ms").arg(convert_to_frame_interval(*valid_fps) * 1000));
}

void PLSPropertiesView::AddFrameRate(obs_property_t *prop, bool &warning, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	bool enabled = obs_property_enabled(prop);
	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	frame_rate_ranges_t fps_ranges;
	size_t num = obs_property_frame_rate_fps_ranges_count(prop);
	fps_ranges.reserve(num);
	for (size_t i = 0; i < num; i++)
		fps_ranges.emplace_back(obs_property_frame_rate_fps_range_min(prop, i), obs_property_frame_rate_fps_range_max(prop, i));

	auto widget = CreateFrameRateWidget(prop, warning, option, valid_fps, fps_ranges);
	auto info = new WidgetInfo(this, prop, widget);

	widget->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	widget->name = name;
	widget->settings = settings;

	widget->modeSelect->setEnabled(enabled);
	widget->simpleFPS->setEnabled(enabled);
	widget->fpsRange->setEnabled(enabled);
	widget->numEdit->setEnabled(enabled);
	widget->denEdit->setEnabled(enabled);

	label = widget->warningLabel = new QLabel{obs_property_description(prop)};

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, widget);

	children.emplace_back(info);

	UpdateFPSLabels(widget);

	auto stack = widget->modeDisplay;
	auto combo = widget->modeSelect;

	stack->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged);
	connect(combo, comboIndexChanged, stack, [=](int index) {
		bool out_of_bounds = index >= stack->count();
		auto idx = out_of_bounds ? stack->count() - 1 : index;
		stack->setCurrentIndex(idx);

		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
		emit info->ControlChanged();
	});

	connect(widget->simpleFPS, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	connect(widget->fpsRange, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
	});

	auto sbValueChanged = static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged);
	connect(widget->numEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	connect(widget->denEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});
}

void PLSPropertiesView::AddMobileGuider(obs_property_t *prop, QFormLayout *layout)
{
	OBSSource source = pls_get_source_by_pointer_address(obj);

	auto name = obs_property_name(prop);
	auto value = obs_property_description(prop);

	obs_data_t *privateData = obs_data_create();
	if (nullptr == value) {
		obs_source_get_private_data(source, privateData);
		value = obs_data_get_string(privateData, name);
	}

	QLabel *label = new QLabel(QT_UTF8(value));
	label->setObjectName("prismMobileLabelGuide");
	layout->addRow(nullptr, label);

	obs_data_release(privateData);
}

void PLSPropertiesView::AddMobileHelp(obs_property_t *prop, QFormLayout *layout)
{
	auto desc = obs_property_description(prop);

	auto subLayout = new QHBoxLayout();
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(5);
	subLayout->setContentsMargins(0, 0, 0, 0);

	auto label = new QLabel(QT_UTF8(desc));
	label->setObjectName("prismMobileLabelHelp");
	label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	subLayout->addWidget(label);

	auto button = new QPushButton("", this);
	button->setObjectName("prismMobileButtonHelp");
	connect(button, &QPushButton::clicked, [] { pls_show_mobile_source_help(); });
	subLayout->addWidget(button);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(subLayout);
}

QWidget *PLSPropertiesView::AddMobileName(obs_property_t *prop)
{
	OBSSource source = pls_get_source_by_pointer_address(obj);

	auto name = obs_property_name(prop);

	obs_data_t *privateData = obs_data_create();
	obs_source_get_private_data(source, privateData);
	auto value = obs_data_get_string(privateData, name);

	auto isEmpty = nullptr == value || value[0] == '\0';

	auto subWidget = new QWidget(this);
	auto subLayout = new QHBoxLayout(subWidget);
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(10);
	subLayout->setContentsMargins(0, 0, 0, 0);

	QLineEdit *edit = new QLineEdit();
	edit->setObjectName(OBJECT_NAME_LINEEDIT);
	edit->setText(QT_UTF8(value));
	//edit->setPlaceholderText(QT_UTF8(obs_property_placeholder(prop)));
	edit->setEnabled(false);
	edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	if (edit->text().isEmpty()) {
		edit->setText(QT_UTF8(obs_property_placeholder(prop)));
		edit->setStyleSheet("padding-left: /*hdpi*/ 12px; color: #666666;");
	} else {
		edit->setStyleSheet("padding-left: /*hdpi*/ 12px; color: white;");
	}
	subLayout->addWidget(edit);

	QPushButton *button = new QPushButton(QT_UTF8(obs_property_text_button_text(prop)), this);
	button->setEnabled(!isEmpty);
	PLSDpiHelper().setFixedSize(button, {128, 40});
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, button);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	obs_data_release(privateData);

	return subWidget;
}

QWidget *PLSPropertiesView::AddMobileStatus(obs_property_t *)
{
	OBSSource source = pls_get_source_by_pointer_address(obj);

	obs_data_t *privateData = obs_data_create();
	obs_source_get_private_data(source, privateData);
	string statusImage = obs_data_get_string(privateData, "statusImage");
	auto statusText = obs_data_get_string(privateData, "statusText");

	auto subWidget = new QWidget(this);
	auto subLayout = new QHBoxLayout(subWidget);
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(5);
	subLayout->setContentsMargins(0, 0, 0, 0);

	if (!statusImage.empty()) {
		auto label = new QLabel();
		auto setPixmap = [=] { label->setPixmap(pls_load_svg(QString::fromStdString(statusImage), PLSDpiHelper::calculate(label, QSize(18, 18)))); };
		setPixmap();
		subLayout->addWidget(label);
		PLSDpiHelper().notifyDpiChanged(label, [=] { setPixmap(); });
	}
	if (statusText != nullptr && *statusText) {
		auto label = new QLabel(QT_UTF8(statusText));
		label->setObjectName("prismMobileLabelStatus");
		subLayout->addWidget(label);
	}

	obs_data_release(privateData);

	return subWidget;
}

QWidget *PLSPropertiesView::AddPrivateDataText(obs_property_t *, QFormLayout *, QLabel *&)
{
	OBSSource source = pls_get_source_by_pointer_address(obj);

	obs_data_t *privateData = obs_data_create();
	obs_source_get_private_data(source, privateData);
	auto resolution = obs_data_get_string(privateData, "resolution");

	QLineEdit *edit = new QLineEdit();
	edit->setObjectName(OBJECT_NAME_LINEEDIT);
	edit->setText(QT_UTF8(resolution));
	edit->setStyleSheet("padding-left: /*hdpi*/ 12px");

	obs_data_release(privateData);

	return edit;
}

void PLSPropertiesView::AddGroup(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);
	const char *desc = obs_property_description(prop);
	enum obs_group_type type = obs_property_group_type(prop);

	// Create GroupBox
	QGroupBox *groupBox = new QGroupBox(QT_UTF8(desc));
	groupBox->setCheckable(type == OBS_GROUP_CHECKABLE);
	groupBox->setChecked(groupBox->isCheckable() ? val : true);
	groupBox->setAccessibleName("group");
	groupBox->setEnabled(obs_property_enabled(prop));

	// Create Layout and build content
	QFormLayout *subLayout = new QFormLayout();
	subLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	groupBox->setLayout(subLayout);

	obs_properties_t *content = obs_property_group_content(prop);
	obs_property_t *el = obs_properties_first(content);
	while (el != nullptr) {
		AddProperty(el, subLayout);
		obs_property_next(&el);
	}

	// Insert into UI
	layout->setWidget(layout->rowCount(), QFormLayout::ItemRole::SpanningRole, groupBox);
	AddSpacer(obs_property_get_type(prop), layout);

	// Register Group Widget
	WidgetInfo *info = new WidgetInfo(this, prop, groupBox);
	children.emplace_back(info);

	// Signals
	connect(groupBox, SIGNAL(toggled(bool)), info, SLOT(ControlChanged()));
}

void PLSPropertiesView::AddProperty(obs_property_t *property, QLayout *layout)
{
	const char *name = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);

	if (!obs_property_visible(property))
		return;

	QFormLayout *formLayout = dynamic_cast<QFormLayout *>(layout);
	QBoxLayout *boxLayout = dynamic_cast<QBoxLayout *>(layout);

	QLabel *label = nullptr;
	QWidget *widget = nullptr;
	bool warning = false;
	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL:
		widget = AddCheckbox(property, formLayout);
		break;
	case OBS_PROPERTY_SWITCH:
		widget = AddSwitch(property, formLayout);
		break;
	case OBS_PROPERTY_BOOL_GROUP:
		AddRadioButtonGroup(property, formLayout);
		break;
	case OBS_PROPERTY_INT:
		AddInt(property, formLayout, &label);
		break;
	case OBS_PROPERTY_FLOAT:
		AddFloat(property, formLayout, &label);
		break;
	case OBS_PROPERTY_TEXT:
		widget = AddText(property, formLayout, label);
		break;
	case OBS_PROPERTY_PATH: {
		if (!showColorFilterPath) {
			showColorFilterPath = true;
			return;
		}
		AddPath(property, formLayout, &label);
		break;
	}

	case OBS_PROPERTY_LIST:
		widget = AddList(property, warning);
		break;
	case OBS_PROPERTY_BGM_MUSIC_LIST:
		AddMusicList(property, formLayout);
		break;
	case OBS_PROPERTY_REGION_SELECT:
		widget = AddSelectRegion(property, warning);
		break;
	case OBS_PROPERTY_TIPS:
		AddTips(property, formLayout);
		break;
	case OBS_PROPERTY_COLOR:
		AddColor(property, formLayout, label);
		break;
	case OBS_PROPERTY_FONT:
		AddFont(property, formLayout, label);
		break;
	case OBS_PROPERTY_BUTTON:
		widget = AddButton(property, formLayout);
		break;
	case OBS_PROPERTY_BUTTON_GROUP:
		AddButtonGroup(property, formLayout);
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		AddEditableList(property, formLayout, label);
		break;
	case OBS_PROPERTY_FRAME_RATE:
		AddFrameRate(property, warning, formLayout, label);
		break;
	case OBS_PROPERTY_GROUP:
		AddGroup(property, formLayout);
		break;
	case OBS_PROPERTY_CHAT_TEMPLATE_LIST:
		AddChatTemplateList(property, formLayout);
		break;
	case OBS_PROPERTY_CHAT_FONT_SIZE:
		AddChatFontSize(property, formLayout);
		break;
	case OBS_PROPERTY_TM_TEXT_CONTENT:
		AddTmTextContent(property, formLayout);
		break;
	case OBS_PROPERTY_TM_TAB:
		AddTmTab(property, formLayout);
		break;
	case OBS_PROPERTY_TM_TEMPLATE_TAB:
		AddTmTemplateTab(property, formLayout);
		break;
	case OBS_PROPERTY_TM_TEMPLATE_LIST:
		AddTmTabTemplateList(property, formLayout);
		break;
	case OBS_PROPERTY_TM_TEXT:
		AddTmText(property, formLayout, label);
		break;
	case OBS_PROPERTY_TM_COLOR:
		AddTmColor(property, formLayout, label);
		break;
	case OBS_PROPERTY_TM_MOTION:
		AddTmMotion(property, formLayout, label);
		break;
	case OBS_PROPERTY_CAMERA_VIRTUAL_BACKGROUND_STATE:
		AddCameraVirtualBackgroundState(property, formLayout, label);
		break;
	case OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE:
		AddVirtualBackgroundResource(property, boxLayout);
		break;
	case OBS_PROPERTY_IMAGE_GROUP:
		AddImageGroup(property, formLayout, label);
		break;
	case OBS_PROPERTY_CUSTOM_GROUP:
		AddCustomGroup(property, formLayout, label);
		break;
	case OBS_PROPERTY_H_LINE:
		AddHLine(property, formLayout, label);
		break;
	case OBS_PROPERTY_BOOL_LEFT:
		widget = AddCheckbox(property, formLayout, Qt::RightToLeft);
		break;
	case OBS_PROPERTY_MOBILE_GUIDER:
		AddMobileGuider(property, formLayout);
		break;
	case OBS_PROPERTY_MOBILE_HELP:
		AddMobileHelp(property, formLayout);
		break;
	case OBS_PROPERTY_MOBILE_NAME:
		widget = AddMobileName(property);
		break;
	case OBS_PROPERTY_MOBILE_STATUS:
		widget = AddMobileStatus(property);
		break;
	case OBS_PROPERTY_PRIVATE_DATA_TEXT:
		widget = AddPrivateDataText(property, formLayout, label);
		break;
	case OBS_PROPERTY_CHECKBOX_GROUP:
		AddCheckboxGroup(property, formLayout);
		break;
	case OBS_PROPERTY_INT_GROUP:
		AddIntGroup(property, formLayout, label);
		break;
	case OBS_PROPERTY_FONT_SIMPLE:
		AddFontSimple(property, formLayout, label);
		break;
	case OBS_PROPERTY_COLOR_CHECKBOX:
		AddColorCheckbox(property, formLayout, label);
		break;
	case OBS_PROPERTY_TIMER_LIST_LISTEN:
		AddTimerListListen(property, formLayout, label);
		break;
	case OBS_PROPERTY_LABEL_TIP:
		AddLabelTip(property, formLayout);
		break;
	}

	if (widget && !obs_property_enabled(property))
		widget->setEnabled(false);

	if (!label && type != OBS_PROPERTY_BOOL && type != OBS_PROPERTY_BUTTON && type != OBS_PROPERTY_BUTTON_GROUP && type != OBS_PROPERTY_GROUP && type != OBS_PROPERTY_CHAT_TEMPLATE_LIST &&
	    type != OBS_PROPERTY_CHAT_FONT_SIZE && type != OBS_PROPERTY_TM_TAB && type != OBS_PROPERTY_TM_TEMPLATE_LIST && type != OBS_PROPERTY_TM_TEXT_CONTENT && type != OBS_PROPERTY_BOOL_LEFT &&
	    type != OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE && type != OBS_PROPERTY_MOBILE_HELP && type != OBS_PROPERTY_H_LINE && type != OBS_PROPERTY_INT_GROUP)
		label = new QLabel(QT_UTF8(obs_property_description(property)));

	if (warning && label) //TODO: select color based on background color
		label->setStyleSheet("QLabel { color: red; }");

	if (label) {
		PLSDpiHelper dpiHelper;
		if (minSize > 0) {
			dpiHelper.setMinimumWidth(label, minSize);
		}
		if (maxSize > 0) {
			dpiHelper.setMaximumWidth(label, maxSize);
		}
		label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		label->setWordWrap(true);
	}

	bool child = PROPERTY_FLAG_CHILD_CONTROL & obs_property_get_flags(property);

	if (label && !obs_property_enabled(property))
		label->setEnabled(false);
	if (label) {
		if (!child)
			label->setObjectName(OBJECT_NAME_FORMLABEL);
		else
			label->setObjectName("subLabel");
	}

	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (label && source) {
		const char *id = obs_source_get_id(source);
		if (id && !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) && !child) {
			label->setObjectName("titleLabel");
		}
	}

	if (type == OBS_PROPERTY_TM_TEXT || type == OBS_PROPERTY_TM_COLOR || type == OBS_PROPERTY_TM_MOTION) {
		label->setObjectName("tmLabel");
	}

	if (label && (PROPERTY_FLAG_NO_LABEL_HEADER & obs_data_get_flags(settings)) || (PROPERTY_FLAG_NO_LABEL_SINGLE & obs_property_get_flags(property))) {
		label->deleteLater();
	}

	if (!widget || !formLayout)
		return;

	if (!lastFocused.empty())
		if (lastFocused.compare(name) == 0)
			lastWidget = widget;

	// set different vertical distance between different source type
	AddSpacer(type, formLayout);

	if ((PROPERTY_FLAG_NO_LABEL_HEADER & obs_data_get_flags(settings)) || (PROPERTY_FLAG_NO_LABEL_SINGLE & obs_property_get_flags(property))) {
		formLayout->addRow(widget);
	} else {
		formLayout->addRow(label, widget);
	}
}

static bool isSamePropertyType(obs_property_type a, obs_property_type b)
{
	switch (a) {
	case OBS_PROPERTY_BOOL:
		switch (b) {
		case OBS_PROPERTY_BOOL_LEFT:
		case OBS_PROPERTY_BOOL:
			return true;
		default:
			return false;
		}
		break;
	case OBS_PROPERTY_INT:
	case OBS_PROPERTY_FLOAT:
	case OBS_PROPERTY_TEXT:
	case OBS_PROPERTY_PATH:
	case OBS_PROPERTY_LIST:
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_BUTTON:
	case OBS_PROPERTY_FONT:
	case OBS_PROPERTY_EDITABLE_LIST:
	case OBS_PROPERTY_FRAME_RATE:
	case OBS_PROPERTY_FONT_SIMPLE:
	case OBS_PROPERTY_COLOR_CHECKBOX:
	case OBS_PROPERTY_IMAGE_GROUP:
		switch (b) {
		case OBS_PROPERTY_INT:
		case OBS_PROPERTY_FLOAT:
		case OBS_PROPERTY_TEXT:
		case OBS_PROPERTY_PATH:
		case OBS_PROPERTY_LIST:
		case OBS_PROPERTY_COLOR:
		case OBS_PROPERTY_BUTTON:
		case OBS_PROPERTY_FONT:
		case OBS_PROPERTY_EDITABLE_LIST:
		case OBS_PROPERTY_FRAME_RATE:
		case OBS_PROPERTY_IMAGE_GROUP:
		case OBS_PROPERTY_FONT_SIMPLE:
		case OBS_PROPERTY_COLOR_CHECKBOX:
			return true;
		default:
			return false;
		}
	case OBS_PROPERTY_CHAT_TEMPLATE_LIST:
	case OBS_PROPERTY_CHAT_FONT_SIZE:
	case OBS_PROPERTY_TM_TEXT_CONTENT:
	case OBS_PROPERTY_TM_TAB:
	case OBS_PROPERTY_TM_TEMPLATE_TAB:
	case OBS_PROPERTY_TM_TEMPLATE_LIST:
	case OBS_PROPERTY_TM_TEXT:
	case OBS_PROPERTY_TM_COLOR:
	case OBS_PROPERTY_TM_MOTION:
	case OBS_PROPERTY_CUSTOM_GROUP:
	case OBS_PROPERTY_H_LINE:
	case OBS_PROPERTY_CAMERA_VIRTUAL_BACKGROUND_STATE:
	case OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE:
		return false;
	case OBS_PROPERTY_INT_GROUP:
	case OBS_PROPERTY_TIMER_LIST_LISTEN:
		return true;
	default:
		break;
	}
	return true;
}

void PLSPropertiesView::AddSpacer(const obs_property_type &currentType, QFormLayout *layout)
{
	if (lastPropertyType != OBS_PROPERTY_INVALID) {
		if (OBS_PROPERTY_H_LINE == currentType || OBS_PROPERTY_H_LINE == lastPropertyType) {
			layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
		} else if (OBS_PROPERTY_MOBILE_NAME == currentType) {
			layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
		} else {
			if (isSamePropertyType(lastPropertyType, currentType)) {
				layout->addItem(new QSpacerItem(10, PROPERTIES_VIEW_VERTICAL_SPACING_MIN, QSizePolicy::Fixed, QSizePolicy::Fixed));
			} else {
				layout->addItem(new QSpacerItem(10, PROPERTIES_VIEW_VERTICAL_SPACING_MAX, QSizePolicy::Fixed, QSizePolicy::Fixed));
			}
		}
	}
	lastPropertyType = currentType;
}

class ChatTemplate : public QPushButton {
public:
	ChatTemplate(QButtonGroup *buttonGroup, int id, bool checked)
	{
		setObjectName("chatTemplateList_template");
		setProperty("lang", pls_get_current_language());
		setProperty("style", id);
		setCheckable(true);
		setChecked(checked);
		buttonGroup->addButton(this, id);

		QVBoxLayout *vlayout = new QVBoxLayout(this);
		vlayout->setMargin(0);
		vlayout->setSpacing(0);

		QLabel *icon = new QLabel(this);
		icon->setObjectName("icon");
		icon->setScaledContents(true);
		vlayout->addWidget(icon, 1);

		QLabel *text = new QLabel(this);
		text->setObjectName("text");
		text->setText(QString("Style %1").arg(id));
		vlayout->addWidget(text);
	}
	~ChatTemplate() {}

protected:
	virtual bool event(QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::HoverEnter:
			pls_flush_style_recursive(this, "hovered", true);
			break;
		case QEvent::HoverLeave:
			pls_flush_style_recursive(this, "hovered", false);
			break;
		case QEvent::MouseButtonPress:
			pls_flush_style_recursive(this, "pressed", true);
			break;
		case QEvent::MouseButtonRelease:
			pls_flush_style_recursive(this, "pressed", false);
			break;
		default:
			break;
		}
		return QPushButton::event(event);
	}
};

void PLSPropertiesView::AddChatTemplateList(obs_property_t *prop, QFormLayout *layout)
{
	QWidget *widget = new QWidget();
	widget->setObjectName("chatTemplateList");

	QVBoxLayout *vlayout = new QVBoxLayout(widget);
	vlayout->setMargin(0);
	vlayout->setSpacing(15);

	QHBoxLayout *hlayout1 = new QHBoxLayout();
	hlayout1->setMargin(0);
	hlayout1->setSpacing(10);

	const char *desc = obs_property_description(prop);
	QLabel *nameLabel = new QLabel(QString::fromUtf8(desc && desc[0] ? desc : ""));
	nameLabel->setObjectName("chatTemplateList_nameLabel");

	hlayout1->addWidget(nameLabel);
	if (!obs_frontend_streaming_active()) {
		const char *longDesc = obs_property_long_description(prop);
		QLabel *descLabel = new QLabel(QString::fromUtf8(longDesc && longDesc[0] ? longDesc : ""));
		descLabel->setObjectName("chatTemplateList_descLabel");
		hlayout1->addWidget(descLabel, 1);
	}

	QHBoxLayout *hlayout2 = new QHBoxLayout();
	hlayout2->setMargin(0);
	hlayout2->setSpacing(10);

	const char *name = obs_property_name(prop);
	int val = (int)obs_data_get_int(settings, name);

	QButtonGroup *buttonGroup = new QButtonGroup(hlayout2);
	buttonGroup->setExclusive(true);
	buttonGroup->setProperty("selected", val);

	for (int i = 1; i <= 4; ++i) {
		QPushButton *button = new ChatTemplate(buttonGroup, i, val == i);
		hlayout2->addWidget(button);
	}

	vlayout->addLayout(hlayout1);
	vlayout->addLayout(hlayout2, 1);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(widget);

	WidgetInfo *wi = new WidgetInfo(this, prop, buttonGroup);
	children.emplace_back(wi);

	void (QButtonGroup::*buttonClicked)(int) = &QButtonGroup::buttonClicked;
	connect(buttonGroup, buttonClicked, wi, [=](int index) {
		int previous = buttonGroup->property("selected").toInt();
		if (previous != index) {
			buttonGroup->setProperty("selected", index);
			wi->ControlChanged();
			PLS_UI_STEP(PROPERTY_MODULE, QString::asprintf("property-window: chat theme button %d", index).toUtf8().constData(), ACTION_CLICK);

			pls_flush_style_recursive(buttonGroup->button(previous));
			pls_flush_style_recursive(buttonGroup->button(index));
		}
	});
}

void PLSPropertiesView::AddChatFontSize(obs_property_t *prop, QFormLayout *layout)
{
	QWidget *widget = new QWidget();
	widget->setObjectName("chatFontSize");

	QHBoxLayout *hlayout1 = new QHBoxLayout(widget);
	hlayout1->setMargin(0);
	hlayout1->setSpacing(20);

	QLabel *label = new QLabel(QString::fromUtf8(obs_property_description(prop)));
	label->setObjectName("label");

	QHBoxLayout *hlayout2 = new QHBoxLayout();
	hlayout2->setMargin(0);
	hlayout2->setSpacing(10);

	int minVal = obs_property_chat_font_size_min(prop);
	int maxVal = obs_property_chat_font_size_max(prop);
	int stepVal = obs_property_chat_font_size_step(prop);

	const char *name = obs_property_name(prop);
	int val = (int)obs_data_get_int(settings, name);

	SliderIgnoreScroll *slider = new SliderIgnoreScroll();
	slider->setObjectName("slider");
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);

	PLSSpinBox *spinBox = new PLSSpinBox();
	spinBox->setObjectName("spinBox");
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);

	hlayout2->addWidget(slider, 1);
	hlayout2->addWidget(spinBox);

	hlayout1->addWidget(label);
	hlayout1->addLayout(hlayout2, 1);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(widget);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

	WidgetInfo *wi = new WidgetInfo(this, prop, spinBox);
	connect(spinBox, SIGNAL(valueChanged(int)), wi, SLOT(ControlChanged()));
	children.emplace_back(wi);
}

void PLSPropertiesView::AddTmText(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	label = new QLabel(QString::fromUtf8(obs_property_description(prop)));
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	QFormLayout *flayout = new QFormLayout();
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);

	QHBoxLayout *hlayout1 = new QHBoxLayout();
	hlayout1->setMargin(0);
	hlayout1->setSpacing(10);

	PLSComboBox *fontCbx = new PLSComboBox();
	fontCbx->setObjectName("tmFontBox");
	QString family = obs_data_get_string(val, "font-family");
	fontCbx->addItem(family);
	fontCbx->setCurrentText(family);
	PLSComboBox *weightCbx = new PLSComboBox();
	updateFontSytle(family, weightCbx);
	weightCbx->setCurrentText(obs_data_get_string(val, "font-weight"));

	weightCbx->setObjectName("tmFontStyleBox");
	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), [this, weightCbx](const QString &text) { updateFontSytle(text, weightCbx); });

	QMetaObject::invokeMethod(
		fontCbx,
		[family, fontCbx, this]() {
			QSignalBlocker block(fontCbx);
			fontCbx->clear();
			fontCbx->addItems(m_fontDatabase.families());
			fontCbx->setCurrentText(family);
		},
		Qt::QueuedConnection);
	hlayout1->addWidget(fontCbx);
	hlayout1->addWidget(weightCbx);
	hlayout1->setStretch(0, 292);
	hlayout1->setStretch(1, 210);

	QLabel *fontLabel = new QLabel(QTStr("textmotion.font"));
	fontLabel->setObjectName("subLabel");
	flayout->addRow(fontLabel, hlayout1);
	m_tmLabels.append(fontLabel);

	WidgetInfo *fontWidgetInfo = new WidgetInfo(this, prop, fontCbx);
	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), fontWidgetInfo, &WidgetInfo::ControlChanged);
	children.emplace_back(fontWidgetInfo);

	WidgetInfo *fontStyleWidgetInfo = new WidgetInfo(this, prop, weightCbx);
	connect(weightCbx, QOverload<const QString &>::of(&QComboBox::currentIndexChanged), fontStyleWidgetInfo, &WidgetInfo::ControlChanged);
	children.emplace_back(fontStyleWidgetInfo);

	QHBoxLayout *hlayout3 = new QHBoxLayout();
	hlayout3->setMargin(0);
	hlayout3->setSpacing(20);

	int minVal = obs_property_tm_text_min(prop, OBS_PROPERTY_TM_TEXT);
	int maxVal = obs_property_tm_text_max(prop, OBS_PROPERTY_TM_TEXT);
	int stepVal = obs_property_tm_text_step(prop, OBS_PROPERTY_TM_TEXT);
	int fontSize = (int)obs_data_get_int(val, "font-size");

	createTMSlider(prop, 0, minVal, maxVal, stepVal, fontSize, hlayout3, false, false, false);
	QLabel *fontSizeLabel = new QLabel(QTStr("textmotion.size"));
	fontSizeLabel->setObjectName("subLabel");
	flayout->addRow(fontSizeLabel, hlayout3);
	m_tmLabels.append(fontSizeLabel);

	QHBoxLayout *boxSizeHLayout = new QHBoxLayout();
	boxSizeHLayout->setSpacing(16);
	boxSizeHLayout->setMargin(0);

	int minWidthVal = 0;
	int maxWidthVal = 5000;
	int widthStepVal = 1;
	int widthSize = obs_data_get_int(settings, "width");
	createTMSlider(prop, 1, minWidthVal, maxWidthVal, widthStepVal, widthSize, boxSizeHLayout, false, false, false, QTStr("textmotion.text.box.width"));

	int minHeightVal = 0;
	int maxHeightVal = 5000;
	int heightStepVal = 1;
	int heightSize = obs_data_get_int(settings, "height");
	createTMSlider(prop, 2, minHeightVal, maxHeightVal, heightStepVal, heightSize, boxSizeHLayout, false, false, false, QTStr("textmotion.text.box.height"));

	QLabel *boxSizeLabel = new QLabel(QTStr("textmotion.text.box.size"));
	boxSizeLabel->setObjectName("subLabel");
	flayout->addRow(boxSizeLabel, boxSizeHLayout);
	m_tmLabels.append(boxSizeLabel);

	QHBoxLayout *textAlignmentLayout = new QHBoxLayout;
	textAlignmentLayout->setSpacing(10);

	QHBoxLayout *hlayout2 = new QHBoxLayout();
	hlayout2->setMargin(0);
	hlayout2->setSpacing(1);

	QButtonGroup *group = new QButtonGroup;
	group->setObjectName("group");
	createTMButton(3, val, hlayout2, group, ButtonType::CustomButton, {"TMHLButton", "TMHCButton", "TMHRButton"});

	bool isAlign = obs_data_get_bool(val, "is-h-aligin");
	int hAlignIndex = obs_data_get_int(val, "h-aligin");

	QAbstractButton *alignBtn = group->button(hAlignIndex);
	if (!alignBtn) {
		return;
	}
	group->button(hAlignIndex)->setChecked(true);
	if (isAlign) {
		group->button(hAlignIndex)->toggled(true);
	} else {
		for (auto btn : group->buttons()) {
			btn->setEnabled(isAlign);
		}
	}
	textAlignmentLayout->addLayout(hlayout2);

	WidgetInfo *alignWidgetInfo = new WidgetInfo(this, prop, group);
	connect(group, QOverload<int>::of(&QButtonGroup::buttonClicked), alignWidgetInfo, &WidgetInfo::ControlChanged);
	children.emplace_back(alignWidgetInfo);

	QHBoxLayout *hlayout4 = new QHBoxLayout();
	hlayout4->setMargin(0);
	hlayout4->setSpacing(1);
	QButtonGroup *group2 = new QButtonGroup;
	group2->setObjectName("group2");
	group2->setExclusive(false);
	createTMButton(2, val, hlayout4, group2, ButtonType::LetterButton, {"AA", "aa"}, false, false);

	int letterIndex = obs_data_get_int(val, "letter");
	QAbstractButton *button = group2->button(letterIndex);
	if (letterIndex != -1 && button) {
		button->clicked();
	}

	textAlignmentLayout->addLayout(hlayout4);
	textAlignmentLayout->setStretch(0, 292);
	textAlignmentLayout->setStretch(1, 210);

	QLabel *textAlignmentLabel = new QLabel(QTStr("textmotion.align.transform"));
	textAlignmentLabel->setObjectName("subLabel");
	flayout->addRow(textAlignmentLabel, textAlignmentLayout);
	m_tmLabels.append(textAlignmentLabel);

	WidgetInfo *letterWidgetInfo = new WidgetInfo(this, prop, group2);
	connect(group2, QOverload<int>::of(&QButtonGroup::buttonClicked), [letterWidgetInfo, this, group2](int index) {
		QAbstractButton *btn = group2->button(index);
		for (auto btnIndex : group2->buttons()) {
			bool isChecked = btnIndex->isChecked();
			if (btnIndex != btn && isChecked) {
				btnIndex->clicked();
			}
		}
		letterWidgetInfo->ControlChanged();
	});
	children.emplace_back(letterWidgetInfo);

	QWidget *w = new QWidget;
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	hlayout1->setEnabled(obs_data_get_bool(val, "is-font"));
	hlayout2->setEnabled(obs_data_get_bool(val, "is-h-aligin"));
	hlayout3->setEnabled(obs_data_get_bool(val, "is-font-size"));

	obs_data_release(val);
}

void PLSPropertiesView::AddTmTextContent(obs_property_t *prop, QFormLayout *layout)
{
	// = new QLabel(QString::fromUtf8(obs_property_description(prop)));
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	int textCount = obs_data_get_int(val, "text-count");

	QHBoxLayout *hLayout = new QHBoxLayout();
	hLayout->setMargin(0);
	hLayout->setSpacing(6);

	creatTMTextWidget(prop, textCount, val, hLayout);

	layout->addRow(hLayout);
	layout->addItem(new QSpacerItem(10, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
	obs_data_release(val);
}

void PLSPropertiesView::AddTmTab(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	int tabIndex = obs_data_get_int(settings, name);
	QFrame *tabFrame = new QFrame;
	tabFrame->setObjectName("TMTabFrame");
	QHBoxLayout *hLayout = new QHBoxLayout();
	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(0);
	tabFrame->setLayout(hLayout);

	bool audioVisualizer = false;
	bool isTimerSource = false;
	OBSSource source = pls_get_source_by_pointer_address(obj);
	if (source) {
		const char *id = obs_source_get_id(source);
		if (id && id[0]) {
			audioVisualizer = !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID);
			isTimerSource = !strcmp(id, PRISM_TIMER_SOURCE_ID);
		}
	}
	if (isTimerSource) {
		tabFrame->setProperty("height_timer", true);
	}

	QStringList tabList = {QTStr("textmotion.select.template"), QTStr("textmotion.detailed.settings")};
	QButtonGroup *buttonGroup = new QButtonGroup;
	for (int index = 0; index != tabList.count(); ++index) {
		QPushButton *button = new QPushButton;
		button->setObjectName("TMTabButton");
		buttonGroup->addButton(button, index);
		button->setAutoExclusive(true);
		button->setCheckable(true);
		button->setText(tabList.value(index));
		button->setChecked(index == tabIndex);
		hLayout->addWidget(button);
	}
	QAbstractButton *button = buttonGroup->button(tabIndex);
	if (!button) {
		return;
	}
	button->setChecked(true);

	WidgetInfo *wi = new WidgetInfo(this, prop, buttonGroup);
	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [wi, this]() {
		wi->ControlChanged();
		m_tmTabChanged = true;
	});
	children.emplace_back(wi);

	if (audioVisualizer) {
		layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed));
	} else if (isTimerSource) {
		layout->addItem(new QSpacerItem(10, 6, QSizePolicy::Fixed, QSizePolicy::Fixed));
		lastPropertyType = obs_property_get_type(prop);
	} else {
		AddSpacer(obs_property_get_type(prop), layout);
	}

	layout->addRow(tabFrame);
	if (audioVisualizer)
		tabIndex ? layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed)) : layout->addItem(new QSpacerItem(10, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
}

void PLSPropertiesView ::AddTmTemplateTab(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	int tabIndex = obs_data_get_int(settings, name);
	obs_property_t *propNext = prop;
	bool isNext = obs_property_next(&propNext);
	const char *nextName = nullptr;
	int templateTabIndex = -1;
	if (isNext) {
		nextName = obs_property_name(propNext);
		templateTabIndex = obs_data_get_int(settings, nextName);
	}
	if (tabIndex < 0 || templateTabIndex < 0) {
		return;
	}
	QVBoxLayout *vLayout = new QVBoxLayout();
	vLayout->setContentsMargins(0, 15, 0, 0);
	vLayout->setSpacing(15);

	QFrame *tabFrame = new QFrame;
	tabFrame->setObjectName("TMTabTemplistFrame");
	QHBoxLayout *hLayout = new QHBoxLayout();
	hLayout->setSpacing(0);
	hLayout->setMargin(0);
	hLayout->setContentsMargins(31, 0, 31, 0);
	tabFrame->setLayout(hLayout);

	QGridLayout *gLayout = new QGridLayout();
	gLayout->setSpacing(12);
	gLayout->setAlignment(Qt::AlignLeft);
	QButtonGroup *buttonGroup = new QButtonGroup;
	QStringList templateList = {"TITLE", "SOCIAL", "CAPTION", "ELEMENT"};
	int templateCount = templateList.count();
	QString selectTemplateStr = m_tmHelper->findTemplateGroupStr(templateTabIndex);
	for (int index = 0; index != templateCount; ++index) {
		QPushButton *button = new QPushButton;
		button->setObjectName("TMTabTemplistBtn");
		buttonGroup->addButton(button, index);
		button->setAutoExclusive(true);
		button->setCheckable(true);
		button->setText(templateList.value(index).toUpper());
		if (m_tmTemplateChanged) {
			button->setChecked(index == tabIndex);
		} else {
			button->setChecked(0 == templateList.value(index).compare(selectTemplateStr, Qt::CaseInsensitive));
		}
		hLayout->addWidget(button);
		if (index != templateCount - 1) {
			hLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Fixed));
		}
	}
	if (m_tmTemplateChanged) {
		QString templateName = templateList.value(tabIndex);
		updateTMTemplateButtons(tabIndex, templateName.toLower(), gLayout);
	} else {
		updateTMTemplateButtons(tabIndex, selectTemplateStr.toLower(), gLayout);
	}

	WidgetInfo *wi = new WidgetInfo(this, prop, buttonGroup);
	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), wi, &WidgetInfo::ControlChanged);
	children.emplace_back(wi);

	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), [this, buttonGroup, gLayout](int index) {
		if (index >= 0) {
			updateTMTemplateButtons(index, buttonGroup->button(index)->text().toLower(), gLayout);
			m_tmTemplateChanged = true;
		}
	});

	vLayout->addWidget(tabFrame, 0);
	vLayout->addLayout(gLayout, 1);
	layout->addRow(vLayout);
}
void PLSPropertiesView::AddTmTabTemplateList(obs_property_t *prop, QFormLayout *)
{
	const char *name = obs_property_name(prop);
	int tabIndex = obs_data_get_int(settings, name);
	if (tabIndex < 0) {
		return;
	}
	QString selectTemplateStr = m_tmHelper->findTemplateGroupStr(tabIndex);
	QButtonGroup *buttonGroup = m_tmHelper->getTemplateButtons(selectTemplateStr);

	if (buttonGroup) {
		m_tmHelper->resetButtonStyle();
		QAbstractButton *button = buttonGroup->button(tabIndex);
		if (button) {
			button->setChecked(true);
		}
	}
	QMap<int, QString> templateNames = m_tmHelper->getTemplateNames();
	for (auto templateName : templateNames.keys()) {
		auto templateTabGroup = m_tmHelper->getTemplateButtons(templateNames.value(templateName).toLower());
		if (templateTabGroup) {
			WidgetInfo *wi = new WidgetInfo(this, prop, templateTabGroup);
			connect(templateTabGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), wi, &WidgetInfo::ControlChanged);
			children.emplace_back(wi);
		}
	}
}

void PLSPropertiesView::AddTmColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	label = new QLabel(QString::fromUtf8(obs_property_description(prop)));
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	QGridLayout *glayout = new QGridLayout;
	glayout->setHorizontalSpacing(20);
	glayout->setVerticalSpacing(10);

	QHBoxLayout *textColorLayout = new QHBoxLayout;
	textColorLayout->setMargin(0);
	textColorLayout->setSpacing(20);
	QLabel *textColorLabel = new QLabel(QTStr("textmotion.text"));
	textColorLabel->setObjectName("subLabel");
	glayout->addWidget(textColorLabel, 0, 0);
	m_tmLabels.append(textColorLabel);

	QCheckBox *ChecBox = nullptr;
	createColorButton(prop, glayout, ChecBox, QTStr("textmotion.opacity"), 0, true, true);

	//flayout->addRow(textColorLabel, textColorLayout);
	QCheckBox *bkControlChecBox = nullptr;
	QHBoxLayout *bkColorLayout = new QHBoxLayout;
	bkColorLayout->setMargin(0);
	bkColorLayout->setSpacing(20);
	QFrame *bkColorLabelFrame = new QFrame;
	bkColorLabelFrame->setObjectName("colorFrame");
	bool isEnable = obs_data_get_bool(val, "is-bk-color-on");
	bool isChecked = obs_data_get_bool(val, "is-bk-color");
	bool isInitChecked = obs_data_get_bool(val, "is-bk-init-color");
	if (isEnable) {
		if (isInitChecked) {
			isEnable = false;
		} else {
			isEnable = true;
		}
	}
	createTMColorCheckBox(bkControlChecBox, prop, bkColorLabelFrame, 1, QTStr("textmotion.background"), bkColorLayout, isChecked, isEnable);

	glayout->addWidget(bkColorLabelFrame, 1, 0);
	createColorButton(prop, glayout, bkControlChecBox, QTStr("textmotion.opacity"), 1, true, true);

	//flayout->addRow(bkColorLabelFrame, bkColorLayout);

	QCheckBox *outlineControlCheckBox = nullptr;
	QHBoxLayout *outLineLayout = new QHBoxLayout;
	outLineLayout->setMargin(0);
	outLineLayout->setSpacing(20);
	QFrame *outLineLabelFrame = new QFrame;
	bkColorLabelFrame->setObjectName("outLineFrame");
	createTMColorCheckBox(outlineControlCheckBox, prop, outLineLabelFrame, 2, QTStr("textmotion.outline"), outLineLayout, obs_data_get_bool(val, "is-outline-color"),
			      obs_data_get_bool(val, "is-outline-color-on"));

	glayout->addWidget(outLineLabelFrame, 2, 0);
	createColorButton(prop, glayout, outlineControlCheckBox, QTStr("textmotion.thickness"), 2, false, true);

	QWidget *w = new QWidget;
	w->setObjectName("horiLine");
	glayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), glayout->rowCount(), 0);
	glayout->addWidget(w, glayout->rowCount(), 0, 1, glayout->columnCount());

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(glayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddTmMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	label = new QLabel(QString::fromUtf8(obs_property_description(prop)));
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	QFormLayout *flayout = new QFormLayout;
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);
	QHBoxLayout *hlayout = new QHBoxLayout();
	hlayout->setAlignment(Qt::AlignLeft);
	hlayout->setMargin(0);
	hlayout->setSpacing(20);
	QButtonGroup *group2 = new QButtonGroup;
	group2->setObjectName("group2");

	createTMButton(2, val, hlayout, group2, ButtonType::RadioButon, {QTStr("textmotion.repeat.motion.on"), QTStr("textmotion.retpeat.motion.off")}, true);

	QLabel *repeatLabel = new QLabel(QTStr("textmotion.repeat.off"));
	repeatLabel->setObjectName("subLabel");
	flayout->addRow(repeatLabel, hlayout);
	m_tmLabels.append(repeatLabel);

	QHBoxLayout *hlayout2 = new QHBoxLayout();
	hlayout2->setMargin(0);
	hlayout2->setSpacing(20);

	int minVal = obs_property_tm_text_min(prop, OBS_PROPERTY_TM_MOTION);
	int maxVal = obs_property_tm_text_max(prop, OBS_PROPERTY_TM_MOTION);
	int stepVal = obs_property_tm_text_step(prop, OBS_PROPERTY_TM_MOTION);
	int currentVal = obs_data_get_int(val, "text-motion-speed");
	createTMSlider(prop, -1, minVal, maxVal, stepVal, currentVal, hlayout2, true, true, true);
	connect(group2, QOverload<int, bool>::of(&QButtonGroup::buttonToggled), [hlayout2, this](int index, bool) { setLayoutEnable(hlayout2, index == 0); });
	int isAgain = obs_data_get_int(val, "text-motion");
	group2->button(isAgain == 1 ? 0 : 1)->setChecked(true);

	WidgetInfo *wi = new WidgetInfo(this, prop, group2);
	connect(group2, QOverload<int>::of(&QButtonGroup::buttonClicked), wi, &WidgetInfo::ControlChanged);
	children.emplace_back(wi);

	QLabel *speedLabel = new QLabel(QTStr("textmotion.speed"));
	speedLabel->setObjectName("subLabel");
	flayout->addRow(speedLabel, hlayout2);
	m_tmLabels.append(speedLabel);

	hlayout2->setEnabled(obs_data_get_bool(val, "is-text-motion-speed"));

	QWidget *w = new QWidget;
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddImageGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	int index = obs_data_get_int(settings, name);
	QGridLayout *gLayout = new QGridLayout;
	gLayout->setHorizontalSpacing(10);
	gLayout->setContentsMargins(0, 0, 0, 0);

	QPointer<QButtonGroup> buttonGroup = new QButtonGroup(gLayout);

	size_t count = obs_property_image_group_item_count(prop);
	int row = 0, colum = 0;
	obs_image_style_type type;
	obs_property_image_group_params(prop, &row, &colum, &type);
	row = row ? row : 1;
	colum = colum ? colum : 1;

	if (type == OBS_IMAGE_STYLE_TEMPLATE) {
		gLayout->setHorizontalSpacing(12);
		gLayout->setVerticalSpacing(20);
	} else if (type == OBS_IMAGE_STYLE_SOLID_COLOR)
		gLayout->setHorizontalSpacing(8);
	else if (type == OBS_IMAGE_STYLE_GRADIENT_COLOR)
		gLayout->setHorizontalSpacing(9);
	else if (type == OBS_IMAGE_STYLE_BORDER_BUTTON) {
		gLayout->setHorizontalSpacing(6);
		gLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	} else if (type == OBS_IMAGE_STYLE_APNG_BUTTON) {
		gLayout->setHorizontalSpacing(12);
		gLayout->setVerticalSpacing(20);
	}

	for (size_t i = 0; i < count; i++) {
		QString url = obs_property_image_group_item_url(prop, int(i));
		const char *subName = obs_property_image_group_item_name(prop, int(i));
		if (type == OBS_IMAGE_STYLE_BORDER_BUTTON) {
			BorderImageButton *button = new BorderImageButton(buttonGroup, type, url, int(i), int(i) == index, subName);
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
		} else if (type == OBS_IMAGE_STYLE_APNG_BUTTON) {
			double _dpi = 0;
			if (pls_get_main_view()) {
				//because the first get this dpi is incorrect, so get the main dpi, otherwise the apng movie will have the scale animations.
				_dpi = PLSDpiHelper::getDpi(pls_get_main_view());
			} else {
				_dpi = PLSDpiHelper::getDpi(this);
			}

			ImageAPNGButton *button = new ImageAPNGButton(buttonGroup, type, url, int(i), int(i) == index, subName, _dpi, QSize{157, 88});
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
			m_movieButtons.append(button);
		} else {
			ImageButton *button = new ImageButton(buttonGroup, type, url, int(i), int(i) == index);
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
		}
	}

	WidgetInfo *wi = new WidgetInfo(this, prop, buttonGroup);
	connect(buttonGroup, SIGNAL(buttonClicked(int)), wi, SLOT(UserOperation()));
	connect(buttonGroup, SIGNAL(buttonClicked(int)), wi, SLOT(ControlChanged()));
	children.emplace_back(wi);

	AddSpacer(obs_property_get_type(prop), layout);
	if (PROPERTY_FLAG_NO_LABEL_SINGLE & obs_property_get_flags(prop))
		layout->addRow(gLayout);
	else {
		label = new QLabel(QT_UTF8(obs_property_description(prop)));
		label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		layout->addRow(label, gLayout);
	}
}

void PLSPropertiesView::AddCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	QGridLayout *gLayout = new QGridLayout;
	gLayout->setHorizontalSpacing(20);
	gLayout->setVerticalSpacing(10);
	gLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	gLayout->setContentsMargins(0, 0, 0, 0);

	int row = 1;
	int colum = 1;
	obs_property_custom_group_row_column(prop, &row, &colum);
	row = (row) ? row : 1;
	colum = (colum) ? colum : 1;

	size_t count = obs_property_custom_group_item_count(prop);
	for (size_t i = 0; i < count; i++) {
		QHBoxLayout *hLayout = new QHBoxLayout();
		hLayout->setMargin(0);
		hLayout->setSpacing(20);

		QWidget *item = nullptr;
		QLayout *subLayout = nullptr;

		QLabel *descLabel = new QLabel(QT_UTF8(obs_property_custom_group_item_desc(prop, i)));
		descLabel->setObjectName("subLabel");
		hLayout->addWidget(descLabel);

		enum obs_control_type type = obs_property_custom_group_item_type(prop, i);
		switch (type) {
		case OBS_CONTROL_UNKNOWN:
			return;
		case OBS_CONTROL_INT:
			item = addIntForCustomGroup(prop, int(i));
			break;
		default:
			break;
		}

		QString name = obs_property_custom_group_item_name(prop, i);
		if (item) {
			item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			item->setProperty("idx", i);
			item->setProperty("child_type", type);
			item->setProperty("child_name", name);
			hLayout->addWidget(item);
		} else if (subLayout) {
			subLayout->setProperty("idx", i);
			subLayout->setProperty("child_type", type);
			subLayout->setProperty("child_name", name);
			hLayout->addLayout(subLayout);
		}
		gLayout->addLayout(hLayout, int(i) / colum, int(i) % colum);
	}

	AddSpacer(obs_property_get_type(prop), layout);

	if (PROPERTY_FLAG_NO_LABEL_SINGLE & obs_property_get_flags(prop)) {
		layout->addRow(gLayout);
	} else {
		label = new QLabel(QT_UTF8(obs_property_description(prop)));
		layout->addRow(label, gLayout);
	}
}

void PLSPropertiesView::AddHLine(obs_property_t *prop, QFormLayout *layout, QLabel *&)
{
	QWidget *w = new QWidget;
	w->setObjectName("horiLine");

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(w);
}

QWidget *PLSPropertiesView::addIntForCustomGroup(obs_property_t *prop, int index)
{
	PLSSpinBox *spins = new PLSSpinBox(this);
	bool isEnabled = obs_property_enabled(prop);
	spins->setObjectName(OBJECT_NAME_SPINBOX);
	spins->setEnabled(isEnabled);

	int val = 0;
	const char *name = obs_property_custom_group_item_name(prop, index);
	if (name)
		val = (int)obs_data_get_int(settings, name);

	const char *suffix = obs_property_custom_group_int_suffix(prop, index);
	if (suffix)
		spins->setSuffix(QT_UTF8(suffix));

	int minVal = 0, maxVal = 0, stepVal = 0;
	obs_properties_custom_group_int_params(prop, &minVal, &maxVal, &stepVal, index);

	spins->setMinimum(minVal);
	spins->setMaximum(maxVal);
	spins->setSingleStep(stepVal);
	spins->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spins->setValue(val);

	WidgetInfo *info = new WidgetInfo(this, prop, spins);
	children.emplace_back(info);

	connect(spins, SIGNAL(valueChanged(int)), info, SLOT(UserOperation()));
	connect(spins, SIGNAL(valueChanged(int)), info, SLOT(ControlChanged()));
	connect(spins, SIGNAL(valueChanged(int)), this, SLOT(OnIntValueChanged(int)));
	return spins;
}

void PLSPropertiesView::AddCheckboxGroup(obs_property_t *prop, QFormLayout *layout)
{
	size_t count = obs_property_checkbox_group_item_count(prop);

	QHBoxLayout *hBtnLayout = new QHBoxLayout();
	hBtnLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hBtnLayout->setSpacing(20);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	for (size_t i = 0; i < count; ++i) {
		const char *desc = obs_property_checkbox_group_item_text(prop, i);
		QCheckBox *checkbox = new QCheckBox(QT_UTF8(desc), this);
		checkbox->setProperty("idx", i);

		auto enabled = obs_property_checkbox_group_item_enabled(prop, i);
		checkbox->setEnabled(enabled);

		const char *id = obs_property_checkbox_group_item_id(prop, i);
		checkbox->setChecked(obs_data_get_bool(settings, id));

		auto tooltip = obs_property_checkbox_group_item_tooltip(prop, i);
		if (nullptr != tooltip) {
			checkbox->setToolTip(QString::fromUtf8(tooltip));
			if (!enabled) {
				checkbox->setAttribute(Qt::WA_AlwaysShowToolTips);
			}
		}

		WidgetInfo *info = new WidgetInfo(this, prop, checkbox);
		connect(checkbox, SIGNAL(clicked()), info, SLOT(UserOperation()));
		connect(checkbox, SIGNAL(clicked()), info, SLOT(ControlChanged()));
		children.emplace_back(info);
		hBtnLayout->addWidget(checkbox);
	}

	QLabel *nameLabel = new QLabel(QT_UTF8(obs_property_description(prop)));
	nameLabel->setObjectName(OBJECT_NAME_FORMLABEL);
	if (const char *id = obs_source_get_id(pls_get_source_by_pointer_address(obj)); id && !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		nameLabel->setWordWrap(true);
	}
	layout->addRow(nameLabel, hBtnLayout);
}

void PLSPropertiesView::AddIntGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	size_t count = obs_property_int_group_item_count(prop);

	QHBoxLayout *hBtnLayout = new QHBoxLayout();
	hBtnLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hBtnLayout->setSpacing(7);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	for (size_t i = 0; i < count; ++i) {

		int minVal = 0, maxVal = 0, stepVal = 0;
		char *des = "";
		char *subName = "";
		obs_property_int_group_item_params(prop, &subName, &des, &minVal, &maxVal, &stepVal, i);
		int val = (int)obs_data_get_int(settings, subName);

		PLSSpinBox *spinsView = new PLSSpinBox(this);
		spinsView->makeTextVCenter();
		spinsView->setObjectName(OBJECT_NAME_SPINBOX);
		spinsView->setEnabled(true);
		spinsView->setSuffix(QT_UTF8(""));
		spinsView->setMinimum(minVal);
		spinsView->setMaximum(maxVal);
		spinsView->setSingleStep(stepVal);
		spinsView->setToolTip(QT_UTF8(obs_property_long_description(prop)));
		spinsView->setValue(val);
		spinsView->setProperty("child_name", subName);

		WidgetInfo *info = new WidgetInfo(this, prop, spinsView);
		children.emplace_back(info);
		hBtnLayout->addWidget(spinsView);

		QLabel *nameLabel = new QLabel(QT_UTF8(des));
		nameLabel->setObjectName("spinSubLabel");
		hBtnLayout->addWidget(nameLabel);

		connect(spinsView, SIGNAL(valueChanged(int)), info, SLOT(UserOperation()));
		connect(spinsView, SIGNAL(valueChanged(int)), info, SLOT(ControlChanged()));
	}

	AddSpacer(obs_property_get_type(prop), layout);
	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(label, hBtnLayout);
}

void PLSPropertiesView::AddTimerListListen(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{

	const char *name = obs_property_name(prop);
	QComboBox *combo = new PLSComboBox(this);
	combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	combo->setObjectName(OBJECT_NAME_COMBOBOX);

	obs_combo_type type = obs_property_list_type(prop);
	obs_combo_format format = obs_property_list_format(prop);
	size_t count = obs_property_list_item_count(prop);
	int idx = -1;

	for (size_t i = 0; i < count; i++)
		AddComboItem(combo, prop, format, i);

	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	string value = from_obs_data(settings, name, format);

	if (format == OBS_COMBO_FORMAT_STRING && type == OBS_COMBO_TYPE_EDITABLE) {
		// Warning : Here we must invoke setText() after setCurrentIndex()
		combo->setCurrentIndex(combo->findData(QByteArray(value.c_str())));
		combo->lineEdit()->setText(QT_UTF8(value.c_str()));
	} else {
		idx = combo->findData(QByteArray(value.c_str()));
	}

	if (idx != -1) {
		SetComboBoxStyle(combo, idx);
		combo->setCurrentIndex(idx);
	}
	combo->model();
	WidgetInfo *info = new WidgetInfo(this, prop, combo);
	connect(combo, SIGNAL(currentIndexChanged(int)), info, SLOT(UserOperation()));
	connect(combo, SIGNAL(currentIndexChanged(int)), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	/* trigger a settings update if the index was not found */
	if (idx == -1)
		info->ControlChanged();

	QHBoxLayout *hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->setSpacing(10);

	QPushButton *listenButton = new QPushButton();
	listenButton->setObjectName("listenBtn");

	auto isChecked = obs_data_get_bool(settings, "listen_list_btn");
	listenButton->setProperty(PROPERTY_NAME_SOURCE_SELECT, isChecked);
	listenButton->setEnabled(obs_property_enabled(prop));
	listenButton->setToolTip(isChecked ? tr("timer.source.listen.btn.checked.tip") : tr("timer.source.listen.btn.no.checked.tip"));
	WidgetInfo *btnInfo = new WidgetInfo(this, prop, listenButton);
	connect(listenButton, SIGNAL(clicked()), btnInfo, SLOT(UserOperation()));
	connect(listenButton, SIGNAL(clicked()), btnInfo, SLOT(ControlChanged()));
	children.emplace_back(btnInfo);

	hlayout->addWidget(combo);
	hlayout->addWidget(listenButton);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, hlayout);
}

void PLSPropertiesView::AddLabelTip(obs_property_t *prop, QFormLayout *layout)
{
	QHBoxLayout *hLayout = new QHBoxLayout();
	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->setContentsMargins(0, 2, 0, 0);
	hLayout->setSpacing(0);

	QLabel *label = new QLabel(QT_UTF8(obs_property_description(prop)));
	label->setObjectName("tipLabel");
	hLayout->addWidget(label);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(nullptr, hLayout);
}

class CameraVirtualBackgroundStateButton : public QFrame {
	bool hovered = false;
	bool pressed = false;
	std::function<void()> clicked;

public:
	CameraVirtualBackgroundStateButton(const QString &buttonText, QWidget *parent, std::function<void()> clicked_) : QFrame(parent), clicked(std::move(clicked_))
	{
		setObjectName("cameraVirtualBackgroundButton");
		setMouseTracking(true);

		QLabel *icon = new QLabel(this);
		icon->setObjectName("icon");
		icon->setMouseTracking(true);

		QLabel *text = new QLabel(this);
		text->setObjectName("text");
		text->setMouseTracking(true);
		text->setText(buttonText);

		QHBoxLayout *layout = new QHBoxLayout(this);
		layout->setMargin(0);
		layout->setSpacing(2);
		layout->addWidget(icon);
		layout->addWidget(text);
	}
	virtual ~CameraVirtualBackgroundStateButton() {}

private:
	void setState(const char *name, bool &state, bool value)
	{
		if (state != value) {
			pls_flush_style_recursive(this, name, state = value);
		}
	}

protected:
	virtual bool event(QEvent *event) override
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
		case QEvent::MouseMove: {
			setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
			break;
		}
		}
		return QFrame::event(event);
	}
};

void PLSPropertiesView::AddCameraVirtualBackgroundState(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	label = new QLabel(QT_UTF8(obs_property_description(prop)));

	QWidget *widget = new QWidget();
	widget->setObjectName("cameraVirtualBackground");

	QHBoxLayout *hlayout = new QHBoxLayout(widget);
	hlayout->setMargin(0);
	hlayout->setSpacing(20);

	int val = 0;
	if (auto source = pls_get_source_by_pointer_address(obj); source) {
		obs_data_t *privateSettings = obs_source_get_private_settings(source);
		const char *name = obs_property_name(prop);
		val = (int)obs_data_get_int(privateSettings, name);
		obs_data_release(privateSettings);
	}

	QLabel *state = new QLabel(tr(val == 0 ? "CameraProperties.VirtualBackgroundState.State.None" : "CameraProperties.VirtualBackgroundState.State.Using"), widget);
	state->setObjectName("cameraVirtualBackgroundState");

	CameraVirtualBackgroundStateButton *button =
		new CameraVirtualBackgroundStateButton(tr("CameraProperties.VirtualBackgroundState.State.Button"), widget, []() { pls_show_virtual_background(); });

	hlayout->addWidget(state);
	hlayout->addWidget(button);
	hlayout->addStretch(1);

	WidgetInfo *wi = new WidgetInfo(this, prop, widget);
	connect(wi, &WidgetInfo::PropertyUpdateNotify, state, [this, prop, state]() {
		int val = 0;
		if (auto source = pls_get_source_by_pointer_address(obj); source) {
			obs_data_t *privateSettings = obs_source_get_private_settings(source);
			const char *name = obs_property_name(prop);
			val = (int)obs_data_get_int(privateSettings, name);
			obs_data_release(privateSettings);
		}
		state->setText(tr(val == 0 ? "CameraProperties.VirtualBackgroundState.State.None" : "CameraProperties.VirtualBackgroundState.State.Using"));
	});
	children.emplace_back(wi);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, widget);
}

void PLSPropertiesView::AddVirtualBackgroundResource(obs_property_t *prop, QBoxLayout *layout)
{
	bool motionEnabled = obs_data_get_bool(settings, "motion_enabled");
	QString itemId = QString::fromUtf8(obs_data_get_string(settings, "item_id"));

	QWidget *widget = pls_create_virtual_background_resource_widget(
		nullptr,
		[prop, this](QWidget *widget) {
			connect(widget, SIGNAL(filterButtonClicked()), this, SLOT(OnVirtualBackgroundResourceOpenFilter()));

			WidgetInfo *wi = new WidgetInfo(this, prop, widget);
			connect(widget, SIGNAL(checkState(bool)), wi, SLOT(VirtualBackgroundResourceMotionDisabledChanged(bool)));
			connect(widget, SIGNAL(currentResourceChanged(QString, int, QString, QString, QString, bool, QString, QString)), wi,
				SLOT(VirtualBackgroundResourceSelected(QString, int, QString, QString, QString, bool, QString, QString)));
			connect(widget, SIGNAL(deleteCurrentResource(QString)), wi, SLOT(VirtualBackgroundResourceDeleted(QString)));
			connect(widget, SIGNAL(removeAllMyResource(QStringList)), wi, SLOT(VirtualBackgroundMyResourceDeleteAll(QStringList)));
			children.emplace_back(wi);
		},
		true, itemId, !motionEnabled, isFirstAddSource());

	layout->addWidget(widget, 1);
}

void PLSPropertiesView::SignalChanged()
{
	emit Changed();
}

void PLSPropertiesView::OnColorFilterOriginalPressed(bool state)
{
	colorFilterOriginalPressed = state;
	if (spinsView && sliderView) {
		if (infoView) {
			infoView->SetOriginalColorFilter(state);
		}
		if (colorFilterOriginalPressed && isColorFilter) {
			spinsView->setValue(0);
			spinsView->setEnabled(false);
			sliderView->setEnabled(false);
			sliderView->setProperty(STATUS_HANDLE, false);

		} else {
			spinsView->setEnabled(true);
			sliderView->setEnabled(true);
			sliderView->setProperty(STATUS_HANDLE, true);
		}
		pls_flush_style(sliderView);
	}
}

void PLSPropertiesView::OnIntValueChanged(int value)
{
	if (isColorFilter) {
		emit ColorFilterValueChanged(value);
	}
}

void PLSPropertiesView::UpdateColorFilterValue(int value, bool isOriginal)
{
	if (!isColorFilter) {
		return;
	}

	if (!spinsView) {
		return;
	}

	if (infoView) {
		infoView->SetOriginalColorFilter(isOriginal);
	}
	spinsView->setValue(value);
}

void PLSPropertiesView::OnShowScrollBar(bool isShow)
{
	if (!contentWidget) {
		return;
	}

	auto isPrismMobileSource = false;
	if (const auto sourceId = getSourceId(); nullptr != sourceId && strcmp(sourceId, PRISM_MOBILE_SOURCE_ID) == 0) {
		isPrismMobileSource = true;
	}

	if (isShow && scroll && setCustomContentMargins) {
		int width = scroll->width();
		double dpi = PLSDpiHelper::getDpi(this);
		contentWidget->setContentsMargins(PLSDpiHelper::calculate(dpi, 10), 0, PLSDpiHelper::calculate(dpi, 19) - width, PLSDpiHelper::calculate(dpi, isPrismMobileSource ? 15 : 50));

		PLSDpiHelper dpiHelper;
		dpiHelper.setDynamicContentsMargins(contentWidget, true);
	} else if (setCustomContentMargins) {
		contentWidget->setContentsMargins(PLSDpiHelper::calculate(this, QMargins(10, 0, 19, isPrismMobileSource ? 15 : 50)));
	}
}

void PLSPropertiesView::OnOpenMusicButtonClicked(OBSSource source)
{
	emit OpenMusicButtonClicked(source);
}

void PLSPropertiesView::OnVirtualBackgroundResourceOpenFilter()
{
	PLS_UI_STEP(PROPERTY_MODULE, "Filter", ACTION_CLICK);
	emit OpenFilters();
}

void PLSPropertiesView::PropertyUpdateNotify(const QString &name)
{
	for (const std::shared_ptr<WidgetInfo> child : children) {
		const char *pname = obs_property_name(child->property);
		if (pname && pname[0] && (name == QString::fromUtf8(pname))) {
			child->PropertyUpdateNotify();
			break;
		}
	}
}

void PLSPropertiesView::ResetProperties(obs_properties_t *newProperties)
{
	if (!newProperties || !reloadCallback || (properties && !properties.get_deleter()))
		return;

	if (obj) {
		properties.reset(newProperties);
	} else {
		properties.reset(newProperties);
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;
}

static bool FrameRateChangedVariant(const QVariant &variant, media_frames_per_second &fps, obs_data_item_t *&obj, const media_frames_per_second *valid_fps)
{
	if (!variant.canConvert<media_frames_per_second>())
		return false;

	fps = variant.value<media_frames_per_second>();
	if (valid_fps && fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	return true;
}

static bool FrameRateChangedCommon(PLSFrameRatePropertyWidget *w, obs_data_item_t *&obj, const media_frames_per_second *valid_fps)
{
	media_frames_per_second fps{};
	if (!FrameRateChangedVariant(w->simpleFPS->currentData(), fps, obj, valid_fps))
		return false;

	UpdateRationalFPSWidgets(w, &fps);
	return true;
}

static bool FrameRateChangedRational(PLSFrameRatePropertyWidget *w, obs_data_item_t *&obj, const media_frames_per_second *valid_fps)
{
	auto num = w->numEdit->value();
	auto den = w->denEdit->value();

	auto fps = make_fps(num, den);
	if (valid_fps && media_frames_per_second_is_valid(fps) && fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	UpdateSimpleFPSSelection(w, &fps);
	return true;
}

static bool FrameRateChanged(QWidget *widget, const char *name, OBSData &settings)
{
	auto w = qobject_cast<PLSFrameRatePropertyWidget *>(widget);
	if (!w)
		return false;

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>())
		return false;

	auto StopUpdating = [&](void *) { w->updating = false; };
	unique_ptr<void, decltype(StopUpdating)> signalGuard(static_cast<void *>(w), StopUpdating);
	w->updating = true;

	if (!obs_data_has_user_value(settings, name))
		obs_data_set_obj(settings, name, nullptr);

	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};
	auto obj_ptr = obj.get();
	auto CheckObj = [&]() {
		if (!obj_ptr)
			obj.release();
	};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	auto tag = variant.value<frame_rate_tag>();
	switch (tag.type) {
	case frame_rate_tag::SIMPLE:
		if (!FrameRateChangedCommon(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::RATIONAL:
		if (!FrameRateChangedRational(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::USER:
		if (tag.val && option && strcmp(tag.val, option) == 0)
			return false;

		obs_data_item_set_frames_per_second(&obj_ptr, {}, tag.val);
		break;
	}

	UpdateFPSLabels(w);
	CheckObj();
	return true;
}

void WidgetInfo::CheckValue()
{
}

void WidgetInfo::SetOriginalColorFilter(bool state)
{
	isOriginColorFilter = state;
}

void WidgetInfo::BoolChanged(const char *setting)
{
	QCheckBox *checkbox = static_cast<QCheckBox *>(widget);
	bool isChecked = checkbox->checkState() == Qt::Checked;
	obs_data_set_bool(view->settings, setting, isChecked);
	PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %s", setting ? setting : "checkbox", isChecked ? "checked" : "unchecked");
}

void WidgetInfo::BoolGroupChanged(const char *setting)
{
	QRadioButton *radiobutton = static_cast<QRadioButton *>(widget);
	int idx = radiobutton->property("idx").toInt();
	obs_data_set_int(view->settings, setting, idx);
	PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %d", setting, idx);

	if (obs_property_bool_group_clicked(property, view->obj, idx)) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void WidgetInfo::IntChanged(const char *setting)
{
	QSpinBox *spin = static_cast<QSpinBox *>(widget);
	if (!isOriginColorFilter) {
		obs_data_set_int(view->settings, setting, spin->value());
	}
}

void WidgetInfo::FloatChanged(const char *setting)
{
	QDoubleSpinBox *spin = static_cast<QDoubleSpinBox *>(widget);
	obs_data_set_double(view->settings, setting, spin->value());
}

void WidgetInfo::TextChanged(const char *setting)
{
	obs_text_type type = obs_property_text_type(property);

	if (type == OBS_TEXT_MULTILINE) {
		QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(widget);
		obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->toPlainText()));
		return;
	}

	if (type == OBS_TEXT_DEFAULT_LIMIT) {
		QLineEdit *edit = static_cast<QLineEdit *>(widget);

		int maxTextLength = MAX_TEXT_LENGTH;
		int length = obs_property_get_length_limit(property);
		if (length > 0 && length < MAX_TEXT_LENGTH) {
			maxTextLength = length;
		}
		QString limitText = edit->text();
		if (edit->text().length() > maxTextLength) {
			QSignalBlocker signalBlocker(edit);
			limitText = edit->text().left(maxTextLength);
			edit->setText(limitText);
		}
		obs_data_set_string(view->settings, setting, QT_TO_UTF8(limitText));
		return;
	}
	// Fix #3757 Filter leading and trailing spaces for input in ffmpeg source
	QLineEdit *edit = static_cast<QLineEdit *>(widget);
	OBSSource source = pls_get_source_by_pointer_address(view->obj);
	if (source) {
		const char *id = obs_source_get_id(source);

		if (0 == strcmp(id, MEDIA_SOURCE_ID) && QT_UTF8(setting) == "input")
			obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->text().simplified()));
		else
			obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->text()));
	} else
		obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->text()));
}

bool WidgetInfo::PathChanged(const char *setting)
{
	const char *desc = obs_property_description(property);
	obs_path_type type = obs_property_path_type(property);
	const char *filter = obs_property_path_filter(property);
	const char *default_path = obs_property_path_default_path(property);
	QString path;

	if (type == OBS_PATH_DIRECTORY)
		path = QFileDialog::getExistingDirectory(view, QT_UTF8(desc), QT_UTF8(default_path), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	else if (type == OBS_PATH_FILE)
		path = QFileDialog::getOpenFileName(view, QT_UTF8(desc), QT_UTF8(default_path), QT_UTF8(filter));
	else if (type == OBS_PATH_FILE_SAVE)
		path = QFileDialog::getSaveFileName(view, QT_UTF8(desc), QT_UTF8(default_path), QT_UTF8(filter));

	if (path.isEmpty())
		return false;

	PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s changed.", desc ? desc : "Path");

	QLineEdit *edit = static_cast<QLineEdit *>(widget);
	edit->setText(path);
	obs_data_set_string(view->settings, setting, QT_TO_UTF8(path));
	return true;
}

void WidgetInfo::ListChanged(const char *setting)
{
	QComboBox *combo = static_cast<QComboBox *>(widget);
	obs_combo_format format = obs_property_list_format(property);
	obs_combo_type type = obs_property_list_type(property);
	QVariant data;

	if (type == OBS_COMBO_TYPE_EDITABLE) {
		data = combo->currentText().toUtf8();
	} else {
		int index = combo->currentIndex();
		if (index != -1)
			data = combo->itemData(index);
		else
			return;
	}

	if (combo->currentIndex() >= 0)
		PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %s", setting ? setting : "ComboxList", combo->currentText().toStdString().c_str());
	else
		PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: no selected", setting ? setting : "ComboxList");

	switch (format) {
	case OBS_COMBO_FORMAT_INVALID:
		return;
	case OBS_COMBO_FORMAT_INT:
		obs_data_set_int(view->settings, setting, data.value<long long>());
		break;
	case OBS_COMBO_FORMAT_FLOAT:
		obs_data_set_double(view->settings, setting, data.value<double>());
		break;
	case OBS_COMBO_FORMAT_STRING:
		obs_data_set_string(view->settings, setting, data.toByteArray().constData());
		break;
	}

	if (auto id = view->getSourceId(); (id && !strcmp(id, "decklink-input") || !strcmp(view->type.c_str(), "decklink_output")) && !strcmp(setting, "device_hash")) {
		obs_data_set_string(view->settings, "device_name", combo->currentText().toUtf8().constData());
	}
}

bool WidgetInfo::ColorChanged(const char *setting)
{
	shared_ptr<WidgetInfo> this_object = this->shared_from_this();
	QByteArray _id = setting;
	const char *desc = obs_property_description(property);
	obs_property_type type = obs_property_get_type(property);

	long long val = 0;
	if (type == OBS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(view->settings, setting);
		val = obs_data_get_int(color_obj, "color_val");
		obs_data_release(color_obj);
	} else {
		val = obs_data_get_int(view->settings, setting);
	}

	QColor color = color_from_int(val);

	QColorDialog::ColorDialogOptions options = 0;

	/* The native dialog on OSX has all kinds of problems, like closing
	 * other open QDialogs on exit, and
	 * https://bugreports.qt-project.org/browse/QTBUG-34532
	 */
#ifdef __APPLE__
	options |= QColorDialog::DontUseNativeDialog;
#endif

	color = PLSColorDialogView::getColor(color, view, QT_UTF8(desc), options);
	color.setAlpha(255);

	if (!color.isValid())
		return false;

	//RenJinbo #9685 font color select crash, not used released pointer.
	this_object->view->textColorChanged(_id, color);
	return false;
}

bool WidgetInfo::FontChanged(const char *setting)
{
	obs_data_t *font_obj = obs_data_get_obj(view->settings, setting);
	bool success;
	uint32_t flags;
	QFont font;

	QFontDialog::FontDialogOptions options;

#ifdef __APPLE__
	options = QFontDialog::DontUseNativeDialog;
#endif

	if (!font_obj) {
		QFont initial;
		font = PLSFontDialogView::getFont(&success, initial, view, "Pick a Font", options);
	} else {
		MakeQFont(font_obj, font);
		font = PLSFontDialogView::getFont(&success, font, view, "Pick a Font", options);
		obs_data_release(font_obj);
	}

	if (!success)
		return false;

	font_obj = obs_data_create();

	PLS_INFO(PROPERTY_MODULE, "PropertyOperation FontDialog font:%s style:%s size:%d", font.family().toStdString().c_str(), font.styleName().toStdString().c_str(), font.pointSize());

	obs_data_set_string(font_obj, "face", QT_TO_UTF8(font.family()));
	obs_data_set_string(font_obj, "style", QT_TO_UTF8(font.styleName()));
	obs_data_set_int(font_obj, "size", font.pointSize());
	flags = font.bold() ? OBS_FONT_BOLD : 0;
	flags |= font.italic() ? OBS_FONT_ITALIC : 0;
	flags |= font.underline() ? OBS_FONT_UNDERLINE : 0;
	flags |= font.strikeOut() ? OBS_FONT_STRIKEOUT : 0;
	obs_data_set_int(font_obj, "flags", flags);

	QLabel *label = static_cast<QLabel *>(widget);
	QFont labelFont;
	MakeQFont(font_obj, labelFont, true);
	label->setFont(labelFont);
	label->setStyleSheet(QString("font-size:%1px;").arg(labelFont.pointSize()));
	label->setText(QString("%1 %2").arg(font.family(), font.styleName()));

	obs_data_set_obj(view->settings, setting, font_obj);
	obs_data_release(font_obj);
	return true;
}

void WidgetInfo::GroupChanged(const char *setting)
{
	QGroupBox *groupbox = static_cast<QGroupBox *>(widget);
	obs_data_set_bool(view->settings, setting, groupbox->isCheckable() ? groupbox->isChecked() : true);
}

void WidgetInfo::ChatTemplateListChanged(const char *setting)
{
	QButtonGroup *buttonGroup = static_cast<QButtonGroup *>(object);
	obs_data_set_int(view->settings, setting, buttonGroup->checkedId());
}

void WidgetInfo::ChatFontSizeChanged(const char *setting)
{
	PLSSpinBox *spinBox = static_cast<PLSSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spinBox->value());
}

void WidgetInfo::TMTextChanged(const char *setting)
{
	obs_data_t *tm_text_obj = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();
	obs_data_t *tm_new_text_obj = obs_data_create();
	obs_data_set_string(tm_new_text_obj, "font-family", obs_data_get_string(tm_text_obj, "font-family"));
	obs_data_set_string(tm_new_text_obj, "font-weight", obs_data_get_string(tm_text_obj, "font-weight"));
	obs_data_set_int(tm_new_text_obj, "font-size", obs_data_get_int(tm_text_obj, "font-size"));
	obs_data_set_bool(tm_new_text_obj, "is-h-aligin", obs_data_get_bool(tm_text_obj, "is-h-aligin"));
	obs_data_set_int(tm_new_text_obj, "h-aligin", obs_data_get_int(tm_text_obj, "h-aligin"));
	obs_data_set_int(tm_new_text_obj, "letter", obs_data_get_int(tm_text_obj, "letter"));

	if ("tmFontBox" == objName) {
		QString currentFamily(static_cast<PLSComboBox *>(object)->currentText());
		QStringList styles(view->m_fontDatabase.styles(currentFamily));
		QString weight;
		if (!styles.isEmpty()) {
			weight = styles.first();
		}
		obs_data_set_string(tm_new_text_obj, "font-family", currentFamily.toUtf8());
		obs_data_set_string(tm_new_text_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:family ", ACTION_CLICK);

	} else if ("tmFontStyleBox" == objName) {
		QString weight(static_cast<PLSComboBox *>(object)->currentText());
		obs_data_set_string(tm_new_text_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:font-weight ", ACTION_CLICK);

	} else if (object->property("index").isValid() && "spinBox" == objName) {

		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_text_obj, "font-size", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:font-size ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(view->settings, "width", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:box width ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(view->settings, "height", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:box height ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (object->property("index").isValid() && "slider" == objName) {
		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_text_obj, "font-size", static_cast<QSlider *>(object)->value());
			break;
		case 1:
			obs_data_set_int(view->settings, "width", static_cast<QSlider *>(object)->value());
			break;
		case 2:
			obs_data_set_int(view->settings, "height", static_cast<QSlider *>(object)->value());
			break;
		default:
			break;
		}

	} else if ("group" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_text_obj, "h-aligin", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:h-aligin ", ACTION_CLICK);

	} else if ("group2" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_text_obj, "letter", checkId);
		} else {
			obs_data_set_int(tm_new_text_obj, "letter", -1);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:letter ", ACTION_CLICK);
	}
	obs_data_set_obj(view->settings, setting, tm_new_text_obj);
	obs_data_release(tm_text_obj);
	obs_data_release(tm_new_text_obj);
}
void WidgetInfo::TMTextContentChanged(const char *setting)
{
	obs_data_t *text_content_obj = obs_data_get_obj(view->settings, setting);
	obs_data_t *text_net_content_obj = obs_data_create();

	obs_data_set_int(text_net_content_obj, "text-count", obs_data_get_int(text_content_obj, "text-count"));
	obs_data_set_string(text_net_content_obj, "text-content-1", obs_data_get_string(text_content_obj, "text-content-1"));
	obs_data_set_bool(text_net_content_obj, "text-content-change-1", obs_data_get_bool(text_content_obj, "text-content-change-1"));
	obs_data_set_string(text_net_content_obj, "text-content-2", obs_data_get_string(text_content_obj, "text-content-2"));
	obs_data_set_bool(text_net_content_obj, "text-content-change-2", obs_data_get_bool(text_content_obj, "text-content-change-2"));

	QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(object);
	QString editStr = edit->toPlainText().trimmed();
	if (edit->objectName() == QString("%1_%2").arg(OBJECT_NAME_PLAINTEXTEDIT).arg(1)) {

		obs_data_set_string(text_net_content_obj, "text-content-1", editStr.toUtf8());
		obs_data_set_bool(text_net_content_obj, "text-content-change-1", true);

	} else if (edit->objectName() == QString("%1_%2").arg(OBJECT_NAME_PLAINTEXTEDIT).arg(2)) {
		obs_data_set_string(text_net_content_obj, "text-content-2", editStr.toUtf8());
		obs_data_set_bool(text_net_content_obj, "text-content-change-2", true);
	}
	obs_data_set_obj(view->settings, setting, text_net_content_obj);
	obs_data_release(text_content_obj);
	obs_data_release(text_net_content_obj);
}

void WidgetInfo::TMTextTabChanged(const char *setting)
{
	QButtonGroup *buttonGroup = static_cast<QButtonGroup *>(object);
	obs_data_set_int(view->settings, setting, buttonGroup->checkedId());
	std::string info = "property window:" + std::string(setting);
	PLS_UI_STEP(PROPERTY_MODULE, info.c_str(), ACTION_CLICK);
}

void WidgetInfo::TMTextTemplateTabChanged(const char *setting)
{
	QButtonGroup *buttonGroup = static_cast<QButtonGroup *>(object);
	int checkId = buttonGroup->checkedId();
	if (checkId >= 0) {
		obs_data_set_int(view->settings, setting, buttonGroup->checkedId());
	}
	std::string info = "property window:" + std::string(setting);
	PLS_UI_STEP(PROPERTY_MODULE, info.c_str(), ACTION_CLICK);
}

void WidgetInfo::TMTextTemplateListChanged(const char *setting)
{
	QButtonGroup *buttonGroup = static_cast<QButtonGroup *>(object);
	int checkId = buttonGroup->checkedId();
	if (checkId >= 0) {
		obs_data_set_int(view->settings, setting, checkId);
	}
	std::string info = "property window:" + std::string(setting);
	PLS_UI_STEP(PROPERTY_MODULE, info.c_str(), ACTION_CLICK);
}

void WidgetInfo::TMTextColorChanged(const char *setting)
{
	obs_data_t *tm_color = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();

	obs_data_t *tm_new_color = obs_data_create();

	obs_data_set_int(tm_new_color, "text-color", obs_data_get_int(tm_color, "text-color"));
	obs_data_set_int(tm_new_color, "text-color-alpha", obs_data_get_int(tm_color, "text-color-alpha"));
	obs_data_set_bool(tm_new_color, "text-color-change", obs_data_get_bool(tm_color, "text-color-change"));
	obs_data_set_bool(tm_new_color, "is-color", obs_data_get_bool(tm_color, "is-color"));
	obs_data_set_bool(tm_new_color, "is-text-color-alpha", obs_data_get_bool(tm_color, "is-text-color-alpha"));
	obs_data_set_bool(tm_new_color, "is-bk-color-on", obs_data_get_bool(tm_color, "is-bk-color-on"));
	obs_data_set_bool(tm_new_color, "is-bk-color", obs_data_get_bool(tm_color, "is-bk-color"));
	obs_data_set_bool(tm_new_color, "is-bk-init-color", obs_data_get_bool(tm_color, "is-bk-init-color"));
	obs_data_set_int(tm_new_color, "bk-color", obs_data_get_int(tm_color, "bk-color"));
	obs_data_set_bool(tm_new_color, "is-bk-color-alpha", obs_data_get_bool(tm_color, "is-bk-color-alpha"));
	obs_data_set_int(tm_new_color, "bk-color-alpha", obs_data_get_int(tm_color, "bk-color-alpha"));

	obs_data_set_bool(tm_new_color, "is-outline-color-on", obs_data_get_bool(tm_color, "is-outline-color-on"));
	obs_data_set_bool(tm_new_color, "is-outline-color", obs_data_get_bool(tm_color, "is-outline-color"));
	obs_data_set_int(tm_new_color, "outline-color", obs_data_get_int(tm_color, "outline-color"));
	obs_data_set_int(tm_new_color, "outline-color-line", obs_data_get_int(tm_color, "outline-color-line"));

	if (object->property("index").isValid() && "spinBox" == objName) {

		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_color, "text-color-alpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:text-color-alpha ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color-alpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:bk-color-alpha ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color-line", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:outline-color-line ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (object->property("index").isValid() && "slider" == objName) {
		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_color, "text-color-alpha", static_cast<QSlider *>(object)->value());

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color-alpha", static_cast<QSlider *>(object)->value());
			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color-line", static_cast<QSlider *>(object)->value());

			break;
		default:
			break;
		}

	} else if (object->property("index").isValid() && "checkBox" == objName) {
		switch (object->property("index").toInt()) {
		case 1:
			obs_data_set_bool(tm_new_color, "is-bk-color", static_cast<QCheckBox *>(object)->isChecked());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:is-bk-color ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_bool(tm_new_color, "is-outline-color", static_cast<QCheckBox *>(object)->isChecked());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:is-outline-color ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if ("group" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_color, "color-tab", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:color-tab ", ACTION_CLICK);
	} else {
		QColorDialog::ColorDialogOptions options = 0;
		QLabel *label = static_cast<QLabel *>(widget);
		const char *desc = obs_property_description(property);
		QColor color = PLSColorDialogView::getColor(label->text(), view->parentWidget(), QT_UTF8(desc), options);
		color.setAlpha(255);
		if (!color.isValid()) {
			obs_data_release(tm_new_color);
			return;
		}

		label->setText(color.name(QColor::HexRgb));
		QPalette palette = QPalette(color);
		label->setPalette(palette);
		label->setStyleSheet(QString("font-weight: normal;background-color :%1; color: %2;")
					     .arg(palette.color(QPalette::Window).name(QColor::HexRgb))
					     .arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

		obs_data_set_int(view->settings, setting, color_to_int(color));
		switch (label->property("index").toInt()) {
		case 0:
			obs_data_set_bool(tm_new_color, "text-color-change", true);
			obs_data_set_int(tm_new_color, "text-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:text-color ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:bk-color ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:outline-color ", ACTION_CLICK);

			break;
		default:
			break;
		}
	}
	obs_data_set_obj(view->settings, setting, tm_new_color);
	obs_data_release(tm_color);
	obs_data_release(tm_new_color);
}

void WidgetInfo::TMTextMotionChanged(const char *setting)
{
	obs_data_t *tm_motion = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();

	obs_data_t *tm_new_motion = obs_data_create();
	obs_data_set_int(tm_new_motion, "text-motion", obs_data_get_int(tm_motion, "text-motion"));
	obs_data_set_bool(tm_new_motion, "is-text-motion-speed", obs_data_get_bool(tm_motion, "is-text-motion-speed"));
	obs_data_set_int(tm_new_motion, "text-motion-speed", obs_data_get_int(tm_motion, "text-motion-speed"));

	if ("spinBox" == objName) {

		obs_data_set_int(tm_new_motion, "text-motion-speed", static_cast<QSpinBox *>(object)->value());

	} else if ("slider" == objName) {
		obs_data_set_int(tm_new_motion, "text-motion-speed", static_cast<QSlider *>(object)->value());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:text-motion-speed ", ACTION_CLICK);

	} else if ("group2" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_motion, "text-motion", qAbs(checkId - 1));
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:text-motion ", ACTION_CLICK);
	}
	obs_data_set_obj(view->settings, setting, tm_new_motion);
	obs_data_release(tm_motion);
	obs_data_release(tm_new_motion);
}

void WidgetInfo::EditableListChanged()
{
	const char *setting = obs_property_name(property);
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	obs_data_array *array = obs_data_array_create();

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		obs_data_t *arrayItem = obs_data_create();
		obs_data_set_string(arrayItem, "value", QT_TO_UTF8(item->text()));
		obs_data_set_bool(arrayItem, "selected", item->isSelected());
		obs_data_set_bool(arrayItem, "hidden", item->isHidden());
		obs_data_array_push_back(array, arrayItem);
		obs_data_release(arrayItem);
	}

	obs_data_set_array(view->settings, setting, array);
	obs_data_array_release(array);

	ControlChanged();
}

void WidgetInfo::ButtonClicked()
{
	OBSSource source = pls_get_source_by_pointer_address(view->obj);
	if (source) {
		if (0 == strcmp(obs_source_get_id(source), BGM_SOURCE_ID)) {
			OnOpenMusicButtonClicked();
		}
	}

	if (obs_property_button_clicked(property, view->obj)) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void WidgetInfo::ButtonGroupClicked(const char *)
{
	PLSPushButton *button = static_cast<PLSPushButton *>(widget);
	int idx = button->property("idx").toInt();
	if (obs_property_button_group_clicked(property, view->obj, idx)) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
		button->SetText(obs_property_button_group_item_text(property, idx));
	}
}

void WidgetInfo::TextButtonClicked()
{
	if (obs_property_text_button_clicked(property, view->obj)) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void WidgetInfo::TogglePasswordText(bool show)
{
	reinterpret_cast<QLineEdit *>(widget)->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
}

void WidgetInfo::SelectRegionClicked(const char *setting)
{
	obs_enter_graphics();
	auto max_size = gs_texture_get_max_size();
	obs_leave_graphics();
	PLSRegionCapture *regionCapture = new PLSRegionCapture(view);
	regionCapture->setWindowModality(Qt::ApplicationModal);
	connect(regionCapture, &PLSRegionCapture::selectedRegion, this, [=](const QRect &selectedRect) {
		qInfo() << "user selected a new region=" << selectedRect;
		obs_data_t *region_obj = obs_data_create();
		obs_data_set_int(region_obj, "left", selectedRect.left());
		obs_data_set_int(region_obj, "top", selectedRect.top());
		obs_data_set_int(region_obj, "width", selectedRect.width());
		obs_data_set_int(region_obj, "height", selectedRect.height());
		obs_data_set_obj(view->settings, setting, region_obj);
		obs_data_release(region_obj);

		view->refreshViewAfterUIChanged(property);
	});
	regionCapture->StartCapture(max_size, max_size);
}

void WidgetInfo::ImageGroupChanged(const char *setting)
{
	QButtonGroup *buttons = static_cast<QButtonGroup *>(object);
	obs_data_set_int(view->settings, setting, buttons->checkedId());
	for (auto subButton : buttons->buttons()) {
		pls_flush_style_recursive(subButton, "checked", subButton == buttons->checkedButton());
	}

	if (obs_property_image_group_clicked(property, view->obj, buttons->checkedId())) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void WidgetInfo::intCustomGroupChanged(const char *setting)
{
	QSpinBox *spin = static_cast<QSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spin->value());
}

void WidgetInfo::CustomGroupChanged(const char *)
{
	enum obs_control_type type = (obs_control_type)widget->property("child_type").toInt();
	QString childName = widget->property("child_name").toString();
	switch (type) {
	case OBS_CONTROL_UNKNOWN:
		break;
	case OBS_CONTROL_INT:
		intCustomGroupChanged(QT_TO_UTF8(childName));
		break;
	default:
		break;
	}
}

void WidgetInfo::CheckboxGroupChanged(const char *setting)
{
	QCheckBox *checkbox = static_cast<QCheckBox *>(widget);
	int idx = checkbox->property("idx").toInt();
	obs_data_set_int(view->settings, setting, idx);
	const char *id = obs_property_checkbox_group_item_id(property, idx);
	bool checked = checkbox->isChecked();
	obs_data_set_bool(view->settings, id, checked);
	obs_property_checkbox_group_clicked(property, view->obj, idx, checked);
	PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %d", setting, idx);
}

void WidgetInfo::intGroupChanged()
{
	QString childName = widget->property("child_name").toString();
	IntChanged(QT_TO_UTF8(childName));
}

void WidgetInfo::FontSimpleChanged(const char *setting)
{
	obs_data_t *font_obj = obs_data_get_obj(view->settings, setting);

	if (!font_obj) {
		PLS_WARN(PROPERTY_MODULE, "property window:font checked, the font_obj is nil");
	}

	QString objName = object->objectName();
	if ("FontCheckedFamilyBox" == objName) {
		QString currentFamily(static_cast<PLSComboBox *>(object)->currentText());
		QString weight = view->m_fontDatabase.styles(currentFamily).first();
		obs_data_set_string(font_obj, "font-family", currentFamily.toUtf8());
		obs_data_set_string(font_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:family", ACTION_CLICK);
	} else if ("FontCheckedWidgetBox" == objName) {
		QString weight(static_cast<PLSComboBox *>(object)->currentText());
		obs_data_set_string(font_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:font-weight ", ACTION_CLICK);
	}

	obs_data_set_obj(view->settings, setting, font_obj);
	obs_data_release(font_obj);
}

void WidgetInfo::ColorCheckBoxChanged(const char *setting)
{
	QString objName = object->objectName();
	if (OBJECT_NAME_FORMCHECKBOX == objName) {
		QCheckBox *checkBox = reinterpret_cast<QCheckBox *>(object);
		if (checkBox) {
			obs_data_t *color_obj = obs_data_get_obj(view->settings, setting);
			obs_data_set_bool(color_obj, "is_enable", checkBox->isChecked());
			obs_data_set_obj(view->settings, setting, color_obj);
			obs_data_release(color_obj);
		}
	} else {
		ColorChanged(setting);
	}
}

void WidgetInfo::timerListenListChanged(const char *setting)
{
	bool isSelect = obs_data_get_bool(view->settings, "listen_list_btn");
	QString objName = object->objectName();
	if (OBJECT_NAME_COMBOBOX == objName) {
		ListChanged(setting);
		if (isSelect) {
			obs_data_set_bool(view->settings, "listen_list_btn", !isSelect);
		}
	} else {
		obs_data_set_bool(view->settings, "listen_list_btn", !isSelect);
	}
}

void WidgetInfo::UserOperation()
{
	const char *setting = obs_property_name(property);
	if (!setting)
		return;

	obs_property_type type = obs_property_get_type(property);
	switch (type) {
	case OBS_PROPERTY_BOOL:
	case OBS_PROPERTY_BOOL_LEFT:
	case OBS_PROPERTY_LIST:
	case OBS_PROPERTY_BGM_MUSIC_LIST:
	case OBS_PROPERTY_BUTTON:
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_FONT:
	case OBS_PROPERTY_REGION_SELECT:
	case OBS_PROPERTY_PATH: {
		std::string controls = std::string("property-window:") + std::string(setting);
		PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		break;
	}
	case OBS_PROPERTY_INT:
	case OBS_PROPERTY_FLOAT: {
		if (widget) {
			QAbstractSpinBox *spin = reinterpret_cast<QAbstractSpinBox *>(widget);
			std::string controls = std::string("property-window:") + std::string(setting);
			if (spin) {
				controls = controls + " val:" + spin->text().toUtf8().constData();
			}
			PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		}
		break;
	}
	case OBS_PROPERTY_BUTTON_GROUP: {
		PLSPushButton *button = static_cast<PLSPushButton *>(widget);
		int idx = button->property("idx").toInt();
		std::string controls = std::string("property-window:") + obs_property_button_group_item_text(property, idx);
		PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		break;
	}
	case OBS_PROPERTY_IMAGE_GROUP: {
		QButtonGroup *buttonGroup = static_cast<QButtonGroup *>(object);
		int idx = buttonGroup->checkedId();
		std::string controls = std::string("property-window:") + obs_property_image_group_item_name(property, idx);
		PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		break;
	}
	case OBS_PROPERTY_CUSTOM_GROUP: {
		if (widget) {
			std::string controls = std::string("property-window:") + widget->property("child_name").toString().toStdString();
			PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		}
		break;
	}
	case OBS_PROPERTY_INT_GROUP: {
		if (widget) {
			QAbstractSpinBox *spin = reinterpret_cast<QAbstractSpinBox *>(widget);
			std::string controls = std::string("property-window:") + widget->property("child_name").toString().toStdString();
			if (spin) {
				controls = controls + " val:" + spin->text().toUtf8().constData();
			}
			PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		}
		break;
	}
	case OBS_PROPERTY_COLOR_CHECKBOX:
	case OBS_PROPERTY_TIMER_LIST_LISTEN: {
		if (widget) {
			std::string controls = std::string("property-window:") + std::string(setting) + " objectName:" + widget->objectName().toStdString();
			PLS_UI_STEP(PROPERTY_MODULE, controls.c_str(), ACTION_CLICK);
		}
		break;
	}

	default:
		break;
	}
}

void WidgetInfo::ControlChanged()
{
	const char *setting = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);
	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL_LEFT:
	case OBS_PROPERTY_BOOL:
	case OBS_PROPERTY_SWITCH:
		BoolChanged(setting);
		break;
	case OBS_PROPERTY_BOOL_GROUP:
		BoolGroupChanged(setting);
		break;
	case OBS_PROPERTY_INT:
		IntChanged(setting);
		break;
	case OBS_PROPERTY_FLOAT:
		FloatChanged(setting);
		break;
	case OBS_PROPERTY_TEXT:
		TextChanged(setting);
		break;
	case OBS_PROPERTY_LIST:
		ListChanged(setting);
		break;
	case OBS_PROPERTY_BUTTON:
		ButtonClicked();
		return;
	case OBS_PROPERTY_BUTTON_GROUP:
		ButtonGroupClicked(setting);
		return;
	case OBS_PROPERTY_COLOR:
		ColorChanged(setting);
		return;
	case OBS_PROPERTY_FONT:
		if (!FontChanged(setting))
			return;
		break;
	case OBS_PROPERTY_PATH:
		if (!PathChanged(setting))
			return;
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		break;
	case OBS_PROPERTY_FRAME_RATE:
		if (!FrameRateChanged(widget, setting, view->settings))
			return;
		break;
	case OBS_PROPERTY_GROUP:
		GroupChanged(setting);
		break;
	case OBS_PROPERTY_CHAT_TEMPLATE_LIST:
		ChatTemplateListChanged(setting);
		break;
	case OBS_PROPERTY_CHAT_FONT_SIZE:
		ChatFontSizeChanged(setting);
		break;
	case OBS_PROPERTY_TM_TEXT:
		TMTextChanged(setting);
		break;
	case OBS_PROPERTY_TM_TEXT_CONTENT:
		TMTextContentChanged(setting);
		break;
	case OBS_PROPERTY_TM_TAB:
		TMTextTabChanged(setting);
		break;
	case OBS_PROPERTY_TM_TEMPLATE_TAB:
		TMTextTemplateTabChanged(setting);
		break;
	case OBS_PROPERTY_TM_COLOR:
		TMTextColorChanged(setting);
		break;
	case OBS_PROPERTY_TM_TEMPLATE_LIST:
		TMTextTemplateListChanged(setting);
		break;
	case OBS_PROPERTY_TM_MOTION:
		TMTextMotionChanged(setting);
		break;
	case OBS_PROPERTY_REGION_SELECT:
		SelectRegionClicked(setting);
		return;
	case OBS_PROPERTY_IMAGE_GROUP:
		ImageGroupChanged(setting);
		break;
	case OBS_PROPERTY_CUSTOM_GROUP:
		CustomGroupChanged(setting);
		break;
	case OBS_PROPERTY_MOBILE_NAME:
		TextButtonClicked();
		break;
	case OBS_PROPERTY_CHECKBOX_GROUP:
		CheckboxGroupChanged(setting);
		break;
	case OBS_PROPERTY_INT_GROUP:
		intGroupChanged();
		break;
	case OBS_PROPERTY_FONT_SIMPLE:
		FontSimpleChanged(setting);
		break;
	case OBS_PROPERTY_COLOR_CHECKBOX:
		ColorCheckBoxChanged(setting);
		return;
	case OBS_PROPERTY_TIMER_LIST_LISTEN:
		timerListenListChanged(setting);
		break;
	}

	view->refreshViewAfterUIChanged(property);
}

void WidgetInfo::OnOpenMusicButtonClicked()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:music button", ACTION_CLICK);

	if (view) {
		obs_source_t *source = pls_get_source_by_pointer_address(view->obj);
		view->OnOpenMusicButtonClicked(source);
	}
}

void WidgetInfo::VirtualBackgroundResourceMotionDisabledChanged(bool motionDisabled)
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:motion disabled", ACTION_CLICK);

	obs_data_set_bool(view->settings, "motion_enabled", !motionDisabled);

	ControlChanged();
}

void WidgetInfo::VirtualBackgroundResourceSelected(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
						   const QString &foregroundPath, const QString &foregroundStaticImgPath)
{
	Q_UNUSED(foregroundPath)
	Q_UNUSED(foregroundStaticImgPath)

	PLS_UI_STEP(PROPERTY_MODULE, "property-window:virtual background resource", ACTION_CLICK);

	obs_data_set_bool(view->settings, "prism_resource", prismResource);
	obs_data_set_string(view->settings, "item_id", itemId.toUtf8().constData());
	obs_data_set_int(view->settings, "item_type", type);
	obs_data_set_string(view->settings, "file_path", resourcePath.toUtf8().constData());
	obs_data_set_string(view->settings, "image_file_path", staticImgPath.toUtf8().constData());
	obs_data_set_string(view->settings, "thumbnail_file_path", thumbnailPath.toUtf8().constData());

	ControlChanged();
}

void WidgetInfo::VirtualBackgroundResourceDeleted(const QString &itemId)
{
	const char *currentItemId = obs_data_get_string(view->settings, "item_id");
	if (!currentItemId || !currentItemId[0]) {
		view->reloadOldSettings();
	} else if (QString::fromUtf8(currentItemId) == itemId) {
		obs_data_set_bool(view->settings, "prism_resource", false);
		obs_data_set_string(view->settings, "item_id", "");
		obs_data_set_int(view->settings, "item_type", 0);
		obs_data_set_string(view->settings, "file_path", "");
		obs_data_set_string(view->settings, "image_file_path", "");
		obs_data_set_string(view->settings, "thumbnail_file_path", "");

		view->reloadOldSettings();
		ControlChanged();
	}
}

void WidgetInfo::VirtualBackgroundMyResourceDeleteAll(const QStringList &itemIds)
{
	const char *currentItemId = obs_data_get_string(view->settings, "item_id");
	if (!currentItemId || !currentItemId[0]) {
		view->reloadOldSettings();
	} else if (itemIds.contains(QString::fromUtf8(currentItemId))) {
		obs_data_set_bool(view->settings, "prism_resource", false);
		obs_data_set_string(view->settings, "item_id", "");
		obs_data_set_int(view->settings, "item_type", 0);
		obs_data_set_string(view->settings, "file_path", "");
		obs_data_set_string(view->settings, "image_file_path", "");

		view->reloadOldSettings();
		ControlChanged();
	}
}

#define PROPERTY_EDITITEM_EDIT "propertyEditItemEdit"
#define PROPERTY_EDITITEM_BROWSER "propertyEditItemBrowser"
#define PROPERTY_EDITITEM_BTNBOXCANCEL "propertyEditItemButtonBoxCancel"

class EditableItemDialog : public PLSDialogView {
	QLineEdit *edit;
	QString filter;
	QString default_path;

	void BrowseClicked()
	{
		QString curPath = QFileInfo(edit->text()).absoluteDir().path();

		if (curPath.isEmpty())
			curPath = default_path;

		QString path = QFileDialog::getOpenFileName(App()->GetMainWindow(), QTStr("Browse"), curPath, filter);
		if (path.isEmpty())
			return;

		edit->setText(path);
	}

public:
	explicit EditableItemDialog(QWidget *parent, const QString &text, bool browse, const char *filter_ = nullptr, const char *default_path_ = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper())
		: PLSDialogView(parent, dpiHelper), filter(QT_UTF8(filter_)), default_path(QT_UTF8(default_path_))
	{
		dpiHelper.setCss(this, {PLSCssIndex::EditableItemDialog});
		dpiHelper.setInitSize(this, QSize(400, 190));

		QHBoxLayout *topLayout = new QHBoxLayout();
		QVBoxLayout *mainLayout = new QVBoxLayout();

		edit = new QLineEdit();
		edit->setText(text);
		edit->setObjectName(PROPERTY_EDITITEM_EDIT);

		topLayout->addWidget(edit);
		topLayout->setAlignment(edit, Qt::AlignVCenter);

		if (browse) {
			QPushButton *browseButton = new QPushButton(QTStr("Browse"));
			browseButton->setProperty("themeID", "settingsButtons");
			browseButton->setObjectName(PROPERTY_EDITITEM_BROWSER);

			topLayout->addWidget(browseButton);
			topLayout->setAlignment(browseButton, Qt::AlignVCenter);

			connect(browseButton, &QPushButton::clicked, this, &EditableItemDialog::BrowseClicked);
		}

		QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;

		QDialogButtonBox *buttonBox = new QDialogButtonBox(buttons);
		buttonBox->setCenterButtons(true);
		dpiHelper.setFixedSize(buttonBox->button(QDialogButtonBox::Ok), {128, 40});
		dpiHelper.setFixedSize(buttonBox->button(QDialogButtonBox::Cancel), {128, 40});
		buttonBox->button(QDialogButtonBox::Cancel)->setObjectName(PROPERTY_EDITITEM_BTNBOXCANCEL);

		mainLayout->addLayout(topLayout);
		mainLayout->addWidget(buttonBox);

		this->content()->setLayout(mainLayout);

		connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
		connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	}

	inline QString GetText() const { return edit->text(); }
};

void WidgetInfo::EditListAdd()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:add", ACTION_CLICK);

	enum obs_editable_list_type type = obs_property_editable_list_type(property);

	if (type == OBS_EDITABLE_LIST_TYPE_STRINGS) {
		EditListAddText();
		return;
	}

	/* Files and URLs */
	QMenu popup(view->window());

	QAction *action;

	action = new QAction(QTStr("Basic.PropertiesWindow.AddFiles"), this);
	connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddFiles);
	popup.addAction(action);

	action = new QAction(QTStr("Basic.PropertiesWindow.AddDir"), this);
	connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddDir);
	popup.addAction(action);

	if (type == OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS) {
		action = new QAction(QTStr("Basic.PropertiesWindow.AddURL"), this);
		connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddText);
		popup.addAction(action);
	}

	popup.exec(QCursor::pos());
}

void WidgetInfo::EditListAddText()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:addText", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);

	EditableItemDialog dialog(widget->window(), QString(), false);
	auto title = QTStr("Basic.PropertiesWindow.AddEditableListEntry").arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	list->addItem(text);
	EditableListChanged();
}

void WidgetInfo::EditListAddFiles()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:addFiles", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	const char *default_path = obs_property_editable_list_default_path(property);

	QString title = QTStr("Basic.PropertiesWindow.AddEditableListFiles").arg(QT_UTF8(desc));

	QStringList files = QFileDialog::getOpenFileNames(App()->GetMainWindow(), title, QT_UTF8(default_path), QT_UTF8(filter));

	if (files.count() == 0)
		return;

	list->addItems(files);
	EditableListChanged();
}

void WidgetInfo::EditListAddDir()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:addDir", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *default_path = obs_property_editable_list_default_path(property);

	QString title = QTStr("Basic.PropertiesWindow.AddEditableListDir").arg(QT_UTF8(desc));

	QString dir = QFileDialog::getExistingDirectory(App()->GetMainWindow(), title, QT_UTF8(default_path));

	if (dir.isEmpty())
		return;

	PLS_INFO(PROPERTY_MODULE, "PropertyOperation addDir.");

	list->addItem(dir);
	EditableListChanged();
}

void WidgetInfo::EditListRemove()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:remove", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	QList<QListWidgetItem *> items = list->selectedItems();

	for (QListWidgetItem *item : items)
		delete item;
	EditableListChanged();
}

void WidgetInfo::EditListEdit()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:edit", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	enum obs_editable_list_type type = obs_property_editable_list_type(property);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	QList<QListWidgetItem *> selectedItems = list->selectedItems();

	if (!selectedItems.count())
		return;

	QListWidgetItem *item = selectedItems[0];

	if (type == OBS_EDITABLE_LIST_TYPE_FILES) {
		QDir pathDir(item->text());
		QString path;

		if (pathDir.exists())
			path = QFileDialog::getExistingDirectory(App()->GetMainWindow(), QTStr("Browse"), item->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		else
			path = QFileDialog::getOpenFileName(App()->GetMainWindow(), QTStr("Browse"), item->text(), QT_UTF8(filter));

		if (path.isEmpty())
			return;

		item->setText(path);
		EditableListChanged();
		return;
	}

	EditableItemDialog dialog(widget->window(), item->text(), type != OBS_EDITABLE_LIST_TYPE_STRINGS, filter);
	auto title = QTStr("Basic.PropertiesWindow.EditEditableListEntry").arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	item->setText(text);
	EditableListChanged();
}

void WidgetInfo::EditListUp()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:moveUp", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = -1;

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row - 1) != lastItemRow) {
			lastItemRow = row - 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}

void WidgetInfo::EditListDown()
{
	PLS_UI_STEP(PROPERTY_MODULE, "property-window:editlist:moveDown", ACTION_CLICK);

	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = list->count();

	for (int i = list->count() - 1; i >= 0; i--) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row + 1) != lastItemRow) {
			lastItemRow = row + 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}

bool WidgetInfo::eventFilter(QObject *target, QEvent *event)
{
	if (target == widget && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
		OBSSource source = pls_get_source_by_pointer_address(view->obj);
		const char *setting = obs_property_name(property);
		if (source && setting) {
			const char *id = obs_source_get_id(source);
			bool is_local_file = (0 == strcmp(id, MEDIA_SOURCE_ID) && QT_UTF8(setting) == "is_local_file");
			bool is_checkBox = (obs_property_get_type(property) == OBS_PROPERTY_BOOL);

			obs_data_t *loadData = obs_data_create();
			obs_data_set_string(loadData, "method", "media_load");
			obs_source_get_private_data(source, loadData);
			bool loading = obs_data_get_bool(loadData, "media_load");
			obs_data_release(loadData);

			if (is_local_file && is_checkBox && loading) {
				QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
				if (mouseEvent->button() == Qt::LeftButton) {
					PLS_INFO(PROPERTY_MODULE, "media is loading or reading, switch local file is disable.");
					return true;
				}
			}
		}
	}

	return QObject::eventFilter(target, event);
}
