#ifndef RTMPCHANNELVIEW_HPP
#define RTMPCHANNELVIEW_HPP

#include <QDialog>
#include <QVariantMap>
#include "dialog-view.hpp"

namespace Ui {
class RtmpChannelView;
}

class PLSLoginInfo;

class PLSRtmpChannelView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSRtmpChannelView(QVariantMap &oldData, QWidget *parent = nullptr);
	~PLSRtmpChannelView();
	void initUi();
	QVariantMap SaveResult();
	void loadFromData(const QVariantMap &oldData);
	void showResolutionGuide();

protected:
	void changeEvent(QEvent *e);
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void updateSaveBtnAvailable();

	void on_SaveBtn_clicked();

	void on_CancelBtn_clicked();

	void on_StreamKeyVisible_toggled(bool);

	void on_PasswordVisible_toggled(bool);

	void on_RTMPUrlEdit_textChanged(const QString &);

	void on_PlatformCombbox_currentIndexChanged(const QString &text);

	void on_OpenLink_clicked();

private:
	void languageChange();
	void initCommbox();
	void verifyRename();
	bool checkIsModified();
	bool isInfoValid();
	void updateRtmpInfos();
	bool isRtmUrlRight();

	void updatePlatform(const QString &platform);

	void ValidateNameEdit();

private:
	Ui::RtmpChannelView *ui;
	QVariantMap mOldData;
	bool isEdit;
	QMap<QString, QString> mRtmps;
	QStringList mPlatforms;
};

#endif // RTMPCHANNELVIEW_HPP
