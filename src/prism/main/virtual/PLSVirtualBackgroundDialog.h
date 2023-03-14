#pragma once

#include <dialog-view.hpp>
#include <QWidget>
#include "obs.hpp"
#include "window-basic-main.hpp"
#include "PLSMotionErrorLayer.h"

namespace Ui {
class PLSVirtualBackgroundDialog;
}

using namespace std;

class PLSVirtualBackgroundDialog : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSVirtualBackgroundDialog(DialogInfo info, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSVirtualBackgroundDialog();

	void setSourceSelect(const QString &sourceName, OBSSceneItem item);
	void reloadVideoDatas(OBSSceneItem forceShowItem = nullptr);
	void setSourceVisible(const QString &sourceName, OBSSceneItem item, bool visible);

	void setBgFileError(const QString &sourceName);

	void vrRunderProcessExit(void *source);
	void vrReloadCurrentData(void *source);

	void setPreviewCallback(bool set);

private:
	void setupSingals();
	void setupFirstUI();

	void updateUI();
	void updateSwitchImageEditModelUI();
	void updateBackgroundUI(bool isFromMotionList = false, bool isFromNowMethod = false);
	void updateBlurPageUI();
	void updateImageEditUI();

	void reInitDisplay();
	void clearSourceDisplay();

	void reloadVideoComboboxList(const DShowSourceVecType &list, OBSSceneItem forceShowItem = nullptr);

	static void drawPreview(void *data, uint32_t cx, uint32_t cy);
	static void drawBorder(void *data, uint32_t cx, uint32_t cy, float scale);
	static void drawBackground();

	void setCurrentSourceItem(OBSSceneItem item);
	bool isSourceComboboxVisible(const OBSSceneItem item);

	int findText(const OBSSceneItem item);
	bool setCurrentIndex(OBSSceneItem item);
	int getSourceComboboxVisibleCount();
	void closeDialogSave();
	template<typename T> void initSlider(QSlider *slider, const T &min, const T &max, const T &step, const T &currentValue, Qt::Orientation ori = Qt::Horizontal);

	void isEnterTempOriginModel(bool isTempModel);
	void foundAndSetShouldSetSource();
	void onResetImageData(bool needSendToCore = true);
	void setEditSliderRange(int min, int max);

	void showChromakeyToast();
	void hideChromakeyToast();
	void adjustChromakeyTipSize(int superWidth = 0);

	void showFPSToast();
	void hideFPSToast();
	bool adjustFPSTipSize(int superWidth = 0);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	virtual void closeEvent(QCloseEvent *event) override;
	virtual bool eventFilter(QObject *obj, QEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;

public slots:
	void onSceneChanged();
	void onSourceRenamed();
	void onCurrentResourceFileIsInvaild(const QStringList &list);

private slots:

	void onSourceComboxIndexChanged(int index);

	void onBlurControlButtonClicked();
	void onBlurValueChanged(int value);

	void onImageEidtButtonClicked();
	void onMotionCheckBoxClicked(int state);

	void onMinButtonClicked();
	void onMaxButtonClicked();
	void onZoomSliderValueChanged(int value);
	void onFlipVertiButtonClicked();
	void onFlipHoriButtonClicked();

	void onCloseEditButtonClicked();
	void onSaveEditButtonClicked();

	//top right
	void onOriginBackgroundClicked();
	void onClearBackgroundButtonClicked();

	void onCurrentMotionListResourceChanged(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
						const QString &foregroundPath, const QString &foregroundStaticImgPath);

signals:
	void virtualViewVisibleChanged(bool);
	void currentSourceChanged(const QString &name, OBSSceneItem item);
	void virtualViewChromaKeyClicked(OBSSource source);

private:
	Ui::PLSVirtualBackgroundDialog *ui;
	bool isChromaKeyFirstOpened{true};
	bool sliderValueInit{false};
	bool delayShowFpsToast{false};
	QRect renderViewportRect;
	QRect beginDragVBkgImgRect;
	static gs_texture_t *bg_texture;
	bool m_ignoreChangeIndex{true};
	bool m_isFirstShow{true};
	bool m_motionViewCreated{false};
	PLSMotionErrorLayer *m_chromakeyToast{nullptr};
	PLSMotionErrorLayer *m_fpsToast{nullptr};
};
