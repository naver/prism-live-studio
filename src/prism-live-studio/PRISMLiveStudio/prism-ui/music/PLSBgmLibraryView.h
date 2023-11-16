#ifndef PLSBGMLIBRARYVIEW_H
#define PLSBGMLIBRARYVIEW_H

#include "PLSBgmDataManager.h"
#include "PLSToastMsgFrame.h"
#include "PLSDialogView.h"
#include "loading-event.hpp"
#include "obs.hpp"
#include <QPushButton>
#include <QTimer>
#include <QNetworkAccessManager>
#include "PLSPushButton.h"

namespace Ui {
class PLSBgmLibraryView;
class PLSBgmLibraryItem;
}

class PLSBgmItemView;
class PLSBgmItemData;
class QLabel;
class PLSFloatScrollBarScrollArea;

class PLSBgmLibraryItem : public QFrame {
	Q_OBJECT
public:
	enum class MusicPlayState { Playing, Pause, Loading, Disabled, Selected, Default };
	Q_ENUM(MusicPlayState)

	explicit PLSBgmLibraryItem(const PLSBgmItemData &data, QWidget *parent = nullptr);
	~PLSBgmLibraryItem() override;

	void SetData(const PLSBgmItemData &data);
	void SetDurations(const int &full, const int &fiftten, const int &thirty, const int &sixty);
	void SetDurationType(int type);
	void SetDurationTypeSelected(int type);
	void SetDurationTypeClicked(int type);
	void SetDurationTypeSelected(const QString &url);
	void SetCheckedButtonVisible(bool visible);
	void SetDurationButtonVisible();
	void SetGroup(const QString &group);

	bool IsCurrentSong(const QString &title, int id) const;
	const PLSBgmItemData &GetData() const;
	QString GetTitle() const;
	QString GetProducer() const;
	QString GetGroup() const;
	QString GetUrl(int type) const;
	int GetDurationType() const;
	int GetDuration(int type) const;
	bool ContainUrl(const QString &url);

	static QString GetNameElideString(const QString &name, const QLabel *label);

protected:
	void leaveEvent(QEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *event) override;
#else
	void enterEvent(QEvent *event);
#endif
	bool eventFilter(QObject *object, QEvent *event) override;

private:
	void OnMouseEventChanged(const QString &state);
	int GetUrlCount() const;

private slots:
	void on_listenBtn_clicked();
	void on_checkBtn_clicked();
	void on_addBtn_clicked();
	void on_fifteenButton_clicked(bool checked);
	void on_thirtyButton_clicked(bool checked);
	void on_sixtyButton_clicked(bool checked);
	void on_fullButton_clicked(bool checked);

public slots:
	void onMediaStateChanged(const QString &url, obs_media_state state);

signals:
	void ListenButtonClickedSignal(PLSBgmLibraryItem *item);
	void AddButtonClickedSignal(PLSBgmLibraryItem *item);
	void CheckedButtonClickedSignal(PLSBgmLibraryItem *item);
	void DurationTypeChangeSignal(PLSBgmLibraryItem *item);

private:
	Ui::PLSBgmLibraryItem *ui;
	PLSBgmItemData data;
	bool mouseEnter{false};
	bool isChecked{false};
	int selectedCount = 0;
	PLSLoadingEvent loadingEvent;
};

class PLSBgmLibraryView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSBgmLibraryView(QWidget *parent = nullptr);
	~PLSBgmLibraryView() override;

	PLSBgmLibraryView(const PLSBgmLibraryView &) = delete;
	PLSBgmLibraryView &operator=(const PLSBgmLibraryView &) = delete;
	PLSBgmLibraryView(PLSBgmLibraryView &&) = delete;
	PLSBgmLibraryView &operator=(PLSBgmLibraryView &&) = delete;

	void initGroup();

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void OnListenButtonClicked(const PLSBgmLibraryItem *item);
	void OnAddButtonClicked(PLSBgmLibraryItem *item);
	void OnCheckedButtonClicked(PLSBgmLibraryItem *item);
	void OnDurationTypeChanged(PLSBgmLibraryItem *item);
	void OnOkButtonClicked();
	void OnCancelButtonClicked();
	void OnPreGroupButtonClicked();
	void OnNextGroupButtonClicked();
	void OnGroupScrolled();
	void OnGroupButtonClicked(const QString &group);

	static void MediaStateChanged(void *data, calldata_t *calldata);
	static void MediaLoad(void *data, calldata_t *calldata);

	void OnMediaStateChanged(const QString &url, obs_media_state state);
	void OnMediaLoad(const QString &url, bool load);
	void OnNetWorkStateChanged(bool isConnected);
	void OnDownloadJsonFailed();
	void DownloadMusicJson();

private:
	PLSBgmLibraryItem *CreateLibraryBgmItemView(const PLSBgmItemData &data, QWidget *parent = nullptr);
	void CreateListenSource();
	void CreateGroup(const QString &groupName);
	void ScrollToCategoryButton(const QPushButton *categoryButton) const;
	void RefreshSelectedGroupButtonStyle(const QString &groupName) const;

	// toast
	void initToast();
	void ShowToastView(const QString &text);
	void ResizeToastView();
	void ShowList(const QString &group);
	void CreateMusicList(const QString &group, const BgmLibraryData &listData);
	void UpdateMusiclist(const QString &group);
	void InitButtonState(const QString &group);
	void InitCategoryData() const;
	void UpdateSelectedString();
	bool CheckMusicResource() const;

signals:
	void AddCachePlayList(const QVector<PLSBgmItemData> &datas);
	void signal_meida_state_changed(const QString &url, obs_media_state state);

private:
	Ui::PLSBgmLibraryView *ui;
	QString selectedGroup{};
	QString listenUrl{};

	bool initGroupFinish{false};
	bool firstListen{true};

	OBSSource sourceAudition = nullptr;
	PLSToastMsgFrame toastView;

	QList<PLSCategoryButton *> listCatgaoryButton;
	std::map<QString, PLSFloatScrollBarScrollArea *> groupWidget;            //key category name
	std::map<QString, std::map<QString, PLSBgmLibraryItem *>> musicListItem; //key category name - value music item

	bool networkAvailable = true;
};

#endif // PLSBGMLIBRARYVIEW_H
