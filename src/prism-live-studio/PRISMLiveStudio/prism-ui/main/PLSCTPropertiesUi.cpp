#include "PLSPropertiesView.hpp"
#include "PLSPropertiesExtraUI.hpp"
#include <QFormLayout>
#include "PLSRadioButton.h"
#include "pls-common-define.hpp"
#include "PLSSpinBox.h"
#include "ChannelCommonFunctions.h"
#include "flowlayout.h"
#include "qbuttongroup.h"
#include "log/log.h"
#include "log/module_names.h"
#include "frontend-api.h"
#include "PLSComboBox.h"
#include <QColorDialog>
#include "PLSColorDialogView.h"
#include "qt-wrappers.hpp"
#include "PLSBasic.h"

const int ctOffset = 5;

extern bool _isValidComparedObj(const QObject *obj, const char *keyStr, const QString &objName);

void PLSWidgetInfo::CTFontChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ctFontColorObj = obs_data_get_obj(view->settings, setting);
	obs_data_t *newCtFontColorObj = obs_data_create();

	obs_data_set_string(newCtFontColorObj, "chatFontFamily", obs_data_get_string(ctFontColorObj, "chatFontFamily"));
	obs_data_set_string(newCtFontColorObj, "chatFontStyle", obs_data_get_string(ctFontColorObj, "chatFontStyle"));
	obs_data_set_bool(newCtFontColorObj, "isEnableChatCommonFont", obs_data_get_bool(ctFontColorObj, "isEnableChatCommonFont"));
	obs_data_set_int(newCtFontColorObj, "chatFontSize", obs_data_get_int(ctFontColorObj, "chatFontSize"));
	obs_data_set_bool(newCtFontColorObj, "isEnableChatFontSize", obs_data_get_bool(ctFontColorObj, "isEnableChatFontSize"));
	obs_data_set_int(newCtFontColorObj, "chatFontOutlineSize", obs_data_get_int(ctFontColorObj, "chatFontOutlineSize"));
	obs_data_set_int(newCtFontColorObj, "chatFontOutlineColor", obs_data_get_int(ctFontColorObj, "chatFontOutlineColor"));
	obs_data_set_bool(newCtFontColorObj, "isEnableChatFontOutlineColor", obs_data_get_bool(ctFontColorObj, "isEnableChatFontOutlineColor"));
	obs_data_set_bool(newCtFontColorObj, "isEnableChatFontOutlineSize", obs_data_get_bool(ctFontColorObj, "isEnableChatFontOutlineSize"));
	obs_data_set_bool(newCtFontColorObj, "isEnableChatFont", obs_data_get_bool(ctFontColorObj, "isEnableChatFont"));

	auto objName = object->objectName();
	auto list = object->dynamicPropertyNames();
	if ("tmFontBox" == objName) {
		QString currentFamily(static_cast<PLSComboBox *>(object)->currentText());
		QStringList styles(QFontDatabase::styles(currentFamily));
		QString weight;
		if (!styles.isEmpty()) {
			auto currentFailyData = currentFamily.toUtf8();
			if (pls_is_equal(currentFailyData.constData(), "NanumGothic") || pls_is_equal(currentFailyData.constData(), "NanumMyeongjo"))
				weight = "ExtraBold";
			else if (pls_is_equal(currentFailyData.constData(), "S-Core Dream"))
				weight = "5 Medium";
			else
				weight = styles.first();
		}
		obs_data_set_string(newCtFontColorObj, "chatFontFamily", currentFamily.toUtf8());
		obs_data_set_string(newCtFontColorObj, "chatFontStyle", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontFamily ", ACTION_CLICK);

	} else if ("tmFontStyleBox" == objName) {
		QString weight(static_cast<PLSComboBox *>(object)->currentText());
		obs_data_set_string(newCtFontColorObj, "chatFontStyle", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontStyle ", ACTION_CLICK);

	} else if (_isValidComparedObj(object, "index", "spinBox")) {

		switch (object->property("index").toInt()) {
		case 0: //outlineSizechange
			obs_data_set_int(newCtFontColorObj, "chatFontOutlineSize", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontOutlineSize ", ACTION_CLICK);

			break;
		case 1: //fontsizechange
			obs_data_set_int(newCtFontColorObj, "chatFontSize", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontSize ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (_isValidComparedObj(object, "index", "slider")) {
		switch (object->property("index").toInt() - ctOffset) {
		case 0: //outlineSizechange
			obs_data_set_int(newCtFontColorObj, "chatFontOutlineSize", static_cast<QSlider *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontOutlineSize ", ACTION_CLICK);

			break;
		case 1: //fontsizechange
			obs_data_set_int(newCtFontColorObj, "chatFontSize", static_cast<QSlider *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatFontSize ", ACTION_CLICK);

			break;
		default:
			break;
		}
	} else {
		QColorDialog::ColorDialogOptions options;
		auto label = static_cast<QLabel *>(widget);
		const char *desc = obs_property_description(property);
		QColor color = PLSColorDialogView::getColor(label->text(), view->parentWidget(), QT_UTF8(desc), options);
		color.setAlpha(255);
		if (!color.isValid()) {
			obs_data_release(newCtFontColorObj);
			return;
		}

		label->setText(color.name(QColor::HexRgb));
		auto palette = QPalette(color);
		label->setPalette(palette);
		label->setStyleSheet(QString("font-weight: normal;background-color :%1; color: %2;")
					     .arg(palette.color(QPalette::Window).name(QColor::HexRgb))
					     .arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

		obs_data_set_int(view->settings, setting, pls_qcolor_to_qint64(color));
		switch (label->property("index").toInt()) {
		case 0:
			obs_data_set_int(newCtFontColorObj, "chatFontOutlineColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:nickTextDefaultColor", ACTION_CLICK);
			break;
		default:
			break;
		}
	}
	obs_data_set_obj(view->settings, setting, newCtFontColorObj);
	obs_data_release(ctFontColorObj);
	obs_data_release(newCtFontColorObj);
}
void PLSWidgetInfo::CTTextColorChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ctTextColorObj = obs_data_get_obj(view->settings, setting);
	obs_data_t *newctTextColorObj = obs_data_create();
	obs_data_set_bool(newctTextColorObj, "isCheckNickTextSingleColor", obs_data_get_bool(ctTextColorObj, "isCheckNickTextSingleColor"));
	obs_data_set_int(newctTextColorObj, "nickTextDefaultColor", obs_data_get_int(ctTextColorObj, "nickTextDefaultColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableSingleNickTextColor", obs_data_get_bool(ctTextColorObj, "isEnableSingleNickTextColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableRadomNickTextColor", obs_data_get_bool(ctTextColorObj, "isEnableRadomNickTextColor"));
	obs_data_set_int(newctTextColorObj, "mgrNickTextColor", obs_data_get_int(ctTextColorObj, "mgrNickTextColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableMgrNickTextColor", obs_data_get_bool(ctTextColorObj, "isEnableMgrNickTextColor"));
	obs_data_set_int(newctTextColorObj, "subcribeTextColor", obs_data_get_int(ctTextColorObj, "subcribeTextColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableSubcribeTextColor", obs_data_get_bool(ctTextColorObj, "isEnableSubcribeTextColor"));
	obs_data_set_int(newctTextColorObj, "messageTextColor", obs_data_get_int(ctTextColorObj, "messageTextColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableMessageTextColor", obs_data_get_bool(ctTextColorObj, "isEnableMessageTextColor"));
	obs_data_set_bool(newctTextColorObj, "isEnableChatTextColor", obs_data_get_bool(ctTextColorObj, "isEnableChatTextColor"));

	auto objName = object->objectName();
	auto list = object->dynamicPropertyNames();
	if (objName == "commonNickGroup") {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_bool(newctTextColorObj, "isCheckNickTextSingleColor", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:color-tab ", ACTION_CLICK);
	} else {
		QColorDialog::ColorDialogOptions options;
		auto label = static_cast<QLabel *>(widget);
		const char *desc = obs_property_description(property);
		QColor color = PLSColorDialogView::getColor(label->text(), view->parentWidget(), QT_UTF8(desc), options);
		color.setAlpha(255);
		if (!color.isValid()) {
			obs_data_release(newctTextColorObj);
			return;
		}

		label->setText(color.name(QColor::HexRgb));
		auto palette = QPalette(color);
		label->setPalette(palette);
		label->setStyleSheet(QString("font-weight: normal;background-color :%1; color: %2;")
					     .arg(palette.color(QPalette::Window).name(QColor::HexRgb))
					     .arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

		obs_data_set_int(view->settings, setting, pls_qcolor_to_qint64(color));
		switch (label->property("index").toInt()) {
		case 0:
			obs_data_set_int(newctTextColorObj, "nickTextDefaultColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:nickTextDefaultColor", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(newctTextColorObj, "mgrNickTextColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkColor", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(newctTextColorObj, "subcribeTextColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkColor", ACTION_CLICK);

			break;
		case 3:
			obs_data_set_int(newctTextColorObj, "messageTextColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkColor", ACTION_CLICK);

			break;
		default:
			break;
		}
	}
	obs_data_set_obj(view->settings, setting, newctTextColorObj);
	obs_data_release(ctTextColorObj);
	obs_data_release(newctTextColorObj);
}
void PLSWidgetInfo::CTBkColorChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ctBkColorObj = obs_data_get_obj(view->settings, setting);
	obs_data_t *newCtBkColorObj = obs_data_create();
	obs_data_set_int(newCtBkColorObj, "chatSingleBkColor", obs_data_get_int(ctBkColorObj, "chatSingleBkColor"));
	obs_data_set_int(newCtBkColorObj, "chatSingleBkAlpha", obs_data_get_int(ctBkColorObj, "chatSingleBkAlpha"));
	obs_data_set_int(newCtBkColorObj, "chatSingleColorStyle", obs_data_get_int(ctBkColorObj, "chatSingleColorStyle"));
	obs_data_set_bool(newCtBkColorObj, "isCheckChatSingleBkColor", obs_data_get_bool(ctBkColorObj, "isCheckChatSingleBkColor"));
	obs_data_set_bool(newCtBkColorObj, "isEnableChatSingleBkColor", obs_data_get_bool(ctBkColorObj, "isEnableChatSingleBkColor"));
	obs_data_set_int(newCtBkColorObj, "chatTotalBkColor", obs_data_get_int(ctBkColorObj, "chatTotalBkColor"));
	obs_data_set_int(newCtBkColorObj, "chatTotalBkAlpha", obs_data_get_int(ctBkColorObj, "chatTotalBkAlpha"));
	obs_data_set_bool(newCtBkColorObj, "isCheckChatTotalBkColor", obs_data_get_bool(ctBkColorObj, "isCheckChatTotalBkColor"));
	obs_data_set_bool(newCtBkColorObj, "isEnableChatTotalBkColor", obs_data_get_bool(ctBkColorObj, "isEnableChatTotalBkColor"));
	obs_data_set_int(newCtBkColorObj, "chatWindowAlpha", obs_data_get_int(ctBkColorObj, "chatWindowAlpha"));
	obs_data_set_bool(newCtBkColorObj, "isEnableChatBackgroundColor", obs_data_get_bool(ctBkColorObj, "isEnableChatBackgroundColor"));

	auto objName = object->objectName();
	auto list = object->dynamicPropertyNames();
	if (_isValidComparedObj(object, "index", "spinBox")) {

		switch (object->property("index").toInt() - ctOffset) {
		case 0:
			obs_data_set_int(newCtBkColorObj, "chatSingleBkAlpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatSingleBkAlpha ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(newCtBkColorObj, "chatTotalBkAlpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkAlpha ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(newCtBkColorObj, "chatWindowAlpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatWindowAlpha", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (_isValidComparedObj(object, "index", "slider")) {
		switch (object->property("index").toInt() - ctOffset) {
		case 0:
			obs_data_set_int(newCtBkColorObj, "chatSingleBkAlpha", static_cast<QSlider *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatSingleBkAlpha ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(newCtBkColorObj, "chatTotalBkAlpha", static_cast<QSlider *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkAlpha ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(newCtBkColorObj, "chatWindowAlpha", static_cast<QSlider *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatWindowAlpha", ACTION_CLICK);

			break;
		default:
			break;
		}
	} else if (_isValidComparedObj(object, "index", "checkBox") && object->property("index").toInt() == 1) {
		obs_data_set_bool(newCtBkColorObj, "isCheckChatTotalBkColor", static_cast<PLSCheckBox *>(object)->isChecked());
	} else if (objName == "singleBkGroup") {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(newCtBkColorObj, "chatSingleColorStyle", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:color-tab ", ACTION_CLICK);
	} else {
		QColorDialog::ColorDialogOptions options;
		auto label = static_cast<QLabel *>(widget);
		const char *desc = obs_property_description(property);
		QColor color = PLSColorDialogView::getColor(label->text(), view->parentWidget(), QT_UTF8(desc), options);
		color.setAlpha(255);
		if (!color.isValid()) {
			obs_data_release(newCtBkColorObj);
			return;
		}

		label->setText(color.name(QColor::HexRgb));
		auto palette = QPalette(color);
		label->setPalette(palette);
		label->setStyleSheet(QString("font-weight: normal;background-color :%1; color: %2;")
					     .arg(palette.color(QPalette::Window).name(QColor::HexRgb))
					     .arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

		obs_data_set_int(view->settings, setting, pls_qcolor_to_qint64(color));
		switch (label->property("index").toInt()) {
		case 0:
			obs_data_set_int(newCtBkColorObj, "chatSingleBkColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatSingleBkColor ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(newCtBkColorObj, "chatTotalBkColor", pls_qcolor_to_qint64(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:chatTotalBkColor", ACTION_CLICK);

			break;
		default:
			break;
		}
	}
	obs_data_set_obj(view->settings, setting, newCtBkColorObj);
	obs_data_release(ctBkColorObj);
	obs_data_release(newCtBkColorObj);
}
QHBoxLayout *PLSPropertiesView::createColorButtonNoSlider(obs_property_t *prop, long long colorValue, int alaphValue, int index)
{
	QHBoxLayout *layout = new QHBoxLayout();
	auto button = pls_new<QPushButton>();
	button->setObjectName("textColorBtn");
	auto colorLabel = pls_new<QLabel>();
	colorLabel->setProperty("index", index);
	colorLabel->setObjectName("colorLabel");
	colorLabel->setAlignment(Qt::AlignCenter);

	layout->setAlignment(Qt::AlignLeft);
	layout->addWidget(colorLabel);
	layout->addWidget(button);
	layout->setSpacing(0);

	setLabelColor(colorLabel, colorValue, alaphValue);
	auto info = pls_new<PLSWidgetInfo>(this, prop, colorLabel);
	connect(button, &QPushButton::clicked, info, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(info);
	return layout;
}
void PLSPropertiesView::AddCtFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);
	auto keys = [](const QList<QPair<QString, QString>> fontList) -> QStringList {
		QStringList fontStrs;
		for (auto fontStr : fontList) {
			fontStrs.append(fontStr.first);
		}
		return fontStrs;
	};
	auto family = obs_data_get_string(val, "chatFontFamily");
	auto labelFont = pls_new<QLabel>(tr("ChatTemplate.Default.Font"));
	labelFont->setObjectName("subLabel");
	auto defaultFontWidget = pls_new<FontSelectionWindow>(pls_get_chat_template_helper_instance()->getChatCustomDefaultFamily(), family);
	flayout->addRow(labelFont, defaultFontWidget);

	auto hlayout1 = pls_new<QHBoxLayout>();
	hlayout1->setContentsMargins(0, 0, 0, 0);
	hlayout1->setSpacing(10);

	auto fontCbx = pls_new<PLSComboBox>();
	fontCbx->setObjectName("tmFontBox");
#if defined(Q_OS_WIN)
	const char *ko_en_family = "Malgun Gothic";
	const char *ko_ko_family = "\353\247\221\354\235\200 \352\263\240\353\224\225";
	if (!strcmp(family, ko_en_family) || !strcmp(family, ko_ko_family)) {
		if (QLocale::system().language() == QLocale::Korean) {
			family = ko_ko_family;
		} else {
			family = ko_en_family;
		}
	}
#endif
	fontCbx->addItem(family);
	fontCbx->setCurrentText(family);
	auto weightCbx = pls_new<PLSComboBox>();
	updateFontSytle(family, weightCbx);
	weightCbx->setCurrentText(obs_data_get_string(val, "chatFontStyle"));

	weightCbx->setObjectName("tmFontStyleBox");

	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), [this, weightCbx](const QString &text) { updateFontSytle(text, weightCbx); });
	connect(defaultFontWidget, &FontSelectionWindow::clickFontBtn, fontCbx, [fontCbx](QAbstractButton *button) {
		if (button) {
			fontCbx->setCurrentText(button->property("qtFamily").toString());
		}
	});

	QMetaObject::invokeMethod(
		fontCbx,
		[family, fontCbx, weightCbx, this]() {
			QSignalBlocker block(fontCbx);
			fontCbx->clear();
			fontCbx->addItems(getFilteredFontFamilies());
			PLS_INFO(PROPERTY_MODULE, "current language = %s", family);
			if (family[0] == '\0' || family == nullptr) {
				auto fontStyle = fontCbx->currentText();
				updateFontSytle(fontStyle, weightCbx);
				return;
			}
			fontCbx->setCurrentText(family);
		},
		Qt::QueuedConnection);
	hlayout1->addWidget(fontCbx);
	hlayout1->addWidget(weightCbx);
	hlayout1->setStretch(0, 292);
	hlayout1->setStretch(1, 210);

	flayout->addRow(new QLabel(), hlayout1);

	auto fontWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, fontCbx);
	connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), fontWidgetInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(fontWidgetInfo);

	auto fontStyleWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, weightCbx);
	connect(weightCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), fontStyleWidgetInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(fontStyleWidgetInfo);

	auto hlayout3 = pls_new<QHBoxLayout>();
	hlayout3->setContentsMargins(0, 0, 0, 0);
	hlayout3->setSpacing(20);

	int minVal = 6;
	int maxVal = 72;
	int stepVal = 1;
	auto fontSize = (int)obs_data_get_int(val, "chatFontSize");

	createTMSlider(prop, 1, minVal, maxVal, stepVal, fontSize, hlayout3, false, false, false, "");
	auto fontSizeLabel = pls_new<QLabel>(tr("textmotion.size"));
	fontSizeLabel->setObjectName("subLabel");
	flayout->addRow(fontSizeLabel, hlayout3);
	m_tmLabels.append(fontSizeLabel);

	auto glayout = pls_new<QGridLayout>();
	glayout->setHorizontalSpacing(20);
	glayout->setVerticalSpacing(10);

	auto insetRow = glayout->rowCount() - 1;
	auto textColorLayout = pls_new<QHBoxLayout>();
	textColorLayout->setContentsMargins(0, 0, 0, 0);
	textColorLayout->setSpacing(20);
	auto textColorLabel = pls_new<QLabel>(tr("ChatTemplate.Outline"));
	textColorLabel->setObjectName("subLabel");
	textColorLabel->setEnabled(obs_data_get_bool(val, "isEnableChatFontOutlineColor"));
	glayout->addWidget(textColorLabel, insetRow, 0);
	m_tmLabels.append(textColorLabel);

	PLSCheckBox *ChecBox = nullptr;
	pls_used(ChecBox);
	createColorButton(prop, glayout, ChecBox, tr("ChatTemplate.Outline.Size"), 0, false, true);

	flayout->addRow(glayout);

	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}
void PLSPropertiesView::AddCtTextColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);
	auto nickCommonLabel = pls_new<QLabel>(tr("ChatTemplate.Common.Nick"));
	nickCommonLabel->setObjectName("subLabel");
	m_tmLabels.append(nickCommonLabel);
	QHBoxLayout *hLayout = pls_new<QHBoxLayout>();
	hLayout->setSpacing(25);
	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->setContentsMargins(0, 0, 0, 0);
	auto frame = pls_new<QFrame>();
	frame->setFrameStyle(QFrame::NoFrame);
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame->setLayout(hLayout);
	bool isCheckNickTextColor = obs_data_get_bool(val, "isCheckNickTextSingleColor");
	bool isEnableSingleNickTextColor = obs_data_get_bool(val, "isEnableSingleNickTextColor");
	bool isEnableRadomNickTextColor = obs_data_get_bool(val, "isEnableRadomNickTextColor");

	auto commonNickGroup = pls_new<PLSRadioButtonGroup>();
	commonNickGroup->setObjectName("commonNickGroup");
	createRadioButton(2, val, hLayout, commonNickGroup, {tr("Radom.Color"), tr("Single.Color")}, true, frame);
	commonNickGroup->button(isCheckNickTextColor)->setChecked(true);

	auto alignInfo = pls_new<PLSWidgetInfo>(this, prop, commonNickGroup);
	connect(commonNickGroup, &PLSRadioButtonGroup::idClicked, alignInfo, &PLSWidgetInfo::ControlChanged);

	auto subHlayout = createColorButtonNoSlider(prop, obs_data_get_int(val, "nickTextDefaultColor"), 255, 0);
	setLayoutEnable(subHlayout, isCheckNickTextColor);
	connect(commonNickGroup, &PLSRadioButtonGroup::idClicked, [subHlayout, this](int id) { setLayoutEnable(subHlayout, id == 1); });
	if (auto btn = commonNickGroup->button(0))
		btn->setEnabled(isEnableRadomNickTextColor);
	if (auto btn = commonNickGroup->button(1))
		btn->setEnabled(isEnableSingleNickTextColor);
	hLayout->addLayout(subHlayout);
	flayout->addRow(nickCommonLabel, frame);

	auto nickMgrLabel = pls_new<QLabel>(tr("ChatTemplate.Mgr.Nick"));
	nickMgrLabel->setObjectName("subLabelWithTips");
	auto nickMgrLabelWithHelp = plsCreateHelpQWidget(nickMgrLabel, tr("ChatTemplate.Mgr.Nick.Tooltip"), "ChatV2_MoveDown", true);
	nickMgrLabelWithHelp->setObjectName("subLabel");

	m_tmLabels.append(nickMgrLabel);
	auto mgrColorLayout = createColorButtonNoSlider(prop, obs_data_get_int(val, "mgrNickTextColor"), 255, 1);
	setLayoutEnable(mgrColorLayout, obs_data_get_bool(val, "isEnableMgrNickTextColor"));
	flayout->addRow(nickMgrLabelWithHelp, mgrColorLayout);

	auto subcirbeTextLabel = pls_new<QLabel>(tr("ChatTemplate.Subcribe.Text"));
	subcirbeTextLabel->setObjectName("subLabelWithTips");
	auto subcirbeTextLabelWithHelp = plsCreateHelpQWidget(subcirbeTextLabel, tr("ChatTemplate.Subcribe.Text.Tooltip"), "ChatV2_MoveDown", true);
	subcirbeTextLabelWithHelp->setObjectName("subLabel");
	m_tmLabels.append(subcirbeTextLabel);
	auto subribeLayout = createColorButtonNoSlider(prop, obs_data_get_int(val, "subcribeTextColor"), 255, 2);
	setLayoutEnable(subribeLayout, obs_data_get_bool(val, "isEnableSubcribeTextColor"));

	flayout->addRow(subcirbeTextLabelWithHelp, subribeLayout);

	auto messageTextLabel = pls_new<QLabel>(tr("ChatTemplate.Message.Text"));
	messageTextLabel->setObjectName("subLabel");
	messageTextLabel->setWordWrap(true);
	m_tmLabels.append(messageTextLabel);
	auto messageLayout = createColorButtonNoSlider(prop, obs_data_get_int(val, "messageTextColor"), 255, 3);
	setLayoutEnable(messageLayout, obs_data_get_bool(val, "isEnableMessageTextColor"));

	flayout->addRow(messageTextLabel, messageLayout);

	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 23, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}
void PLSPropertiesView::AddCtBkColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);

	auto glayout = pls_new<QGridLayout>();
	glayout->setHorizontalSpacing(20);
	glayout->setVerticalSpacing(10);

	PLSCheckBox *ChecBox = nullptr;
	pls_used(ChecBox);
	auto colorStyleIndex = obs_data_get_int(val, "chatSingleColorStyle");
	auto textColorLabel = pls_new<QLabel>(tr("ChatTemplate.Single.Bk.Color"));
	textColorLabel->setObjectName("subLabel");
	textColorLabel->setWordWrap(true);
	if (colorStyleIndex == -1) {
		auto insetRow = glayout->rowCount() - 1;
		auto textColorLayout = pls_new<QHBoxLayout>();
		textColorLayout->setContentsMargins(0, 0, 0, 0);
		textColorLayout->setSpacing(20);
		glayout->addWidget(textColorLabel, insetRow, 0);
		m_tmLabels.append(textColorLabel);
		textColorLabel->setEnabled(obs_data_get_bool(val, "isEnableChatSingleBkColor"));
		createColorButton(prop, glayout, ChecBox, tr("textmotion.opacity"), 0, true, true, ctOffset);
	} else {

		QHBoxLayout *hLayout = pls_new<QHBoxLayout>();
		hLayout->setSpacing(25);
		hLayout->setAlignment(Qt::AlignLeft);
		hLayout->setContentsMargins(0, 0, 0, 0);
		auto frame = pls_new<QFrame>();
		frame->setFrameStyle(QFrame::NoFrame);
		frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		frame->setLayout(hLayout);
		auto singleBkGroup = pls_new<PLSRadioButtonGroup>();
		singleBkGroup->setObjectName("singleBkGroup");
		createRadioButton(2, val, hLayout, singleBkGroup, {tr("Pastel.Color"), tr("Bibid.Color")}, true, frame);

		singleBkGroup->button(colorStyleIndex)->setChecked(true);

		auto alignInfo = pls_new<PLSWidgetInfo>(this, prop, singleBkGroup);
		connect(singleBkGroup, &PLSRadioButtonGroup::idClicked, alignInfo, &PLSWidgetInfo::ControlChanged);
		flayout->addRow(textColorLabel, frame);
	}

	auto currentIndex = glayout->rowCount();
	PLSCheckBox *bkControlChecBox = nullptr;
	auto bkColorLayout = pls_new<QHBoxLayout>();
	bkColorLayout->setContentsMargins(0, 0, 0, 0);
	bkColorLayout->setSpacing(20);
	auto bkColorLabelFrame = pls_new<QFrame>();
	bkColorLabelFrame->setObjectName("ctColorFrame");
	createTMColorCheckBox(bkControlChecBox, prop, bkColorLabelFrame, currentIndex, tr("ChatTemplate.Total.Bk"), bkColorLayout, obs_data_get_bool(val, "isCheckChatTotalBkColor"),
			      obs_data_get_bool(val, "is-bk-init-color-on"));
	glayout->addWidget(bkColorLabelFrame, currentIndex, 0);
	createColorButton(prop, glayout, bkControlChecBox, tr("textmotion.opacity"), currentIndex, true, true, ctOffset);
	flayout->addRow(glayout);

	auto hlayout3 = pls_new<QHBoxLayout>();
	hlayout3->setContentsMargins(0, 0, 0, 0);
	hlayout3->setSpacing(20);
	auto windowAlpha = (int)obs_data_get_int(val, "chatWindowAlpha");
	createTMSlider(prop, 2, 0, 100, 1, windowAlpha, hlayout3, false, true, false, QString(), ctOffset);

	auto windowLabel = pls_new<QLabel>(tr("ChatTemplate.Window.Alpha"));
	windowLabel->setObjectName("subLabel");
	windowLabel->setWordWrap(true);
	flayout->addRow(windowLabel, hlayout3);
	m_tmLabels.append(windowLabel);

	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 23, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddCtDisplay(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	bool isEnablePlatformType = obs_data_get_bool(val, "isEnablePlatformType");
	bool isEnablePlatformIcon = obs_data_get_bool(val, "isEnablePlatformIcon");
	bool isCheckPlatformIcon = obs_data_get_bool(val, "isCheckPlatformIcon");
	bool isEnableLevelIcon = obs_data_get_bool(val, "isEnableLevelIcon");
	bool isCheckLevelIcon = obs_data_get_bool(val, "isCheckLevelIcon");
	bool isEnableIdIcon = obs_data_get_bool(val, "isEnableIdIcon");
	bool isCheckIdIcon = obs_data_get_bool(val, "isCheckIdIcon");
	bool isEnablePlatformIconDisplay = obs_data_get_bool(val, "isEnablePlatformIconDisplay");
	bool isEnableChatDisplay = obs_data_get_bool(val, "isEnableChatDisplay");
	QString selectPlatform = obs_data_get_string(val, "selectPlatformList");
	auto selectPlatformList = selectPlatform.split(';');

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(0);

	auto platformTypeLabel = pls_new<QLabel>(tr("ChatTemplate.display.platformType"));
	auto platformTypehelp = plsCreateHelpQWidget(platformTypeLabel, tr("ChatTemplate.display.platformType.tooltip"), "ChatV2_MoveDown", true);
	platformTypehelp->setObjectName("platformTypehelp");
	auto flowLayout = pls_new<FlowLayout>(0, 25, 15);
	flowLayout->setFixWidthForCalculate(513);
	auto frame = pls_new<QFrame>();
	frame->setFrameStyle(QFrame::NoFrame);
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame->setLayout(flowLayout);
	frame->setObjectName("CTChatPlatformTypeFrame");
	m_platfromCheckBoxs.clear();
	auto list = getChatChannelNameList();
	for (auto &name : list) {
		QString path = getPlatformImageFromName(name, 0);
		QPixmap pix;
		loadPixmap(pix, path, {72, 72});
		QString translateName = translatePlatformName(name);
		auto checkbox = pls_new<PLSCheckBox>(pix, translateName, false);
		checkbox->setObjectName("CTChatPlatformCheck");
		if (pls_is_ncp(name)) {
			name.insert(0, "NCP_");
		}
		checkbox->setProperty("platformName", name);
		checkbox->setSpac(5);
		if (selectPlatformList.contains(name)) {
			checkbox->setChecked(true);
		} else {
			checkbox->setChecked(false);
		}
		flowLayout->addWidget(checkbox);
		m_platfromCheckBoxs.append(checkbox);
		auto platformTypeInfo = pls_new<PLSWidgetInfo>(this, prop, checkbox);
		connect(checkbox, &PLSCheckBox::clicked, platformTypeInfo, &PLSWidgetInfo::ControlChanged);
		children.emplace_back(platformTypeInfo);
	}
	flowLayout->showLayoutItemWidget();
	setLayoutEnable(flowLayout, isEnablePlatformType);
	flayout->addRow(platformTypehelp, frame);
	flayout->addItem(pls_new<QSpacerItem>(10, 37, QSizePolicy::Fixed, QSizePolicy::Fixed));

	auto iconDisplayLabel = pls_new<QLabel>(tr("ChatTemplate.display.icon.display"));
	iconDisplayLabel->setWordWrap(true);
	iconDisplayLabel->setObjectName("subLabel");
	auto hlayout = pls_new<QHBoxLayout>();
	hlayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(25);

	auto frame2 = pls_new<QFrame>();
	frame2->setFrameStyle(QFrame::NoFrame);
	frame2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame2->setLayout(hlayout);
	frame2->setObjectName("CTChatPlatformIconFrame");

	auto platformIconCheck = pls_new<PLSCheckBox>(tr("ChatTemplate.display.platformIcon"));
	platformIconCheck->setObjectName("platformIconCheck");
	platformIconCheck->setSpac(5);
	hlayout->addWidget(platformIconCheck);
	platformIconCheck->setEnabled(isEnablePlatformIcon);
	platformIconCheck->setChecked(isCheckPlatformIcon);
	auto platformIconInfo = pls_new<PLSWidgetInfo>(this, prop, platformIconCheck);
	connect(platformIconCheck, &PLSCheckBox::clicked, platformIconInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(platformIconInfo);

	auto levelIconCheck = pls_new<PLSCheckBox>(tr("ChatTemplate.display.levelIcon"));
	levelIconCheck->setObjectName("levelIconCheck");
	levelIconCheck->setSpac(5);
	levelIconCheck->setEnabled(isEnableLevelIcon);
	levelIconCheck->setChecked(isCheckLevelIcon);
	auto levelIconWidget = plsCreateHelpQWidget(levelIconCheck, tr("ChatTemplate.display.levelIcon.tooltip"), "ChatV2_MoveDown", true);
	levelIconWidget->setObjectName("levelIconWidget");
	levelIconWidget->setEnabled(isEnableLevelIcon);
	hlayout->addWidget(levelIconWidget);
	auto levelIconInfo = pls_new<PLSWidgetInfo>(this, prop, levelIconCheck);
	connect(levelIconCheck, &PLSCheckBox::clicked, levelIconInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(levelIconInfo);

	auto idIconCheck = pls_new<PLSCheckBox>(tr("ChatTemplate.display.idIcon"));
	idIconCheck->setObjectName("idIconCheck");
	idIconCheck->setSpac(5);
	hlayout->addWidget(idIconCheck);
	idIconCheck->setEnabled(isEnableIdIcon);
	idIconCheck->setChecked(isCheckIdIcon);
	auto idIconInfo = pls_new<PLSWidgetInfo>(this, prop, idIconCheck);
	connect(idIconCheck, &PLSCheckBox::clicked, idIconInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(idIconInfo);

	flayout->addRow(iconDisplayLabel, frame2);
	setLayoutEnable(hlayout, isEnablePlatformIconDisplay);
	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(pls_new<QSpacerItem>(10, 30, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);
	setLayoutEnable(flayout, isEnableChatDisplay);
	layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(label);
	layout->addItem(pls_new<QSpacerItem>(10, 28, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}
void PLSPropertiesView::AddCtOptions(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);

	bool isEnableWrap = obs_data_get_bool(val, "isEnableChatWrap");
	bool isCheckWrap = obs_data_get_bool(val, "isCheckChatWrap");
	bool isLeftAlign = obs_data_get_bool(val, "isCheckLeftChatAlign");
	bool isEnableAlign = obs_data_get_bool(val, "isEnableChatAlign");
	bool isCheckDisapper = obs_data_get_bool(val, "isCheckChatDisapperEffect");
	bool isEnableDisapper = obs_data_get_bool(val, "isEnableChatDisapperEffect");
	bool isEnableSize = obs_data_get_bool(val, "isEnableChatBoxSize");
	int chatWidth = obs_data_get_int(settings, "chatWidth");
	int chatHeight = obs_data_get_int(settings, "chatHeight");
	bool isEnableOption = obs_data_get_bool(val, "isEnableChatOption");

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(0);

	auto hlayout = pls_new<QHBoxLayout>();
	hlayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(25);

	auto group = pls_new<PLSRadioButtonGroup>();
	group->setObjectName("CTChatAlignGroup");
	auto frame = pls_new<QFrame>();
	frame->setFrameStyle(QFrame::NoFrame);
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame->setLayout(hlayout);
	frame->setObjectName("CTChatAlignFrame");

	auto checkWarp = pls_new<PLSCheckBox>(tr("ChatTemplate.option.warp"));
	checkWarp->setSpac(5);
	checkWarp->setEnabled(isEnableWrap);
	checkWarp->setChecked(isCheckWrap);
	hlayout->addWidget(checkWarp);

	auto checkWarpInfo = pls_new<PLSWidgetInfo>(this, prop, checkWarp);
	connect(checkWarp, &PLSCheckBox::clicked, checkWarpInfo, &PLSWidgetInfo::ControlChanged);
	checkWarp->setObjectName("CTOptionCheckWarp");
	children.emplace_back(checkWarpInfo);

	createRadioButton(2, val, hlayout, group, {tr("ChatTemplate.option.left.align"), tr("ChatTemplate.option.right.align")}, true, frame);
	if (isLeftAlign) {
		group->button(0)->setChecked(true);
	} else {
		group->button(1)->setChecked(true);
	}
	group->button(0)->setEnabled(isEnableAlign);
	group->button(1)->setEnabled(isEnableAlign);
	auto alignInfo = pls_new<PLSWidgetInfo>(this, prop, group);
	connect(group, &PLSRadioButtonGroup::idClicked, alignInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(alignInfo);

	auto sortLable = pls_new<QLabel>(tr("ChatTemplate.option.sort"));
	sortLable->setObjectName("subLabel");
	sortLable->setWordWrap(true);
	flayout->addRow(sortLable, frame);

	auto hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(25);
	auto group2 = pls_new<PLSRadioButtonGroup>();
	group2->setObjectName("CTChatDisapperGroup");
	auto frame2 = pls_new<QFrame>();
	frame2->setFrameStyle(QFrame::NoFrame);
	frame2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame2->setLayout(hlayout2);
	frame2->setObjectName("CTChatDisapperFrame");

	createRadioButton(2, val, hlayout2, group2, {tr("ChatTemplate.option.disapper.on"), tr("ChatTemplate.option.disapper.off")}, true, frame2);
	if (isCheckDisapper) {
		group2->button(0)->setChecked(true);
	} else {
		group2->button(1)->setChecked(true);
	}
	setLayoutEnable(hlayout2, isEnableDisapper);
	auto DisapperInfo = pls_new<PLSWidgetInfo>(this, prop, group2);
	connect(group2, &PLSRadioButtonGroup::idClicked, DisapperInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(DisapperInfo);

	flayout->addItem(pls_new<QSpacerItem>(10, 30, QSizePolicy::Fixed, QSizePolicy::Fixed));
	auto disapperLable = pls_new<QLabel>(tr("ChatTemplate.option.disapper"));
	disapperLable->setObjectName("subLabel");
	disapperLable->setWordWrap(true);
	flayout->addRow(disapperLable, frame2);

	auto hlayout3 = pls_new<QHBoxLayout>();
	hlayout3->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout3->setContentsMargins(0, 0, 0, 0);
	hlayout3->setSpacing(10);
	auto widthLable = pls_new<QLabel>(tr("ChatTemplate.option.width"));
	widthLable->setObjectName("CTChatWidthLable");
	hlayout3->addWidget(widthLable);

	auto widthSpinBox = pls_new<PLSSpinBox>();
	widthSpinBox->setObjectName("widthSpinBox");
	widthSpinBox->setRange(150, 1024);
	widthSpinBox->setSingleStep(1);
	widthSpinBox->setValue(chatWidth);
	hlayout3->addWidget(widthSpinBox);

	auto widthInfo = pls_new<PLSWidgetInfo>(this, prop, widthSpinBox);
	connect(widthSpinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), widthInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(widthInfo);
	pls_connect(
		PLSBasic::Get(), &PLSBasic::updateChatV2PropertBrowserSize, widthSpinBox,
		[this, widthSpinBox](const QSize &size) {
			widthSpinBox->blockSignals(true);
			widthSpinBox->setValue(size.width());
			widthSpinBox->blockSignals(false);
			if (m_ctSaveTemplateBtn) {
				obs_data_set_bool(settings, "ctParamChanged", true);
				m_ctSaveTemplateBtn->setEnabled(true);
			}
		},
		Qt::QueuedConnection);

	auto frame3 = pls_new<QFrame>();
	frame3->setFrameStyle(QFrame::NoFrame);
	frame3->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame3->setLayout(hlayout3);
	frame3->setObjectName("CTChatWidthFrame");
	hlayout3->setStretch(1, 1);

	auto hlayout4 = pls_new<QHBoxLayout>();
	hlayout4->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout4->setContentsMargins(0, 0, 0, 0);
	hlayout4->setSpacing(10);
	auto heightLable = pls_new<QLabel>(tr("ChatTemplate.option.height"));
	heightLable->setObjectName("CTChatHegihtLable");
	hlayout4->addWidget(heightLable);

	auto heightSpinBox = pls_new<PLSSpinBox>();
	heightSpinBox->setObjectName("heightSpinBox");
	heightSpinBox->setRange(70, 768);
	heightSpinBox->setSingleStep(1);
	heightSpinBox->setValue(chatHeight);
	hlayout4->addWidget(heightSpinBox);
	auto heightInfo = pls_new<PLSWidgetInfo>(this, prop, heightSpinBox);
	connect(heightSpinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), heightInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(heightInfo);
	pls_connect(
		PLSBasic::Get(), &PLSBasic::updateChatV2PropertBrowserSize, heightSpinBox,
		[this, heightSpinBox](const QSize &size) {
			heightSpinBox->blockSignals(true);
			heightSpinBox->setValue(size.height());
			heightSpinBox->blockSignals(false);
			if (m_ctSaveTemplateBtn) {
				obs_data_set_bool(settings, "ctParamChanged", true);
				m_ctSaveTemplateBtn->setEnabled(true);
			}
		},
		Qt::QueuedConnection);

	auto frame4 = pls_new<QFrame>();
	frame4->setFrameStyle(QFrame::NoFrame);
	frame4->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame4->setLayout(hlayout4);
	frame4->setObjectName("CTChatHeightFrame");
	hlayout4->setStretch(1, 1);

	auto hlayout5 = pls_new<QHBoxLayout>();
	hlayout5->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout5->setContentsMargins(0, 0, 0, 0);
	hlayout5->setSpacing(20);
	hlayout5->addWidget(frame3);
	hlayout5->addWidget(frame4);
	setLayoutEnable(hlayout5, isEnableSize);

	flayout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	auto sizeLabel = pls_new<QLabel>(tr("ChatTemplate.option.size"));
	sizeLabel->setObjectName("subLabel");
	sizeLabel->setWordWrap(true);
	flayout->addRow(sizeLabel, hlayout5);
	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(label);
	layout->addItem(pls_new<QSpacerItem>(10, 31, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddCtMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	int index = obs_data_get_int(val, "chatMotionStyle");
	bool isCheck = obs_data_get_bool(val, "isCheckChatMotion");
	bool isEnable = obs_data_get_bool(val, "isEnableChatMotion");

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(0);

	auto hlayout = pls_new<QHBoxLayout>();
	hlayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(25);

	auto group = pls_new<PLSRadioButtonGroup>();
	group->setObjectName("CTMotionGroup");
	auto frame = pls_new<QFrame>();
	frame->setFrameStyle(QFrame::NoFrame);
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame->setLayout(hlayout);
	frame->setObjectName("CTMotionGroupFrame");
	createRadioButton(3, val, hlayout, group, {tr("ChatTemplate.motion.shaking"), tr("ChatTemplate.motion.random"), tr("ChatTemplate.motion.wave")}, true, frame);
	if (index != -1 && isCheck) {
		group->button(index)->setChecked(true);
	}
	setLayoutEnable(hlayout, isEnable && isCheck);
	auto wi = pls_new<PLSWidgetInfo>(this, prop, group);
	connect(group, &PLSRadioButtonGroup::idClicked, wi, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(wi);

	auto hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(6);
	auto checkText = pls_new<QLabel>(tr("ChatTemplate.motions"));
	checkText->setProperty("showHandCursor", true);
	checkText->setEnabled(isEnable);
	checkText->setWordWrap(true);
	auto check = pls_new<PLSCheckBox>();
	check->setEnabled(isEnable);
	check->setChecked(isCheck);
	hlayout2->addWidget(checkText);
	hlayout2->addWidget(check);
	auto frame2 = pls_new<QFrame>();
	frame2->setFrameStyle(QFrame::NoFrame);
	frame2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame2->setLayout(hlayout2);
	frame2->setObjectName("CTMotionCheckboxFrame");
	auto info = pls_new<PLSWidgetInfo>(this, prop, check);
	connect(check, &PLSCheckBox::clicked, info, &PLSWidgetInfo::ControlChanged);
	connect(check, &PLSCheckBox::clicked, [this, hlayout, group, name](bool isCheck) {
		if (isCheck) {
			obs_data_t *val = obs_data_get_obj(settings, name);
			int index = obs_data_get_int(val, "chatMotionStyle");
			if (index != -1)
				group->button(index)->setChecked(true);
		} else {
			for (auto btn : group->buttons()) {
				btn->setChecked(false);
			}
		}
		setLayoutEnable(hlayout, isCheck);
	});
	check->setObjectName("CTMotionCheckbox");
	children.emplace_back(info);

	flayout->insertRow(0, frame2, frame);
	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(pls_new<QSpacerItem>(10, 30, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(label);
	layout->addItem(pls_new<QSpacerItem>(10, 31, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

void PLSWidgetInfo::CTDisplayChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ct_display = obs_data_get_obj(view->settings, setting);
	obs_data_t *ct_new_display = obs_data_create();
	obs_data_set_bool(ct_new_display, "isEnablePlatformType", obs_data_get_bool(ct_display, "isEnablePlatformType"));
	obs_data_set_bool(ct_new_display, "isEnablePlatformIcon", obs_data_get_bool(ct_display, "isEnablePlatformIcon"));
	obs_data_set_bool(ct_new_display, "isEnableLevelIcon", obs_data_get_bool(ct_display, "isEnableLevelIcon"));
	obs_data_set_bool(ct_new_display, "isCheckLevelIcon", obs_data_get_bool(ct_display, "isCheckLevelIcon"));
	obs_data_set_bool(ct_new_display, "isCheckPlatformIcon", obs_data_get_bool(ct_display, "isCheckPlatformIcon"));
	obs_data_set_bool(ct_new_display, "isEnableIdIcon", obs_data_get_bool(ct_display, "isEnableIdIcon"));
	obs_data_set_bool(ct_new_display, "isCheckIdIcon", obs_data_get_bool(ct_display, "isCheckIdIcon"));
	obs_data_set_bool(ct_new_display, "isEnablePlatformIconDisplay", obs_data_get_bool(ct_display, "isEnablePlatformIconDisplay"));
	obs_data_set_bool(ct_new_display, "isEnableChatDisplay", obs_data_get_bool(ct_display, "isEnableChatDisplay"));
	obs_data_set_string(ct_new_display, "selectPlatformList", obs_data_get_string(ct_display, "selectPlatformList"));
	QString objName = object->objectName();
	if ("platformIconCheck" == objName) {
		bool isChecked = static_cast<PLSCheckBox *>(object)->isChecked();
		obs_data_set_bool(ct_new_display, "isCheckPlatformIcon", isChecked);

	} else if ("levelIconCheck" == objName) {
		bool isChecked = static_cast<PLSCheckBox *>(object)->isChecked();
		obs_data_set_bool(ct_new_display, "isCheckLevelIcon", isChecked);
	} else if ("idIconCheck" == objName) {
		bool isChecked = static_cast<PLSCheckBox *>(object)->isChecked();
		obs_data_set_bool(ct_new_display, "isCheckIdIcon", isChecked);
	} else if ("CTChatPlatformCheck" == objName) {
		QStringList selectPlatformList;
		auto propertiesView = static_cast<PLSPropertiesView *>(view);
		for (auto checkbox : propertiesView->m_platfromCheckBoxs) {
			if (checkbox && checkbox->isChecked()) {
				auto platformName = checkbox->property("platformName").toString();
				selectPlatformList.append(platformName);
			}
		}
		obs_data_set_string(ct_new_display, "selectPlatformList", selectPlatformList.join(';').toUtf8().constData());
	}

	obs_data_set_obj(view->settings, setting, ct_new_display);
	obs_data_release(ct_display);
	obs_data_release(ct_new_display);
}
void PLSWidgetInfo::CTOptionsChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ct_options = obs_data_get_obj(view->settings, setting);
	obs_data_t *ct_new_options = obs_data_create();

	obs_data_set_bool(ct_new_options, "isEnableChatWrap", obs_data_get_bool(ct_options, "isEnableChatWrap"));
	obs_data_set_bool(ct_new_options, "isCheckChatWrap", obs_data_get_bool(ct_options, "isCheckChatWrap"));
	obs_data_set_bool(ct_new_options, "isCheckLeftChatAlign", obs_data_get_bool(ct_options, "isCheckLeftChatAlign"));
	obs_data_set_bool(ct_new_options, "isEnableChatAlign", obs_data_get_bool(ct_options, "isEnableChatAlign"));
	obs_data_set_bool(ct_new_options, "isEnableChatSort", obs_data_get_bool(ct_options, "isEnableChatSort"));
	obs_data_set_bool(ct_new_options, "isCheckChatDisapperEffect", obs_data_get_bool(ct_options, "isCheckChatDisapperEffect"));
	obs_data_set_bool(ct_new_options, "isEnableChatDisapperEffect", obs_data_get_bool(ct_options, "isEnableChatDisapperEffect"));
	obs_data_set_bool(ct_new_options, "isEnableChatBoxSize", obs_data_get_bool(ct_options, "isEnableChatBoxSize"));
	obs_data_set_bool(ct_new_options, "isEnableChatOption", obs_data_get_bool(ct_options, "isEnableChatOption"));

	QString objName = object->objectName();

	if ("CTChatAlignGroup" == objName) {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId == 0) {
			obs_data_set_bool(ct_new_options, "isCheckLeftChatAlign", true);
		} else {
			obs_data_set_bool(ct_new_options, "isCheckLeftChatAlign", false);
		}
	} else if ("CTOptionCheckWarp" == objName) {
		bool isChecked = static_cast<PLSCheckBox *>(object)->isChecked();
		obs_data_set_bool(ct_new_options, "isCheckChatWrap", isChecked);
	} else if ("CTChatDisapperGroup" == objName) {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId == 0) {
			obs_data_set_bool(ct_new_options, "isCheckChatDisapperEffect", true);
		} else {
			obs_data_set_bool(ct_new_options, "isCheckChatDisapperEffect", false);
		}
	} else if ("widthSpinBox" == objName) {
		int value = static_cast<QSpinBox *>(object)->value();
		obs_data_set_int(view->settings, "chatWidth", value);

	} else if ("heightSpinBox" == objName) {
		int value = static_cast<QSpinBox *>(object)->value();
		obs_data_set_int(view->settings, "chatHeight", value);
	}

	obs_data_set_obj(view->settings, setting, ct_new_options);
	obs_data_release(ct_options);
	obs_data_release(ct_new_options);
}
void PLSWidgetInfo::CTMotionChanged(const char *setting)
{
	obs_data_set_bool(view->settings, "ctParamChanged", true);
	view->m_ctSaveTemplateBtn->setEnabled(true);

	obs_data_t *ct_motion = obs_data_get_obj(view->settings, setting);
	obs_data_t *ct_new_motion = obs_data_create();
	obs_data_set_bool(ct_new_motion, "isCheckChatMotion", obs_data_get_bool(ct_motion, "isCheckChatMotion"));
	obs_data_set_bool(ct_new_motion, "isEnableChatMotion", obs_data_get_bool(ct_motion, "isEnableChatMotion"));
	obs_data_set_int(ct_new_motion, "chatMotionStyle", obs_data_get_int(ct_motion, "chatMotionStyle"));

	QString objName = object->objectName();
	if ("CTMotionGroup" == objName) {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(ct_new_motion, "chatMotionStyle", checkId);
		}

	} else if ("CTMotionCheckbox" == objName) {
		bool isChecked = static_cast<PLSCheckBox *>(object)->isChecked();
		obs_data_set_bool(ct_new_motion, "isCheckChatMotion", isChecked);
	}

	obs_data_set_obj(view->settings, setting, ct_new_motion);
	obs_data_release(ct_motion);
	obs_data_release(ct_new_motion);
}
