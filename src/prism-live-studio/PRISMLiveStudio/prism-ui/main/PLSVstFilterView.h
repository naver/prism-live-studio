#ifndef PLSVSTFILTERVIEW_H
#define PLSVSTFILTERVIEW_H

#include <QWidget>
#include <QLabel>
#include <obs.hpp>
#include "PLSComboBox.h"
//#include<PLSDpiHelper.h>
#include <frontend-api.h>
#include <obs-source.h>
#include "loading-event.hpp"
#include "PLSPropertiesView.hpp"

namespace Ui {
class PLSVstFilterView;
}

class ComboBoxWithIcon : public PLSComboBox {
	Q_OBJECT

public:
	explicit ComboBoxWithIcon(QWidget *parent = nullptr);
	~ComboBoxWithIcon() final = default;

	void showIcon();
	void hideIcon();

private:
	QLabel *loadingIcon{nullptr};
};

class PLSVstFilterView : public QWidget {
	Q_OBJECT

public:
	explicit PLSVstFilterView(OBSData settings, OBSSource source, QWidget *parent = nullptr);
	~PLSVstFilterView() final;

	void resetProperties();
	static void onVstStateChanged(void *param, calldata_t *data);

private:
	void initProperties();
	void fill_out_plugins();
	void updateVstState(const QString &vst_path, int state);
	void updateTipsState(const QString &tipsStr, bool warning);
	void resetUi();
	void setLoadingVisible(bool visible);

private slots:
	void selectVstPlugin(int index);
	void onOpenVstInterface() const;
	void onOpenWhenActiveClicked(bool trigger) const;
	void onShowScrollBar(bool show);

public slots:
	void handleVstStateChange(const QString &vst_path, int vst_state, uint64_t source_ptr);

private:
	Ui::PLSVstFilterView *ui;
	OBSSource filterSource;
	OBSData filterSettings;
	PLSLoadingEvent loadingEvent;
};

#endif // PLSVSTFILTERVIEW_H
