#pragma once

#include "window-basic-properties.hpp"

class PLSBasicProperties;
class PLSPropertiesView;
class OBSBasic;
class PLSLoadingEvent;
class PLSBasicProperties : public OBSBasicProperties {
	Q_OBJECT

public:
	PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flag);
	~PLSBasicProperties();

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

private slots:
	void onReloadOldSettings() const;

private:
	void _customPreview();

	void ShowLoading();
	void HideLoading();
	void AsyncLoadTextmotionProperties();
	void ShowMobileNotice();
	void ShowPrismLensNaverRunNotice(bool isMobileSource);

	QLabel *labelQRImage;

public slots:
	void updatePreview();
	void showToast(const QString &message);
	void setToastMessage(const QString &message);
	void showGuideText(const QString& guideText);
	void hideGuideText();

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

	static void UpdatePropertiesOkButtonEnable(void *data, calldata_t *params);
	static void PropertyUpdateNotify(void *data, calldata_t *params);
	void updateToastGeometry();
	void updateToastPosition(const QRect &geometry);

	void dialogClosedToSendNoti();
};
