#pragma once

#include <vertical-scroll-area.hpp>
#include <qt-wrappers.hpp>
#include <obs-data.h>
#include <obs.hpp>
#include <qtimer.h>
#include <QPointer>
#include <QMap>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>
#include <libui.h>
#include <typeinfo>
#include "pls/pls-lens-info.h"

class QFormLayout;
class OBSPropertiesView;
class QLabel;
class QComboBox;
class PLSCommonScrollBar;
class PLSLoadingButton;
class QListWidget;
class PLSRadioButtonGroup;
class PLSRadioButton;

typedef obs_properties_t *(*PropertiesReloadCallback)(void *obj);
typedef void (*PropertiesUpdateCallback)(void *obj, obs_data_t *old_settings, obs_data_t *new_settings);
typedef void (*PropertiesVisualUpdateCb)(void *obj, obs_data_t *settings);

QWidget *plsCreateHelpQWidget(QWidget *originWidget, const QString &longDesc, const char *name = nullptr,
			      const QVariant &value = QVariant());

#define NO_PROPERTIES_STRING QObject::tr("Basic.PropertiesWindow.NoProperties")

#if defined(_WIN32)
static constexpr const char *CSTR_VIDEO_DEVICE_ID = "video_device_id";
static constexpr const char *LENSV2_VIDEO_INDEX = "lens_video_index";
// In PRISM_PC-3391, we add fixed path for lens dshow. In future device name of windows lens can be changed with
// any string, then value of "video_device_id" is changed from "PRISM Lens {i}:" to "{anyName}:{path}".
// So, for camera source on windows, we must use device path to judge whether it is Lens device.
// Note: During renaming lens, user cannot use special characters, such as ":".
static const char *CSTR_PRISM_LEN1 = TEXT_LENS_VIDEO_PATH_1;
static const char *CSTR_PRISM_LEN2 = TEXT_LENS_VIDEO_PATH_2;
static const char *CSTR_PRISM_LEN3 = TEXT_LENS_VIDEO_PATH_3;
#elif defined(__APPLE__)
static constexpr const char *CSTR_VIDEO_DEVICE_ID = "device";
static const char *CSTR_PRISM_LEN1 = UUID_PRISM_LEN1;
static const char *CSTR_PRISM_LEN2 = UUID_PRISM_LEN2;
static const char *CSTR_PRISM_LEN3 = UUID_PRISM_LEN3;
#endif

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
	void DeviceChanged(int lens_index, QString text, bool isDshow, bool isMobile);

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

		//PRISM/wangshaohui/20251125/#4598/show len active state
		UnregisterLensState();
	}

	void ControlChangedToRefresh(const char *setting);

	//PRISM/renjinbo/20240719/#/prism add method
	void setIsControlChanging(bool isControlChanging_);
	bool getIsControlChanging() const;
	//PRISM/renjinbo/202640121/#PRISM_PC-5041/prism add ui action log
	void setIsToRefreshUI(bool isToRefreshUI_);
	void printRefreshLogIfNeed();

	//PRISM/wangshaohui/20251125/#4598/show len active state
	void RegisterLensState(OBSSource source, bool videoDevice);
	void UnregisterLensState();

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
	bool m_isToRefreshUI = false;

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

	// cache widgets by resume id for reuse in RefreshProperties (avoid Qt parent deleting them)
	QMap<QString, QPointer<QWidget>> m_resumeWidgets;
	QHash<QString, int> m_resumeReuseCounts;

	QPointer<QPushButton> m_ctSaveTemplateBtn;
	QPointer<PLSLoadingButton> m_openLensBtn;
	void OpenLensApp(bool isMobile, bool showUI);

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

	/// Get cached widget by resume id or create new one. Returns (widget, true if reused).
	/// When reused, uistep auto-bind name is cleared to avoid stale callback after layout rebuild.
	template<typename T, typename... Args>
	std::pair<T *, bool> resumeNewWidget(const QString &resumeID, Args &&...args)
	{
		QString key = resumeID + QStringLiteral("_") + QString::fromUtf8(typeid(T).name());
		if (m_resumeWidgets.contains(key) && m_resumeWidgets[key]) {
			if (T *w = dynamic_cast<T *>(m_resumeWidgets[key].data())) {
				int &reuseCount = m_resumeReuseCounts[key];
				++reuseCount;
				if (reuseCount > 1) {
					qDebug() << "resumeNewWidget reused twice in refresh:" << key
						 << w; //delete log when code stable.
					Q_ASSERT_X(false, "resumeNewWidget", "resuse twice in refresh");
				} else {
					w->disconnect();
					pls_uistep_v2_clear_auto_bind_name(w);
					if constexpr (std::is_base_of_v<PLSRadioButton, T>) {
						if (auto oldGroup = w->group()) {
							oldGroup->removeButton(w);
						}
					} else if constexpr (std::is_base_of_v<QComboBox, T> ||
							     std::is_base_of_v<QListWidget, T>) {
						w->clear();
					}
					return {w, true};
				}
			}
		}
		T *w = new T(std::forward<Args>(args)...);
		m_resumeWidgets[key] = w;
		return {w, false};
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
	//PRISM/renjinbo/202640121/#PRISM_PC-5041/prism add ui action log
	virtual void printRefreshUILog(bool isRefreshed) {};
	//PRISM/wangshaohui/20251209/#4645/add vb and chromakey for lens/mobile source
	virtual void AddVbChromakey(QWidget *parent, QFormLayout *formLayout) {}

	void resizeEvent(QResizeEvent *event) override;

	void GetScrollPos(int &h, int &v, int &hend, int &vend);
	void SetScrollPos(int h, int v, int old_hend, int old_vend);

	void AddRadioItem(PLSRadioButtonGroup *buttonGroup, QFormLayout *layout, obs_property_t *prop, QVariant value,
			  size_t idx);
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
	~OBSPropertiesView() override;

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
	int getPrismLensOutputIndexByText(QString name);
	void showLensInstallTips(bool isMobile);
	void showOpenLensNotice(bool isMobile);

	void textColorChanged(const QByteArray &_id, const QColor &color, QColor::NameFormat format);

	//PRISM/wangshaohui/show lens loading
	virtual void HookLoadingEvent(QPointer<PLSLoadingButton> openLensBtn) {}
	//PRISM/wangshaohui/20251125/#4598/show len active state
	static std::string GetDisplayDeviceName(obs_property_t *prop, size_t idx, OBSSource source, bool videoDevice);
	static std::string GenerateLensName(const char *name, bool actived);
	static int GetLensIndexFromDeviceString(const QString &text);
	static bool IsStateSupportedInLensApp();
	OBSSource GetPropertySource();
	bool IsForLensDeviceList(OBSSource source, obs_property_t *property);
	bool IsCameraSource();
	void SelectLensDevice();
	void OnCameraChanged(bool userOperation);
	bool showInstallLens = true;

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

class PLSWidgetInfoControlNotify {
public:
	explicit PLSWidgetInfoControlNotify(WidgetInfo *watcher_) : m_watcher(watcher_)
	{
		m_isNested = m_watcher->getIsControlChanging();
		if (m_watcher && !m_isNested) {
			m_watcher->setIsControlChanging(true);
			m_watcher->setIsToRefreshUI(false);
		}
	}
	~PLSWidgetInfoControlNotify()
	{
		if (m_watcher && !m_isNested) {
			m_watcher->setIsControlChanging(false);
			m_watcher->printRefreshLogIfNeed();
		}
	};

private:
	QPointer<WidgetInfo> m_watcher;
	bool m_isNested = true;
};
