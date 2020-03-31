#pragma once

#include "vertical-scroll-area.hpp"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"

#include <obs.hpp>
#include <vector>
#include <memory>

class QFormLayout;
class PLSPropertiesView;
class QLabel;
class QHBoxLayout;
class SliderIgnoreScroll;
class PLSSpinBox;

typedef obs_properties_t *(*PropertiesReloadCallback)(void *obj);
typedef void (*PropertiesUpdateCallback)(void *obj, obs_data_t *settings);

/* ------------------------------------------------------------------------- */

class WidgetInfo : public QObject {
	Q_OBJECT

	friend class PLSPropertiesView;

private:
	PLSPropertiesView *view;
	obs_property_t *property;
	QWidget *widget;
	bool isOriginColorFilter{false};
	void BoolChanged(const char *setting);
	void IntChanged(const char *setting);
	void FloatChanged(const char *setting);
	void TextChanged(const char *setting);
	bool PathChanged(const char *setting);
	void ListChanged(const char *setting);
	bool ColorChanged(const char *setting);
	bool FontChanged(const char *setting);
	void GroupChanged(const char *setting);
	void EditableListChanged();
	void ButtonClicked();

	void TogglePasswordText(bool checked);

public:
	explicit inline WidgetInfo(PLSPropertiesView *view_, obs_property_t *prop, QWidget *widget_, bool isOriginColorFilter_ = false)
		: view(view_), property(prop), widget(widget_), isOriginColorFilter(isOriginColorFilter_)
	{
	}
	~WidgetInfo() { isOriginColorFilter = false; }
	void CheckValue();
	void SetOriginalColorFilter(bool state);
public slots:
	void UserOperation();
	void ControlChanged();

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

class PLSPropertiesView : public VScrollArea {
	Q_OBJECT

	friend class WidgetInfo;

	using properties_delete_t = decltype(&obs_properties_destroy);
	using properties_t = std::unique_ptr<obs_properties_t, properties_delete_t>;

protected:
	QWidget *widget = nullptr;
	properties_t properties;
	OBSData settings;
	void *obj = nullptr;
	std::string type;
	PropertiesReloadCallback reloadCallback;
	PropertiesUpdateCallback callback = nullptr;
	int minSize;
	int maxSize;
	std::vector<std::unique_ptr<WidgetInfo>> children;
	std::string lastFocused;
	QWidget *lastWidget = nullptr;
	bool deferUpdate;
	obs_property_type lastPropertyType = OBS_PROPERTY_INVALID;
	bool isDshowInput = false;
	int buttonIndex = 0;
	bool showFiltersBtn = true;
	bool showColorFilterPath = true;
	bool isColorFilter = false;
	bool colorFilterOriginalPressed = false;
	QHBoxLayout *hBtnLayout = nullptr;
	SliderIgnoreScroll *sliderView{};
	PLSSpinBox *spinsView{};
	WidgetInfo *infoView{};
	obs_property_t *colorFilterProperty{};
	QWidget *NewWidget(obs_property_t *prop, QWidget *widget, const char *signal);

	QWidget *AddCheckbox(obs_property_t *prop);
	QWidget *AddText(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	void AddFloat(obs_property_t *prop, QFormLayout *layout, QLabel **label);
	QWidget *AddList(obs_property_t *prop, bool &warning);
	void AddEditableList(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	QWidget *AddButton(obs_property_t *prop);
	void AddColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label);
	void AddFrameRate(obs_property_t *prop, bool &warning, QFormLayout *layout, QLabel *&label);

	void AddGroup(obs_property_t *prop, QFormLayout *layout);
	void AddProperty(obs_property_t *property, QFormLayout *layout);
	virtual void AddSpacer(const obs_property_type &currentType, QFormLayout *layout);
	void resizeEvent(QResizeEvent *event) override;

	void GetScrollPos(int &h, int &v);
	void SetScrollPos(int h, int v);
public slots:
	void ReloadProperties();
	virtual void RefreshProperties();
	void SignalChanged();
	void OnColorFilterOriginalPressed(bool state);

signals:
	void PropertiesResized();
	void Changed();
	void PropertiesRefreshed();
	void OpenFilters();

public:
	explicit PLSPropertiesView(OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback, int minSize = 0, int maxSize = -1,
				   bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false);
	explicit PLSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1, bool showFiltersBtn = false,
				   bool showColorFilterPath = true, bool colorFilterOriginalPressed = false);

	inline obs_data_t *GetSettings() const { return settings; }

	inline void UpdateSettings() { callback(obj, settings); }
	inline bool DeferUpdate() const { return deferUpdate; }

	void CheckValues();
};
