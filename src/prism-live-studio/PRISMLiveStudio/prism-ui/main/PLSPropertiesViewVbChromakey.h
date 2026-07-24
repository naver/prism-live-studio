#pragma once
#include "properties-view.hpp"
#include "PLSLoadingButton.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include "PLSPropertiesView.hpp"

//-----------------------------------------------------------------------------------------------------------
class DescLabel : public QLabel {
	Q_OBJECT
public:
	explicit DescLabel(const QString &text, QWidget *container, QWidget *parent = nullptr);

	void setText(const QString &);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	void UpdateHeight();

	QWidget *containerWidget = nullptr;
};

//-----------------------------------------------------------------------------------------------------------
class PLSVbChromakey : public QWidget {
	Q_OBJECT

	friend PLSPropertiesView;

public:
	explicit PLSVbChromakey(PLSPropertiesView &view, OBSSource source, int index, QWidget *parent = nullptr);
	~PLSVbChromakey();

public slots:
	void OnFilterAddOrRemove(OBSSource filter);

protected:
	static void OnSourceFilterAdded(void *param, calldata_t *data);
	static void OnSourceFilterRemoved(void *param, calldata_t *data);

	void RegisterEvents();

	void OnLensVbChanged(bool vbRemoved);
	void UpdateVbButtonState();

	void OnVbButtonClicked();
	void SwitchVbState();

	void OnChromakeyButtonClicked();

	bool CheckChromakey(OBSSource source);
	void ClearChromakey();
	void UpdateChromakeyButtonText();

	void AppendDesc(QVBoxLayout *vLayout);
	QLabel *CreateDescLabel(QWidget *parent, QWidget *container, const QString &text);
	QWidget *CreateDescLine(int fixedHeight, const QString &text);

	const int lensIndex;
	PLSPropertiesView &propertiesView;
	OBSSource source = nullptr;
	bool isVbRemoved = false;
	PLSLoadingVbButton *vbButton = nullptr;
	QPushButton *chromakeyButton = nullptr;

	OBSSignal addSignal;
	OBSSignal removeSignal;
	bool isChromakeyAdded = false;
};
