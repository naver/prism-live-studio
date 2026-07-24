#pragma once

#include "properties-view.hpp"
#include "PLSLoadingButton.h"
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>

class QButtonGroup;
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
class QButtonGroup;

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
					       const QString &foregroundPath, const QString &foregroundStaticImgPath);
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

	void CTFontChanged(const char *setting);
	void CTTextColorChanged(const char *setting);
	void CTBkColorChanged(const char *setting);

	void CTDisplayChanged(const char *setting);
	void CTOptionsChanged(const char *setting);
	void CTMotionChanged(const char *setting);
	void ChzzkSponsorTypeChanged(const char *setting);
	void CTBKColorTemplateChanged(const char *setting);
	void colorToolButtonChanged(const char *setting);
};

struct PLSPropertiesData {
	int minSize = 0;
	int maxSize = -1;
	bool showFiltersBtn = false;
	bool showColorFilterPath = true;
	bool colorFilterOriginalPressed = false;
	bool refreshProperties = true;
	bool reloadPropertyOnInit = false;
	bool bChzzkKeyframeTip = false;
	bool bFromSetting = false;
};

class PLSPropertiesView : public OBSPropertiesView {
	Q_OBJECT

	friend class WidgetInfo;
	friend class PLSWidgetInfo;
	friend class PLSVbChromakey;

public:
	PLSPropertiesView(OBSData settings, obs_object_t *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  const PLSPropertiesData &pData = {});
	PLSPropertiesView(OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  const PLSPropertiesData &pData = {});
	PLSPropertiesView(const QWidget *parent, OBSData settings, obs_object_t *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  const PLSPropertiesData &pData = {});
	PLSPropertiesView(const QWidget *parent, OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,
			  const PLSPropertiesData &pData = {});
	PLSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, const PLSPropertiesData &pData = {});
	PLSPropertiesView(OBSBasicSettings *basicSettings, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, const PLSPropertiesData &pData = {});
	~PLSPropertiesView() override;

#define obj_constructor(type)                                                                                                                                                               \
	inline PLSPropertiesView(OBSData settings, obs_##type##_t *type, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, \
				 const PLSPropertiesData &pData = {})                                                                                                                       \
		: PLSPropertiesView(settings, (obs_object_t *)type, reloadCallback, callback, cb, pData)                                                                                    \
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

	void printRefreshUILog(bool isRefreshed) override;

public slots:
	void ReloadProperties() override;
	void RefreshProperties() override;
	void OnColorFilterOriginalPressed(bool state);
	void OnIntValueChanged(int value);
	void UpdateColorFilterValue(int value, bool isOriginal);
	void OnVirtualBackgroundResourceOpenFilter() const;
	void PropertyUpdateNotify(const QString &name) const;
	void ResetProperties(obs_properties_t *newProperties);
#if defined(Q_OS_WINDOWS)
	void CheckEnumTimeout();
#endif

signals:
	void OpenFilters();
	void ColorFilterValueChanged(int value);
	void OpenStickers();
	void OpenMusicButtonClicked(OBSSource source);
	void okButtonControl(bool enable);

protected:
	void AddProperty(obs_property_t *property, QFormLayout *layout) override;
	QWidget *AddList(obs_property_t *prop, bool &warning) override;
	void HookLoadingEvent(QPointer<PLSLoadingButton> openLensBtn) override;

	int GetCurrentLensIndex();
	void AddVbChromakey(QWidget *parent, QFormLayout *formLayout) override;

private:
	OBSSource m_source = nullptr;
	bool isColorFilter = false;
	bool isForPropertyWindow = false;
	bool resolutionChanged = false; // zhangdewen check camera(dshow) resolution changed
	SliderIgnoreScroll *sliderView{};
	PLSSpinBox *spinsView{};
	PLSWidgetInfo *infoView{};
	QPointer<QWidget> m_loadingPage;

	ITextMotionTemplateHelper *m_tmHelper = nullptr;
	ITextMotionTemplateHelper *m_ctHelper = nullptr;
	enum class ButtonType { RadioButon, PushButton, CustomButton, LetterButton };
	bool m_tmTabChanged = true;
	bool m_tmTemplateChanged = false;
	QVector<QLabel *> m_tmLabels;
	QList<QPointer<QPushButton>> m_movieButtons;

	OBSBasicSettings *m_basicSettings = nullptr;
	QList<QPointer<PLSCheckBox>> m_platfromCheckBoxs;

	PLSPropertiesData m_propertiesData{};

	void setInitData();
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
	void AddDefaultText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);

	QWidget *AddSelectRegion(obs_property_t *prop, bool &warning);
	void AddImageGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddvirtualCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	PLSSpinBox *addIntForCustomGroup(obs_property_t *prop, int index);
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

	void AddCtTabTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddCtFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCtTextColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCtBkColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);

	void AddCtDisplay(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCtOptions(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCtMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label);

	void AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label) override;

	//PRISM/FanZirong/20251103/PRISM_PC-3577/source capture failed guidance
	void addCaptureGuide(obs_property_t *prop, QFormLayout *layout);

	/*tm ui*/
	void creatColorList(QString resumeID, obs_property_t *prop, QGridLayout *&hLayout, int index, const long long colorValue, const QString &colorList);
	void createTMSlider(QString resumeID, obs_property_t *prop, int propertyValue, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix, bool isEnable = true,
			    bool isShowSliderIcon = false, const QString &sliderName = QString(), int indexOffset = 0);
	void createTMSlider(SliderIgnoreScroll *&slider, PLSSpinBox *&spinBox, obs_property_t *prop, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix,
			    bool isEnable = true);
	void createTMColorCheckBox(QString resumeID, PLSCheckBox *&controlCheckBox, obs_property_t *prop, QFrame *&frame, int index, const QString &labelName, const QHBoxLayout *layout,
				   bool isControlOn, bool isControl);
	void createColorButton(QString resumeID, obs_property_t *prop, QGridLayout *&gLayout, const PLSCheckBox *checkBox, const QString &opationName, int index, bool isSuffix, bool isEnable,
			       int indexOffset = 0);
	void setLabelColor(QLabel *label, const long long colorValue, const int alaphValue, bool frameStyle = true) const;
	void getTmColor(obs_data_t *textData, int tabIndex, bool &isControlOn, bool &isColor, long long &color, bool &isAlaph, int &alaph, int indexOffset = 0) const;
	void createTMButton(QString resumeID, const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, QButtonGroup *&group, ButtonType buttonType,
			    const QStringList &buttonObjs = QStringList(), bool isShowText = false, bool isAutoExclusive = true);
	void createRadioButton(QString resumeID, const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, PLSRadioButtonGroup *&group, const QStringList &buttonObjs = QStringList(),
			       bool isShowText = false, QWidget *parent = nullptr);
	void creatTMTextWidget(obs_property_t *prop, const int textCount, obs_data_t *textData, QHBoxLayout *&hLayout);
	void updateTMTemplateButtons(const int templateTabIndex, const QString &templateTabName, QGridLayout *gLayout);
	void updateCTTemplateButtons(const int templateTabIndex, const QString &tempalteTabName, QGridLayout *glayout);
	void updateFontSytle(const QString &family, PLSComboBox *fontStyleBox) const;
	void setLayoutEnable(const QLayout *layout, bool isEnable);
	void createColorTemplate(obs_property_t *prop, QLabel *colorLabel, QPushButton *button, QHBoxLayout *subLayout);
	QHBoxLayout *createColorButtonNoSlider(QString resumeID, obs_property_t *prop, long long colorValue, int colorAlpha, int index, const QString &colorButtonName);
	void ShowLoading();
	void HideLoading();
	void AddChzzkSponsor(obs_property_t *prop, QFormLayout *layout);
	void addCTBKTemplate(obs_property_t *prop, QFormLayout *layout);
	void addCTBKCustomColor(obs_property_t *prop, QFormLayout *layout);
	static QStringList getFilteredFontFamilies();

private slots:
	void removeCustomChatTemplate(int removeTemplateId);
};
