#pragma once

#include "properties-view.hpp"
#include <QFontDatabase>

class QFormLayout;
class PLSPropertiesView;
class QLabel;
class PLSSpinBox;
class PLSComboBox;
class SliderIgnoreScroll;
class QSpinBox;
class QBoxLayout;
class QGridLayout;
class QHBoxLayout;
struct ITextMotionTemplateHelper;
class OBSBasicSettings;
class PLSCommonScrollBar;
class PLSCheckBox;
class TMCheckBox;
class PLSRadioButtonGroup;

class PLSWidgetInfo : public WidgetInfo {
	Q_OBJECT

	friend class OBSPropertiesView;
	friend class PLSPropertiesView;

public:
	inline PLSWidgetInfo(OBSPropertiesView *view_, obs_property_t *prop, QObject *object_) : WidgetInfo(view_, prop, dynamic_cast<QWidget *>(object_)), object(object_)
	{
		object->installEventFilter(this);
	}
	void SetOriginalColorFilter(bool state);

public slots:
	void UserOperation() const;
	void ControlChanged() override;

	void VirtualBackgroundResourceMotionDisabledChanged(bool motionDisabled);
	void VirtualBackgroundResourceSelected(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
					       const QString &foregroundPath, const QString &foregroundStaticImgPath, int dataType);
	void VirtualBackgroundResourceDeleted(const QString &itemId);
	void VirtualBackgroundMyResourceDeleteAll(const QStringList &itemIds);

signals:
	void PropertyUpdateNotify();

protected:
	void ListChanged(const char *setting) override;

private:
	QObject *object;
	bool isOriginColorFilter{false};

	bool BoolGroupChanged(const char *setting);
	void ButtonGroupClicked(const char *setting);
	void CustomButtonGroupClicked(const char *setting);
	void ChatTemplateListChanged(const char *setting);
	void ChatFontSizeChanged(const char *setting);
	void TMTextChanged(const char *setting);
	void TMTextContentChanged(const char *setting);
	void TMTextTabChanged(const char *setting);
	void TMTextTemplateTabChanged(const char *setting);
	void TMTextTemplateListChanged(const char *setting);
	void TMTextColorChanged(const char *setting);
	void TMTextMotionChanged(const char *setting);
	void EditableListChanged();

	void SelectRegionClicked(const char *setting);
	void ImageGroupChanged(const char *setting);
	void intCustomGroupChanged(const char *setting);
	void CustomGroupChanged(const char *setting);
	void TextButtonClicked();
	void FontSimpleChanged(const char *setting);
	void ColorCheckBoxChanged(const char *setting);
	void templateListChanged(const char *setting);
};

class PLSPropertiesView : public OBSPropertiesView {
	Q_OBJECT

	friend class WidgetInfo;
	friend class PLSWidgetInfo;

public:
	PLSPropertiesView(OBSData settings, obs_object_t *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, int minSize = 0,
			  int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
			  bool reloadPropertyOnInit = true);
	PLSPropertiesView(OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, int minSize = 0,
			  int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
			  bool reloadPropertyOnInit = true);
	PLSPropertiesView(const QWidget *parent, OBSData settings, obs_object_t *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  int minSize = 0, int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
			  bool reloadPropertyOnInit = true);
	PLSPropertiesView(const QWidget *parent, OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  int minSize = 0, int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
			  bool reloadPropertyOnInit = true);
	PLSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true,
			  bool colorFilterOriginalPressed = false, bool refreshProperties = true, bool reloadPropertyOnInit = true);
	PLSPropertiesView(OBSBasicSettings *basicSettings, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1, bool showFiltersBtn = false,
			  bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true, bool reloadPropertyOnInit = true);

#define obj_constructor(type)                                                                                                                                                               \
	inline PLSPropertiesView(OBSData settings, obs_##type##_t *type, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, \
				 int minSize = 0, int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false,                  \
				 bool refreshProperties = true, bool reloadPropertyOnInit = true)                                                                                           \
		: PLSPropertiesView(settings, (obs_object_t *)type, reloadCallback, callback, cb, minSize, maxSize, showFiltersBtn, colorFilterOriginalPressed, refreshProperties,          \
				    reloadPropertyOnInit)                                                                                                                                   \
	{                                                                                                                                                                                   \
	}

	obj_constructor(source);
	obj_constructor(output);
	obj_constructor(encoder);
	obj_constructor(service);
#undef obj_constructor

	void ReloadPropertiesByBool(bool refreshProperties);

	const char *getSourceId() const;
	const char *getSourceId(OBSSource &source) const;
	bool isFirstAddSource() const;
	void refreshViewAfterUIChanged(obs_property_t *p);

	void SetForProperty(bool forPropertyWindow) { isForPropertyWindow = forPropertyWindow; }

	bool isResolutionChanged() const { return resolutionChanged; }
	void CheckValues();

	void addWidgetToBottom(QWidget *addWid);

public slots:
	void ReloadProperties() override;
	void RefreshProperties() override;
	void OnColorFilterOriginalPressed(bool state);
	void OnIntValueChanged(int value);
	void UpdateColorFilterValue(int value, bool isOriginal);
	void OnVirtualBackgroundResourceOpenFilter() const;
	void PropertyUpdateNotify(const QString &name) const;
	void ResetProperties(obs_properties_t *newProperties);

signals:
	void OpenFilters();
	void ColorFilterValueChanged(int value);
	void OpenStickers();
	void OpenMusicButtonClicked(OBSSource source);
	void okButtonControl(bool enable);

protected:
	void AddProperty(obs_property_t *property, QFormLayout *layout) override;
	QWidget *AddList(obs_property_t *prop, bool &warning) override;

private:
	int maxSize;
	bool showColorFilterPath = true;
	bool isColorFilter = false;
	bool colorFilterOriginalPressed = false;
	bool isForPropertyWindow = false;
	bool resolutionChanged = false; // zhangdewen check camera(dshow) resolution changed
	SliderIgnoreScroll *sliderView{};
	PLSSpinBox *spinsView{};
	PLSWidgetInfo *infoView{};
	QPointer<QWidget> m_loadingPage; 

	ITextMotionTemplateHelper *m_tmHelper = nullptr;
	enum class ButtonType { RadioButon, PushButton, CustomButton, LetterButton };
	bool m_tmTabChanged = true;
	bool m_tmTemplateChanged = false;
	QVector<QLabel *> m_tmLabels;
	QList<QPointer<QPushButton>> m_movieButtons;

	OBSBasicSettings *m_basicSettings = nullptr;

	void setInitData(bool showFiltersBtn_, bool refreshProperties_, bool reloadPropertyOnInit_);
	void AddMobileGuider(obs_property_t *prop, QFormLayout *layout);
	void AddHLine(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddRadioButtonGroup(obs_property_t *prop, QFormLayout *layout);
	void AddButtonGroup(obs_property_t *prop, QFormLayout *layout);
	void AddCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddMusicList(obs_property_t *prop, QFormLayout *layout);
	void AddTips(obs_property_t *prop, QFormLayout *layout);
	QWidget *AddTextContent(obs_property_t *prop);

	void AddChatTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddChatFontSize(obs_property_t *prop, QFormLayout *layout);

	void AddTmTab(obs_property_t *prop, QFormLayout *layout);
	void AddTmTemplateTab(obs_property_t *prop, QFormLayout *layout);
	void AddTmTabTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddTmText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTmTextContent(obs_property_t *prop, QFormLayout *layout);
	void AddTmColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTmMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddDefaultText(obs_property_t *prop, QFormLayout *layout, QLabel *&label) const;

	QWidget *AddSelectRegion(obs_property_t *prop, bool &warning);
	void AddImageGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddvirtualCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	QWidget *addIntForCustomGroup(obs_property_t *prop, int index);
	void AddPrismCheckbox(obs_property_t *prop, QFormLayout *layout, Qt::LayoutDirection layoutDirection);
	void AddCameraVirtualBackgroundState(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddVirtualBackgroundResource(obs_property_t *prop, QBoxLayout *layout);
	QWidget *AddSwitch(obs_property_t *prop, QFormLayout *layout);
	void AddMobileHelp(obs_property_t *prop, QFormLayout *layout);
	QWidget *AddMobileName(obs_property_t *prop);
	QWidget *AddMobileStatus(obs_property_t *prop);

	void AddFontSimple(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddColorCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddColorAlphaCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *&label);

	/*tm ui*/
	void creatColorList(obs_property_t *prop, QGridLayout *&hLayout, int index, const long long colorValue, const QString &colorList);
	void createTMSlider(obs_property_t *prop, int propertyValue, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix, bool isEnable = true,
			    bool isShowSliderIcon = false, const QString &sliderName = QString());
	void createTMSlider(SliderIgnoreScroll *&slider, PLSSpinBox *&spinBox, obs_property_t *prop, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix,
			    bool isEnable = true);
	void createTMColorCheckBox(PLSCheckBox *&controlCheckBox, obs_property_t *prop, QFrame *&frame, int index, const QString &labelName, const QHBoxLayout *layout, bool isControlOn,
				   bool isControl);
	void createColorButton(obs_property_t *prop, QGridLayout *&gLayout, const PLSCheckBox *checkBox, const QString &opationName, int index, bool isSuffix, bool isEnable);
	void setLabelColor(QLabel *label, const long long colorValue, const int alaphValue, bool frameStyle = true) const;
	void getTmColor(obs_data_t *textData, int tabIndex, bool &isControlOn, bool &isColor, long long &color, bool &isAlaph, int &alaph) const;
	void createTMButton(const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, QButtonGroup *&group, ButtonType buttonType, const QStringList &buttonObjs = QStringList(),
			    bool isShowText = false, bool isAutoExclusive = true) const;
	void createRadioButton(const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, PLSRadioButtonGroup *&group, const QStringList &buttonObjs = QStringList(), bool isShowText = false,
			       QWidget *parent = nullptr);
	void creatTMTextWidget(obs_property_t *prop, const int textCount, obs_data_t *textData, QHBoxLayout *&hLayout);
	void updateTMTemplateButtons(const int templateTabIndex, const QString &templateTabName, QGridLayout *gLayout);
	void updateFontSytle(const QString &family, PLSComboBox *fontStyleBox) const;
	void setLayoutEnable(const QLayout *layout, bool isEnable);
	void createColorTemplate(obs_property_t *prop, QLabel *colorLabel, QPushButton *button, QHBoxLayout *subLayout);
	void setPlaceholderColor_666666(QWidget *widget) const;

	void ShowLoading();
	void HideLoading();
};
