#ifndef RESOLUTIONGUIDEPAGE_H
#define RESOLUTIONGUIDEPAGE_H

#include <QFrame>
#include "PLSDialogView.h"
#include <QLabel>
#include <QPointer>
#include "PLSPressRestoreButton.hpp"
#include "ui_ResolutionGuidePage.h"
#include <memory>
#include "libresource.h"

namespace Ui {
class ResolutionGuidePage;
}

struct Resolution {
	int width = 1280;
	int height = 720;
	int fps = 30;
	int bitrate = 0;
	int keyframeInterval = -1;
};

struct B2BResolutionPara {
	QString serviceName;
	QString templateName;
	QString output_FPS;
	QString bitrate;
	QString keyframeInterval;
	QString streamingPresetThumbnail;
};

class ResolutionGuidePage : public PLSDialogView {
	Q_OBJECT

public:
	explicit ResolutionGuidePage(QWidget *parent = nullptr);
	~ResolutionGuidePage() override;
	static ResolutionGuidePage *createInstance(QWidget *parent = nullptr);

	//get data
	static const QVariantList &getResolutionsList();
	static QString getPreferResolutionStringOfPlatform(const QString &platform);
	static Resolution getPreferResolutionOfPlatform(const QString &platform);
	static Resolution parserStringOfResolution(const QString &resolution);
	static bool isAcceptToChangeResolution(const QString &platform, const QString &resolution);

	static bool setResolution(int width, int height, int fps, bool toChangeCanvas = true, bool bVerticalOutput = false);
	static bool setResolution(const Resolution &resolution, bool toChangeCanvas = true, bool bVerticalOutput = false);
	static bool setVideoBitrateAndKeyFrameInterval(int bitrate, int keyframeInterval);

	static void showAlertOnSetResolutionFailed();

	//function for after resolution changed
	using UpdateCallback = std::function<void()>;

	//function to agree change resolution
	using IsUserNeed = std::function<bool(const QString &, const QString &)>;

	static void setVisibleOfGuide(QWidget *parent, const UpdateCallback &callback = nullptr, const IsUserNeed & = isAcceptToChangeResolution, bool isVisible = true);

	static bool setUsingPlatformPreferResolution(const QString &platform);

	static void showResolutionGuideCloseAfterChange(QWidget *parent);

	static void checkResolution(QWidget *parent, const QString &uuid);
	static void checkResolutionForPlatform(QWidget *parent, const QString &platform, int channelType = 0, bool bVerticalOutput = false);
	static bool isCurrentResolutionFitableFor(const QString &platform, int channelType = 0, bool bVerticalOutput = false);
	static void showIntroduceTip(QWidget *parent, const QString &platformName);
	enum B2BApiError { EmptyList = 0, ReturnFail, NetworkError, ServiceDisable };
	enum OutputState { NoState = 0x0, OuputIsOff = 0x1, StreamIsOn = 0x2, RecordIsOn = 0x4, ReplayBufferIsOn = 0x8, VirtualCamIsOn = 0x10, OutPutError = 0x20, OtherPluginOutputIsOn = 0x40 };
	Q_DECLARE_FLAGS(OutputStates, OutputState)

	static OutputStates getCurrentOutputState();

	template<typename ParentType> static QWidget *createResolutionButtonsFrame(ParentType *parent, bool bNcp = false)
	{
		auto frame = new QFrame();
		auto layout = new QHBoxLayout();

		auto txtButon = new PLSRestoreCheckBox(frame);
		txtButon->setObjectName("ToResolutionBtn");
		txtButon->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		// tr same as settings ,resolution guide page
		if (bNcp) {
			txtButon->setText(QObject::tr("ResolutionGuide.NCP.MainTitle"));
		} else {
			txtButon->setText(QObject::tr("ResolutionGuide.MainTitle"));
		}
		txtButon->connect(txtButon, &QAbstractButton::clicked, parent, &ParentType::showResolutionGuide);
		layout->addWidget(txtButon);
		layout->setAlignment(txtButon, Qt::AlignRight);

		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(5);
		frame->setLayout(layout);

		frame->setFrameShape(QFrame::NoFrame);
		frame->setObjectName("ResolutionFrame");

		return frame;
	}

	void setAgreeFunction(const IsUserNeed &fun) { isUserNeed = fun; }
	void setUpdateResolutionFunction(const UpdateCallback &callback) { onUpdateResolution = callback; }

	struct CannotTipObject {
		QWidget *mParentWidget = nullptr;
		QPointer<QLabel> mTip = nullptr;
		QSize mFixSize;
		QPoint mMoveDistance;
		bool isValid = false;
		bool isCanChange = false;
		QString mText;

		bool checkIsCanChange();
		void updateUI(bool autoShow = true);
		void updateText();
		void updateGeometry();
	};

	static CannotTipObject createCannotTipForWidget(QWidget *parentWidget, const QSize &fixSize, const QPoint &moveDistance);

signals:
	void visibilityChanged(bool isVisible);
	void resolutionUpdated();
	void sigSetResolutionFailed();
	void downloadThumbnailFinish();

public slots:
	void onUserSelectedResolution(const QString &txt);
	void on_B2BTab_clicked();
	void on_otherTab_clicked();
	void on_updateButton_clicked();

protected:
	void changeEvent(QEvent *e) override;
	bool event(QEvent *event) override;

private slots:
	void on_CloseBtn_clicked();
	void updateItemsState();
	void updateSpace(bool isAdd = true);

private:
	void connectMainView();

	void initialize();
	void loadSettings();
	void saveSettings() const;

	void adjustLayout();
	bool initializeB2BItem();
	void showB2BErrorLabel(const int errorCode);
	QList<B2BResolutionPara> parseServiceStreamingPreset(QJsonObject &object);
	void UpdateB2BUI();
	QString getFilePath(const QString &fileName);
	void handThumbnail();
	//private
	std::unique_ptr<Ui::ResolutionGuidePage> ui = std::make_unique<Ui::ResolutionGuidePage>();
	static QVariantList mResolutions;
	IsUserNeed isUserNeed;
	UpdateCallback onUpdateResolution;
	QLabel *mTipLabel = nullptr;
	CannotTipObject mCannotTip;
	QSpacerItem *mSpaceItem = nullptr;
	bool mB2BLogin = false;
	QList<B2BResolutionPara> mB2BResolutionParaList;
	std::list<pls::rsm::UrlAndHowSave> m_urlAndHowSaves;
	bool m_updateRequestExisted{false};
	bool m_downLoadRequestExisted{false};
};

#endif // RESOLUTIONGUIDEPAGE_H
