#ifndef RESOLUTIONGUIDEPAGE_H
#define RESOLUTIONGUIDEPAGE_H

#include <QFrame>
#include "dialog-view.hpp"

namespace Ui {
class ResolutionGuidePage;
}

struct Resolution {
	int width = 1280;
	int height = 720;
	int fps = 30;
};

class ResolutionGuidePage : public PLSDialogView {
	Q_OBJECT

public:
	explicit ResolutionGuidePage(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~ResolutionGuidePage();
	static ResolutionGuidePage *createInstance(QWidget *parent = nullptr);

	//get data
	static const QVariantList &getResolutionsList();
	static const QString getPreferResolutionStringOfPlatform(const QString &platform);
	static const Resolution getPreferResolutionOfPlatform(const QString &platform);
	static const Resolution parserStringOfResolution(const QString &resolution);
	static bool isAcceptToChangeResolution(const QString &platform, const QString &resolution);

	static void setResolution(int width, int height, int fps, bool toChangeCanvas = true);
	static void setResolution(const Resolution &resolution, bool toChangeCanvas = true);

	//function for after resolution changed
	using UpdateCallback = std::function<void()>;

	//function to agree change resolution
	using IsUserNeed = std::function<bool(const QString &, const QString &)>;

	static void setVisibleOfGuide(QWidget *parent, UpdateCallback callback = nullptr, IsUserNeed = isAcceptToChangeResolution, bool isVisible = true);

	static void setUsingPlatformPreferResolution(const QString &platform);

	static void showResolutionGuideCloseAfterChange(QWidget *parent);

	static void checkResolution(QWidget *parent, const QString &uuid);
	static bool isCurrentResolutionFitableFor(const QString &platform, int channelType = 0);
	static void showIntroduceTip(QWidget *parent, const QString &platformName);

	template<typename ParentType> static QWidget *createResolutionButtonsFrame(ParentType *parent)
	{
		auto frame = new QFrame(parent);
		auto layout = new QHBoxLayout();

		auto txtButon = new QCheckBox(frame);
		txtButon->setObjectName("ToResolutionBtn");
		txtButon->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		// tr same as settings ,resolution guide page
		txtButon->setText(QObject::tr("ResolutionGuide.MainTitle"));
		txtButon->connect(txtButon, &QAbstractButton::clicked, parent, &ParentType::showResolutionGuide);
		layout->addWidget(txtButon);
		layout->setAlignment(txtButon, Qt::AlignRight);

		layout->setMargin(0);
		layout->setSpacing(5);
		frame->setLayout(layout);

		frame->setFrameShape(QFrame::NoFrame);
		frame->setObjectName("ResolutionFrame");

		return frame;
	}

	void setAgreeFunction(IsUserNeed fun) { isUserNeed = fun; }
	void setUpdateResolutionFunction(UpdateCallback callback) { onUpdateResolution = callback; }

signals:
	void visibilityChanged(bool isVisible);
	void resolutionUpdated();

public slots:
	void onUserSelectedResolution(const QString &txt);

protected:
	void changeEvent(QEvent *e);
	bool event(QEvent *event);

private slots:
	void on_CloseBtn_clicked();

private:
	void connectMainView();

	void initialize();
	void loadSettings();
	void saveSettings();

private:
	Ui::ResolutionGuidePage *ui;
	static QVariantList mResolutions;
	IsUserNeed isUserNeed;
	UpdateCallback onUpdateResolution;
};

#endif // RESOLUTIONGUIDEPAGE_H
