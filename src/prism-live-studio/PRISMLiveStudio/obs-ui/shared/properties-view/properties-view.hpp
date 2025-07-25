#pragma once

#include <vertical-scroll-area.hpp>
#include <qt-wrappers.hpp>
#include <obs-data.h>
#include <obs.hpp>
#include <qtimer.h>
#include <QPointer>
#include <vector>
#include <memory>

class QFormLayout;
class OBSPropertiesView;
class QLabel;
class QComboBox;
class PLSCommonScrollBar;

typedef obs_properties_t *(*PropertiesReloadCallback)(void *obj);
typedef void (*PropertiesUpdateCallback)(void *obj, obs_data_t *old_settings, obs_data_t *new_settings);
typedef void (*PropertiesVisualUpdateCb)(void *obj, obs_data_t *settings);

QWidget *plsCreateHelpQWidget(QWidget *originWidget, const QString &longDesc, const char *name = nullptr,
			      const QVariant &value = QVariant());

#define NO_PROPERTIES_STRING QObject::tr("Basic.PropertiesWindow.NoProperties")

/* ------------------------------------------------------------------------- */
//PRISM/renjinbo/20230906/#2471/color dialog clicked after properties refreshed
class WidgetInfo : public QObject, public std::enable_shared_from_this<WidgetInfo> {
	Q_OBJECT

	friend class OBSPropertiesView;
	friend class PLSPropertiesView;
	//PRISM/renjinbo/20221229/#/subclass need read super class
protected:
	OBSPropertiesView *view;
	obs_property_t *property;
	QWidget *widget;
	QPointer<QTimer> update_timer;
	bool recently_updated = false;
	OBSData old_settings_cache;

	void BoolChanged(const char *setting);
	void IntChanged(const char *setting);
	void FloatChanged(const char *setting);
	void TextChanged(const char *setting);
	bool PathChanged(const char *setting);
	virtual void ListChanged(const char *setting);
	bool ColorChangedInternal(const char *setting, bool supportAlpha);
	bool ColorChanged(const char *setting);
	bool ColorAlphaChanged(const char *setting);
	bool FontChanged(const char *setting);
	void GroupChanged(const char *setting);
	void EditableListChanged();
	void ButtonClicked();

	void TogglePasswordText(bool checked);
	void DeviceChanged(QString text, bool isDshow, bool isMobile);

public:
	inline WidgetInfo(OBSPropertiesView *view_, obs_property_t *prop, QWidget *widget_)
		: view(view_),
		  property(prop),
		  widget(widget_)
	{
	}

	~WidgetInfo()
	{
		if (update_timer) {
			update_timer->stop();
			QMetaObject::invokeMethod(update_timer, "timeout");
			update_timer->deleteLater();
		}
	}

	void ControlChangedToRefresh(const char *setting);

	//PRISM/renjinbo/20240719/#/prism add method
	void setIsControlChanging(bool isControlChanging_);

public slots:
	//PRISM/renjinbo/20221229/#/add virtual
	virtual void ControlChanged();
	//PRISM/Xiewei/20230428/none/add SRE log
	void CheckValue();

	/* editable list */
	void EditListAdd();
	void EditListAddText();
	void EditListAddFiles();
	void EditListAddDir();
	void EditListRemove();
	void EditListEdit();
	void EditListUp();
	void EditListDown();
};

/* ------------------------------------------------------------------------- */

class OBSPropertiesView : public VScrollArea {
	Q_OBJECT

	friend class WidgetInfo;
	friend class PLSWidgetInfo;

	using properties_delete_t = decltype(&obs_properties_destroy);
	using properties_t = std::unique_ptr<obs_properties_t, properties_delete_t>;

	//PRISM/renjinbo/20221229/#/subclass need read super class
protected:
	//prism add property and method
	QLayout *boxLayout = nullptr;
	obs_property_type lastPropertyType = OBS_PROPERTY_INVALID;
	PLSCommonScrollBar *scroll{};
	bool setCustomContentWidth = false;
	bool showFiltersBtn = false;
	bool isControlChanging = false;

	void AddSpacer(const obs_property_type &currentType, QFormLayout *layout);

	void updateUIWhenAfterAddProperty(obs_property_t *property, QFormLayout *layout, QLabel *label, QWidget *widget,
					  bool warning);

	void updateTimerUiClickStatus(bool isClick);
	void controlChangedToRefresh(obs_property_t *p, const char *setting);
	void showFilterButton(bool hasNoProperties, const char *id);

	bool isPrismLensOrMobileSource();

	//obs property
	QWidget *widget = nullptr;
	properties_t properties;
	OBSData settings;
	OBSWeakObjectAutoRelease weakObj;
	void *rawObj = nullptr;
	std::string type;
	PropertiesReloadCallback reloadCallback;
	PropertiesUpdateCallback callback = nullptr;
	PropertiesVisualUpdateCb visUpdateCb = nullptr;
	int minSize;
	std::vector<std::shared_ptr<WidgetInfo>> children;
	std::string lastFocused;
	QWidget *lastWidget = nullptr;
	bool deferUpdate;
	bool enableDefer = true;
	bool disableScrolling = false;
	bool m_bFromSetting = false;

	QPointer<QPushButton> m_ctSaveTemplateBtn;
	template<typename Sender, typename SenderParent, typename... Args>
	QWidget *NewWidget(obs_property_t *prop, Sender *widget, void (SenderParent::*signal)(Args...))
	{
		const char *long_desc = obs_property_long_description(prop);

		WidgetInfo *info = new WidgetInfo(this, prop, widget);
		pls_connect(widget, signal, info, &WidgetInfo::ControlChanged);
		children.emplace_back(info);

		widget->setToolTip(QT_UTF8(long_desc));
		return widget;
	}

	QWidget *AddCheckbox(QFormLayout *layout, obs_property_t *prop);
	QWidget *AddText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	virtual void AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddFloat(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	virtual QWidget *AddList(obs_property_t *prop, bool &warning);
	void AddEditableList(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	QWidget *AddButton(obs_property_t *prop);
	void AddColorInternal(obs_property_t *prop, QFormLayout *layout, QLabel *&label, bool supportAlpha);
	void AddColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddColorAlpha(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFrameRate(obs_property_t *prop, bool &warning, QFormLayout *layout, QLabel *&label);

	void AddGroup(obs_property_t *prop, QFormLayout *layout);
	//PRISM/renjinbo/20221229/#/add virtual
	virtual void AddProperty(obs_property_t *property, QFormLayout *layout);

	void resizeEvent(QResizeEvent *event) override;

	void GetScrollPos(int &h, int &v, int &hend, int &vend);
	void SetScrollPos(int h, int v, int old_hend, int old_vend);
	// prism add slots
public slots:
	void OnShowScrollBar(bool isShow);
	void OnOpenPrismLensClicked();

public slots:
	virtual void ReloadProperties();
	virtual void RefreshProperties();
	void SignalChanged();

signals:
	void PropertiesResized();
	void Changed();
	void PropertiesRefreshed();
	void reloadOldSettings();

public:
	OBSPropertiesView(OBSData settings, obs_object_t *obj, PropertiesReloadCallback reloadCallback,
			  PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, int minSize = 0);
	OBSPropertiesView(OBSData settings, void *obj, PropertiesReloadCallback reloadCallback,
			  PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr, int minSize = 0);
	OBSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0,
			  bool bFromSetting = false);

#define obj_constructor(type)                                                                                     \
	inline OBSPropertiesView(OBSData settings, obs_##type##_t *type, PropertiesReloadCallback reloadCallback, \
				 PropertiesUpdateCallback callback, PropertiesVisualUpdateCb cb = nullptr,        \
				 int minSize = 0)                                                                 \
		: OBSPropertiesView(settings, (obs_object_t *)type, reloadCallback, callback, cb, minSize)        \
	{                                                                                                         \
	}

	obj_constructor(source);
	obj_constructor(output);
	obj_constructor(encoder);
	obj_constructor(service);
#undef obj_constructor

	inline obs_data_t *GetSettings() const { return settings; }
	void *GetSourceObj() const
	{
		OBSObject strongObj = GetObject();
		return strongObj ? strongObj.Get() : rawObj;
	}

	inline void UpdateSettings()
	{
		if (callback)
			callback(OBSGetStrongRef(weakObj), nullptr, settings);
		else if (visUpdateCb)
			visUpdateCb(OBSGetStrongRef(weakObj), settings);
	}
	inline bool DeferUpdate() const { return deferUpdate; }
	inline void SetDeferrable(bool deferrable) { enableDefer = deferrable; }

	inline OBSObject GetObject() const { return OBSGetStrongRef(weakObj); }

	void setScrolling(bool enabled)
	{
		disableScrolling = !enabled;
		RefreshProperties();
	}

	void SetDisabled(bool disabled);

	bool getIsCustomContentMargins(const char *sourceId = nullptr);
	void setContentMarginAndWidth();
	void SetCustomContentWidth(bool setCustomContentWidth_) { setCustomContentWidth = setCustomContentWidth_; }
	int getPrismLensOutputIndex();
	int getPrismLensOutputIndex(QString name);
	void showLensUninstallTips(bool isMobile);

	void textColorChanged(const QByteArray &_id, const QColor &color, QColor::NameFormat format);

#define Def_IsObject(type)                                \
	inline bool IsObject(obs_##type##_t *type) const  \
	{                                                 \
		OBSObject obj = OBSGetStrongRef(weakObj); \
		return obj.Get() == (obs_object_t *)type; \
	}

	/* clang-format off */
	Def_IsObject(source)
	Def_IsObject(output)
	Def_IsObject(encoder)
	Def_IsObject(service)
	/* clang-format on */

#undef Def_IsObject
};
