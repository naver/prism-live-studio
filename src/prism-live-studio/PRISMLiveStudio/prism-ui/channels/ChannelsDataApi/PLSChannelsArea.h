#ifndef CHANNELSAREA_H
#define CHANNELSAREA_H

#include <QFrame>
#include <QVariantMap>
#include <memory>
#include "ChannelCommonFunctions.h"
#include "ChannelDefines.h"
#include "ui_ChannelsArea.h"
class QPushButton;
class PLSAddingFrame;
class GoLivePannel;
class QToolButton;

namespace Ui {
class ChannelsArea;
}

class PLSChannelsArea : public QFrame {
	Q_OBJECT

public:
	explicit PLSChannelsArea(QWidget *parent = nullptr);
	~PLSChannelsArea() override;

	void beginInitChannels();

	void holdOnChannelArea(bool holdOn);

signals:
	void sigNextInitialize();

private slots:

	void addChannel(const QString &channelUUID);

	void removeChannel(const QString &channelUUID);

	void updateChannelUi(const QString &channelUUID);

	void refreshChannels();

	void showChannelsAdd();

	void clearAll();

	void clearAllRTMP();

	void initializeNextStep();

	void endInitialize();

	void onFoldUpButtonClick();
	void onFoldDownButtonClick();

protected:
	void changeEvent(QEvent *e) override;

	void wheelEvent(QWheelEvent *event) override;

	bool eventFilter(QObject *watched, QEvent *event) override;

	void showEvent(QShowEvent *event) override;

private:
	enum class ScrollDirection { NOScroll, ForwardScroll, BackScroll };

	//private:
	ChannelData::ChannelCapsulePtr addChannel(const QVariantMap &channelInfo);
	ChannelData::ChannelFoldCapsulePtr addFoldChannel(const QVariantMap &channelInfo);

	QHash<void *, QTimer *> mTimerContainer;
	template<typename FunctionType> void delayTask(FunctionType function, int tick = 200)
	{
		auto pointer = getMemberPointer<void *>(function);
		QTimer *delayTimer = mTimerContainer.value(pointer);
		if (delayTimer == nullptr) {
			delayTimer = new QTimer(this);
			delayTimer->setSingleShot(true);
			auto func = std::bind(function, this);
			delayTimer->connect(delayTimer, &QTimer::timeout, this, [func]() { func(); });
			mTimerContainer.insert(pointer, delayTimer);
		}
		delayTimer->start(tick);
	}

	bool checkIsEmptyUi();
	void updateUi();
	void toReadyState();
	void delayUpdateUi();
	void updateAllChannelsUi();
	void delayUpdateAllChannelsUi();
	void hideLoading();

	void initScollButtons();
	void checkScrollButtonsState(ScrollDirection direction = ScrollDirection::ForwardScroll);
	bool isScrollButtonsNeeded() const;
	void displayScrollButtons(bool isShow = true);
	void enabledScrollButtons(bool isEnabled = true);
	void scrollNext(bool forwartStep = true);
	void ensureCornerChannelVisible(bool forwartStep = true) const;
	void buttonLimitCheck();

	void insertChannelCapsule(QWidget *wid, int index = -1) const;
	void insertFoldChannelCapsule(QWidget *wid, int index = -1) const;

	void refreshOrder() const;

	void initializeMychannels();

	int visibleCount();

	void createFoldButton();

	//private:
	std::unique_ptr<Ui::ChannelsArea> ui = std::make_unique<Ui::ChannelsArea>();

	QMap<QString, ChannelData::ChannelCapsulePtr> mChannelsWidget;
	QMap<QString, ChannelData::ChannelFoldCapsulePtr> m_FoldChannelsWidget;

	QPushButton *mLeftButon;

	QPushButton *mRightButton;

	PLSAddingFrame *mbusyFrame;

	bool isHolding = false;
	bool isUiInitialized = false;

	QList<QVariantMap> mInitializeInfos;

	QToolButton *myChannelsIconBtn;
	QPushButton *myChannelsTxtBtn;
	QPushButton *m_FoldUpButton;
	QPushButton *m_FoldDownButton;
	bool m_bFold = false;
};

#endif // CHANNELSAREA_H
