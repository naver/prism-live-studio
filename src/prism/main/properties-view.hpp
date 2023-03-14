#pragma once

#include "vertical-scroll-area.hpp"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSDpiHelper.h"

#include <obs.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <qfontdatabase.h>
#include <QPointer>

class QFormLayout;
class PLSPropertiesView;
class QLabel;
class QHBoxLayout;
class SliderIgnoreScroll;
class PLSSpinBox;
class PLSCommonScrollBar;
class QListWidgetItem;
class PLSComboBox;
class QCheckBox;
class QButtonGroup;
struct ITextMotionTemplateHelper;

typedef obs_properties_t *(*PropertiesReloadCallback)(void *obj);
typedef void (*PropertiesUpdateCallback)(void *obj, obs_data_t *settings);

/* ------------------------------------------------------------------------- */

class WidgetInfo : public QObject, public std::enable_shared_from_this<WidgetInfo> { //RenJinbo #9685 font color select crash add shared
	Q_OBJECT

	friend class PLSPropertiesView;

private:
	PLSPropertiesView *view;
	obs_property_t *property;
	QObject *object;
	QWidget *widget;
	bool isOriginColorFilter{false};
	void BoolChanged(const char *setting);
	void BoolGroupChanged(const char *setting);
	void IntChanged(const char *setting);
	void FloatChanged(const char *setting);
	void TextChanged(const char *setting);
	bool PathChanged(const char *setting);
	void ListChanged(const char *setting);
	bool ColorChanged(const char *setting);
	bool FontChanged(const char *setting);
	void GroupChanged(const char *setting);
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
	void ButtonClicked();
	void ButtonGroupClicked(const char *setting);
	void TextButtonClicked();

	void TogglePasswordText(bool checked);
	void SelectRegionClicked(const char *setting);
	void ImageGroupChanged(const char *setting);
	void intCustomGroupChanged(const char *setting);
	void CustomGroupChanged(const char *setting);
	void CheckboxGroupChanged(const char *setting);
	void intGroupChanged();
	void FontSimpleChanged(const char *setting);
	void ColorCheckBoxChanged(const char *setting);
	void timerListenListChanged(const char *setting);

signals:
	void IntValueChanged(int value);
	void PropertyUpdateNotify();

public:
	explicit inline WidgetInfo(PLSPropertiesView *view_, obs_property_t *prop, QObject *object_, bool isOriginColorFilter_ = false)
		: view(view_), property(prop), object(object_), widget(dynamic_cast<QWidget *>(object_)), isOriginColorFilter(isOriginColorFilter_)
	{
		object->installEventFilter(this);
	}
	~WidgetInfo() { isOriginColorFilter = false; }
	void CheckValue();
	void SetOriginalColorFilter(bool state);

public slots:
	void UserOperation();
	void ControlChanged();
	void OnOpenMusicButtonClicked();
	void VirtualBackgroundResourceMotionDisabledChanged(bool motionDisabled);
	void VirtualBackgroundResourceSelected(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
					       const QString &foregroundPath, const QString &foregroundStaticImgPath);
	void VirtualBackgroundResourceDeleted(const QString &itemId);
	void VirtualBackgroundMyResourceDeleteAll(const QStringList &itemIds);

	/* editable list */
	void EditListAdd();
	void EditListAddText();
	void EditListAddFiles();
	void EditListAddDir();
	void EditListRemove();
	void EditListEdit();
	void EditListUp();
	void EditListDown();

protected:
	bool eventFilter(QObject *targe, QEvent *event) override;
};

/* ------------------------------------------------------------------------- */

class PLSPropertiesView : public VScrollArea {
	Q_OBJECT

	friend class WidgetInfo;

	using properties_delete_t = decltype(&obs_properties_destroy);
	using properties_t = std::unique_ptr<obs_properties_t, properties_delete_t>;

protected:
	ITextMotionTemplateHelper *m_tmHelper = nullptr;
	QWidget *contentWidget = nullptr;
	properties_t properties;
	OBSData settings;
	void *obj = nullptr;
	std::string type;
	PropertiesReloadCallback reloadCallback;
	PropertiesUpdateCallback callback = nullptr;
	int minSize;
	int maxSize;
	std::vector<std::shared_ptr<WidgetInfo>> children; //RenJinbo #9685 font color select crash add shared
	std::string lastFocused;
	QWidget *lastWidget = nullptr;
	bool deferUpdate;
	obs_property_type lastPropertyType = OBS_PROPERTY_INVALID;
	bool showFiltersBtn = true;
	bool showColorFilterPath = true;
	bool isColorFilter = false;
	bool colorFilterOriginalPressed = false;
	bool isForPropertyWindow = false;
	bool setCustomContentMargins = false;
	bool setCustomContentWidth = false;
	bool resolutionChanged = false; // zhangdewen check camera(dshow) resolution changed
	SliderIgnoreScroll *sliderView{};
	PLSSpinBox *spinsView{};
	WidgetInfo *infoView{};
	PLSCommonScrollBar *scroll{};
	QWidget *NewWidget(obs_property_t *prop, QWidget *widget, const char *signal);
	QWidget *AddCheckbox(obs_property_t *prop, QFormLayout *layout, Qt::LayoutDirection layoutDirection = Qt::LeftToRight);
	QWidget *AddSwitch(obs_property_t *prop, QFormLayout *layout);
	QWidget *AddText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddFloat(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	QWidget *AddList(obs_property_t *prop, bool &warning);
	void AddMusicList(obs_property_t *prop, QFormLayout *layout);
	QWidget *AddSelectRegion(obs_property_t *prop, bool &warning);
	void AddTips(obs_property_t *prop, QFormLayout *layout);
	void AddEditableList(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	QWidget *AddButton(obs_property_t *prop, QFormLayout *layout);
	void AddButtonGroup(obs_property_t *prop, QFormLayout *layout);
	void AddRadioButtonGroup(obs_property_t *prop, QFormLayout *layout);
	void AddColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddColorCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFontSimple(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFrameRate(obs_property_t *prop, bool &warning, QFormLayout *layout, QLabel *&label);
	void AddMobileGuider(obs_property_t *prop, QFormLayout *layout);
	void AddMobileHelp(obs_property_t *prop, QFormLayout *layout);
	QWidget *AddMobileName(obs_property_t *prop);
	QWidget *AddMobileStatus(obs_property_t *prop);
	QWidget *AddPrivateDataText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);

	void AddGroup(obs_property_t *prop, QFormLayout *layout);
	void AddProperty(obs_property_t *property, QLayout *layout);
	void AddSpacer(const obs_property_type &currentType, QFormLayout *layout);
	void AddChatTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddChatFontSize(obs_property_t *prop, QFormLayout *layout);
	void AddTmText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTmTextContent(obs_property_t *prop, QFormLayout *layout);
	void AddTmTab(obs_property_t *prop, QFormLayout *layout);
	void AddTmTemplateTab(obs_property_t *prop, QFormLayout *layout);
	void AddTmTabTemplateList(obs_property_t *prop, QFormLayout *layout);
	void AddTmColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTmMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCameraVirtualBackgroundState(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddVirtualBackgroundResource(obs_property_t *prop, QBoxLayout *layout);

	void AddImageGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddHLine(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	QWidget *addIntForCustomGroup(obs_property_t *prop, int index);

	void AddCheckboxGroup(obs_property_t *prop, QFormLayout *layout);
	void AddIntGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddTimerListListen(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddLabelTip(obs_property_t *prop, QFormLayout *layout);

	void resizeEvent(QResizeEvent *event) override;
	void GetScrollPos(int &h, int &v);
	void SetScrollPos(int h, int v);

	void ReloadProperties(bool refreshProperties);
	virtual void RefreshProperties(std::function<void(QWidget *)> callback, bool update);

	virtual bool isFirstAddSource() const;

	const char *getSourceId() const;

public slots:
	void ReloadProperties();
	virtual void RefreshProperties();
	void SignalChanged();
	void OnColorFilterOriginalPressed(bool state);
	void OnIntValueChanged(int value);
	void UpdateColorFilterValue(int value, bool isOriginal);
	void OnShowScrollBar(bool isShow);
	void OnOpenMusicButtonClicked(OBSSource source);
	void OnVirtualBackgroundResourceOpenFilter();
	void PropertyUpdateNotify(const QString &name);
	void ResetProperties(obs_properties_t *newProperties);

signals:
	void PropertiesResized();
	void Changed();
	void PropertiesRefreshed();
	void OpenFilters();
	void ColorFilterValueChanged(int value);
	void OpenStickers();
	void OpenMusicButtonClicked(OBSSource source);
	void okButtonControl(bool enable);
	void reloadOldSettings();

public:
	explicit PLSPropertiesView(OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, int minSize = 0, int maxSize = -1,
				   bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
				   PLSDpiHelper dpiHelper = PLSDpiHelper(), bool reloadPropertyOnInit = true);
	explicit PLSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1, bool showFiltersBtn = false,
				   bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true, PLSDpiHelper dpiHelper = PLSDpiHelper(),
				   bool reloadPropertyOnInit = true);
	explicit PLSPropertiesView(QWidget *parent, OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, int minSize = 0, int maxSize = -1,
				   bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true,
				   PLSDpiHelper dpiHelper = PLSDpiHelper(), bool reloadPropertyOnInit = true);
	explicit PLSPropertiesView(QWidget *parent, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1, bool showFiltersBtn = false,
				   bool showColorFilterPath = true, bool colorFilterOriginalPressed = false, bool refreshProperties = true, PLSDpiHelper dpiHelper = PLSDpiHelper(),
				   bool reloadPropertyOnInit = true);

	inline obs_data_t *GetSettings() const { return settings; }

	inline void UpdateSettings() { callback(obj, settings); }
	inline bool DeferUpdate() const { return deferUpdate; }

	void CheckValues();

	void SetForProperty(bool forPropertyWindow) { isForPropertyWindow = forPropertyWindow; }
	void SetCustomContentMargins(bool setCustomContentMargins_) { setCustomContentMargins = setCustomContentMargins_; }
	void SetCustomContentWidth(bool setCustomContentWidth_) { setCustomContentWidth = setCustomContentWidth_; }
	bool isResolutionChanged() const { return resolutionChanged; }

	void textColorChanged(const QByteArray &_id, const QColor &color);
	void refreshViewAfterUIChanged(obs_property_t *p);

private:
	enum ButtonType { RadioButon, PushButton, CustomButton, LetterButton };
	QFontDatabase m_fontDatabase;
	bool m_tmTabChanged = true;
	bool m_tmTemplateChanged = false;
	QVector<QLabel *> m_tmLabels;
	QList<QPointer<QPushButton>> m_movieButtons;

private:
	void createTMSlider(obs_property_t *prop, int propertyValue, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix, bool isEnable = true,
			    bool isShowSliderIcon = false, const QString &sliderName = QString());
	void createTMSlider(SliderIgnoreScroll *&slider, PLSSpinBox *&spinBox, obs_property_t *prop, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix,
			    bool isEnable = true);
	void createTMColorCheckBox(QCheckBox *&controlCheckBox, obs_property_t *prop, QFrame *&frame, int index, const QString &labelName, QHBoxLayout *layout, bool isControlOn, bool isControl);
	void createColorButton(obs_property_t *prop, QGridLayout *&gLayout, QCheckBox *checkBox, const QString &opationName, int index, bool isSuffix, bool isEnable);
	void setLabelColor(QLabel *label, const long long colorValue, const int alaphValue);
	void getTmColor(obs_data_t *textData, int tabIndex, bool &isControlOn, bool &isColor, long long &color, bool &isAlaph, int &alaph);
	void createTMButton(const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, QButtonGroup *&group, ButtonType buttonType, const QStringList &buttonObjs = QStringList(),
			    bool isShowText = false, bool isAutoExclusive = true);
	void creatTMTextWidget(obs_property_t *prop, const int textCount, obs_data_t *textData, QHBoxLayout *&hLayout);
	void updateTMTemplateButtons(const int templateTabIndex, const QString &templateTabName, QGridLayout *gLayout);
	void updateFontSytle(const QString &family, PLSComboBox *fontStyleBox);
	void setLayoutEnable(QLayout *layout, bool isEnable);
	void createColorTemplate(obs_property_t *prop, QLabel *colorLabel, QPushButton *button, QHBoxLayout *subLayout);
	void setPlaceholderColor_666666(QWidget *widget);
};
