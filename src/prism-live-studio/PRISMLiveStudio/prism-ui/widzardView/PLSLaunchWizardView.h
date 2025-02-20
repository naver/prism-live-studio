#ifndef PLSLAUNCHWIZARDVIEW_H
#define PLSLAUNCHWIZARDVIEW_H

#include <QWidget>
#include <qbuttongroup.h>
#include <qpointer.h>
#include "PLSWindow.h"
#include <qsystemtrayicon.h>
#include <qscrollarea.h>
#include <qlayoutitem.h>
#include <QHBoxLayout>
#include <qjsonobject.h>
#include "libresource.h"
namespace Ui {
class PLSLaunchWizardView;
}
class PLSWizardInfoView;
class PLSLaunchWizardView : public PLSWindow {
	Q_OBJECT

public:
	static PLSLaunchWizardView *instance();

	explicit PLSLaunchWizardView(QWidget *parent = nullptr);
	~PLSLaunchWizardView() override;
	void updateParent(QWidget *wid);

	void updateUserInfo();
	void onPrismMessageCome(const QVariantHash &params);

	bool getIsShowFlag() { return m_bShowFlag; }

signals:
	void mouseClicked(QObject *obj);
	void sigTryGetScheduleList();
	void loadBannerFailed();
	void bannerImageLoadFinished(const std::list<pls::rsm::DownloadResult> &results);
	void bannerJsonDownloaded();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	virtual void closeEvent(QCloseEvent *event) override;

private:
	void setUserInfo(const QVariantMap &info = QVariantMap());
	void createBannerScrollView();
	void maskWidget(QWidget *wid);
	void createLiveInfoView();
	void createAlertInfoView();
	void createBlogView();
	void createQueView();

	void loadBannerSources();
	void updatebannerView();

	void initBannerView(const QStringList &paths);
	void createBanner(const QString &path, int index = -1);
	void updateBannerPath(int index, const QString &path) const;

	bool isInWidgets(const QWidget *wid) const;
	int currentIndex() const;
	void setCurrentIndex(int index = 0, bool useAnimation = true) const;
	QLayoutItem *itemAt(int index) const;
	QWidget *widgetAt(int index) const;
	QLayoutItem *currentItem() const;
	QWidget *currentLabel() const;

	void moveWidget(int index, bool pre = true);

	void handlePrismState(const QVariantMap &body) const;
	void handleChannelMessage(const QVariantMap &body);
	void checkShowErrorAlert(const QVariantMap &body);

	void handleErrorMessgage(const QVariantMap &body);
	void setErrorViewVisible(bool visible = true);

	void clearDumpInfos() const;
	void onWidgetClicked(QObject *obj) const;

	void updateTipsHtml();

	void checkStackOrder() const;
	void getBannerJson();

public slots:
	void singletonWakeup();
	void hideView();
	void updateLangcherTips();
	void firstShow(QWidget *parent = nullptr);

private slots:
	void startUpdateBanner();
	void finishedDownloadBanner(const std::list<pls::rsm::DownloadResult> &results);
	void changeBannerView();
	void checkBannerButtonsState();
	void delayCheckButtonsState();
	void updateGuideButtonState(bool on = true);
	void onUrlButtonClicked() const;
	void scheduleClicked();
	void updateInfoView(const QString &title = QString(), const QString &timeStr = QString(), const QString &plaform = QString()) const;
	void onDumpCreated();
	void wheelChangeBannerView(bool bPre);

private:
	//private:
	Ui::PLSLaunchWizardView *ui;

	QButtonGroup m_bannerBtnGroup;
	QPointer<PLSWizardInfoView> m_liveInfoView;
	QPointer<PLSWizardInfoView> m_alertInfoView;
	QPointer<QScrollArea> scrollArea;
	QPointer<QHBoxLayout> scrollLay;

	static QPointer<PLSLaunchWizardView> g_wizardView;

	//banner param
	QMap<QString, QString> mBannerUrls;
	QMap<QString, QString> mLinks;

	bool isLoadBannerSuccess = false;
	bool isLoadingBanner = false;
	bool isParsaring = false;
	QSharedPointer<QTimer> mBannerTimer;

	bool isRefresh = false;
	QSharedPointer<QTimer> mDelayTimer;

	bool m_bNeedSwapItem = false;

	int m_UpdateCount = 0;
	bool m_bShowFlag = false;
	QString m_josnPath;
};

QString formatTimeStr(const QDateTime &time);

#endif // PLSLAUNCHWIZARDVIEW_H
