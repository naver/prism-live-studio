#ifndef RTMPCHANNELVIEW_HPP
#define RTMPCHANNELVIEW_HPP

#include <QDialog>
#include <QVariantMap>
#include "PLSDialogView.h"

namespace Ui {
class RtmpChannelView;
}

class PLSLoginInfo;

class PLSRtmpChannelView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSRtmpChannelView(const QVariantMap &oldData, QWidget *parent = nullptr);
	~PLSRtmpChannelView() override;
	void initUi();
	QVariantMap SaveResult() const;
	void loadFromData(const QVariantMap &oldData);
	void showResolutionGuide();

	void setPlatformCombboxIndex(const QString &channleName);

	enum CustomChannelType { RTMP = 0, SRT = 1, RIST = 2, OTHER = 3 };

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void updateSaveBtnAvailable();

	void on_SaveBtn_clicked();

	void on_CancelBtn_clicked();

	void on_StreamKeyVisible_toggled(bool);

	void on_PasswordVisible_toggled(bool);

	void on_RTMPUrlEdit_textChanged(const QString &);

	void on_PlatformCombbox_currentTextChanged(const QString &showText);

	void on_ServerComboBox_currentTextChanged(const QString &text);

	void on_OpenLink_clicked() const;

private:
	void languageChange();
	void initCommbox();
	bool verifyRename();
	bool checkIsModified() const;
	bool isInfoValid();
	void updateRtmpInfos();
	bool isUrlRight(const QString &regular, const QString &url) const;

	void updatePlatform(const QVariantMap &oldData);

	void ValidateNameEdit();
	void IsHideSomeFrame(bool bShow);

	void UpdateTwitchServerList();

	void setTwitchUI(const QString &channelName);

	//private:
	Ui::RtmpChannelView *ui;
	QVariantMap mOldData;
	bool isEdit = false;
	QMap<QString, QString> mRtmps;
	QStringList mPlatforms;
	CustomChannelType m_type = RTMP;
};

#endif // RTMPCHANNELVIEW_HPP
