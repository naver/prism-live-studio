#pragma once

#include "window-basic-properties.hpp"
#include <pls/pls-obs-api.h>

class PLSBasicProperties;
class PLSPropertiesView;
class OBSBasic;
class PLSLoadingEvent;
class PLSBasicProperties : public OBSBasicProperties {
	Q_OBJECT

public:
	PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flag);
	~PLSBasicProperties();
	void cancelSavePropertyData();

	//PRISM/FanZirong/20251103/PRISM_PC-3577/source capture failed guidance
	void setFailedCode(obs_source_failed_status_sub_code code);
	obs_source_failed_status_sub_code getFailedCode();

signals:
	void OpenMusicButtonClicked();

	void OpenFilters(OBSSource source);
	void OpenStickers(OBSSource source);
	void AboutToClose();

protected:
	void reject() override;
	void accept() override;
	void resizeEvent(QResizeEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
	void onReloadOldSettings() const;

private:
	void _customPreview();

	void ShowLoading();
	void HideLoading();
	void AsyncLoadTextmotionProperties();
	void asyncLoadChatWidgetproperties();
	void ShowPrismLensNaverRunNotice(bool isMobileSource);

	QLabel *labelQRImage;

public slots:
	void updatePreview();
	void showToast(const QString &message);
	void setToastMessage(const QString &message);
	void showGuideText(const QString &guideText);
	void hideGuideText();
	void setFailureState(const QString &failedText, bool needLoading);

private:
	QLabel *imagePreview;
	OBSSignal qrImageSignal;
	PLSLoadingEvent *m_pLoadingEvent = nullptr;
	QWidget *m_pWidgetLoadingBG = nullptr;
	OBSSignal updatePropertiesOKButtonSignal;
	unsigned operationFlags;

	QWidget *toast = nullptr;
	QLabel *toastLabel = nullptr;
	QPushButton *toastButton = nullptr;

	//PRISM/FanZirong/20251103/PRISM_PC-3577/source capture failed guidance
	obs_source_failed_status_sub_code failedCode = OBS_SOURCE_STATUS_SUCCESS;

	static void UpdatePropertiesOkButtonEnable(void *data, calldata_t *params);
	static void PropertyUpdateNotify(void *data, calldata_t *params);
	void updateToastGeometry();
	void updateToastPosition(const QRect &geometry);

	void dialogClosedToSendNoti();
};
