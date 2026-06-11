#ifndef GUIDETIPSFRAME_H
#define GUIDETIPSFRAME_H

#include <QFrame>
#include <qmap.h>
#include <qlist.h>
#include <qpointer.h>
#include <qjsonobject.h>

#include <qobject.h>
#include <QMetaEnum>

class PLSGuideTipsFrame;
using BaseClass = QFrame;
using TipsPtr = QPointer<PLSGuideTipsFrame>;
using WidgetPtr = QPointer<QWidget>;
using WidgetsPtrList = QList<WidgetPtr>;

namespace Ui {
class PLSGuideTipsFrame;
}

class PLSGuideTipsFrame : public BaseClass {
	Q_OBJECT

public:
	enum class WindowPostion { NoPositon /*same as BottomLeftPositon*/, LeftPosition, RightPosition, TopLeftPostion, BottomLeftPositon, TopRightPostion, BottonRightPosition };

	enum class WatchType { WidgetType, ButtonType, DockType, InnerDock };

	Q_ENUM(WatchType);

	explicit PLSGuideTipsFrame(QWidget *parent = nullptr);
	~PLSGuideTipsFrame() override;

	void setText(const QString &text);
	void setAliginWidget(QWidget *widget);
	void setBackgroundWidget(QWidget *widget);
	void setWatchType(int type);
	void addListenedWidget(QWidget *widget);
	void addListenedWidgets(const WidgetsPtrList &widgets);
	void setBackgroundColor(const QColor &color);

	void locate();

	bool eventFilter(QObject *watched, QEvent *event) override;

signals:
	void needToUpdate();

private slots:
	void setIsFloat(bool isFloat = true);
	void onDockStateChanged(bool isFloating = true);
	void updateUI();

private:
	void updateLayout();
	void checkPosition();
	void checkTextLayout();
	WindowPostion calculatePositon();
	void setMyDirection(int newPosition);
	void aliginTo();
	void delayAligin();
	void refreshTriangleImage();
	bool isLeftDock() const;

	//private:
	Ui::PLSGuideTipsFrame *ui = nullptr;
	int mDirection = 0;
	int mDgree = 0;
	bool isFloating = false;
	bool isOnBorder = false;
	QWidget *mAliginWidget = nullptr;
	int mWatchType = int(WatchType::ButtonType);

	//widgets for events
	QWidgetList mListenedWidgets;
	//widget which for rect
	QWidget *mBackWindow = nullptr;
	int mTriangleMargin = 10;
	QColor mBackgroundColor{"#666666"};

	QTimer *mDelayTimer = nullptr;

	QString mText;
};

struct GuideRegister {
	QString sourceText;
	QString aliginWidgetName;
	QString refrenceWidget;
	QStringList otherListenedWidgets;
	QString displayOS;

	int watchType = int(PLSGuideTipsFrame::WatchType::ButtonType);

	bool isMatched = false; //mark for find widgets bellow
	WidgetPtr aliginWidget = nullptr;
	WidgetPtr refrenceWidgetP = nullptr;
	WidgetsPtrList otherListenedWidgetsPtrs;

	TipsPtr mTipUi = nullptr;
};

QWidget *getWidgetFromTop(const QString &objName);
QWidget *getWidgetFromTop(const QStringList &objNamePath);

QWidget *getWidget(const QString &objName, QWidget *topWidget);
QWidget *getWidget(const QStringList &objNamePath, QWidget *topWidget);

WidgetsPtrList getWidgets(const QStringList &objNames);

template<typename CheckType = QWidget *, typename SrcType = QWidget *> bool isTypeX(SrcType wid)
{
	return dynamic_cast<CheckType>(wid) != nullptr;
}

GuideRegister convertJsonToGuideRegister(const QJsonObject &obj);
void buildGuideTip(GuideRegister &reg);
PLSGuideTipsFrame *createTipFrameFromRegister(GuideRegister &reg);

class GuideRegisterManager : public QObject {
	Q_OBJECT
public:
	static GuideRegisterManager *instance();
	void load();
	void createCover();
	void beginShow();
	void buildGuideTips();
	void registerGuide(const GuideRegister &reg);

	void registerGuide(const QString &sourceText, const QString &aliginWidgetName, const QString &refrenceWidget, const QStringList &otherListenedWidgets,
			   int watchType = int(PLSGuideTipsFrame::WatchType::ButtonType), const QString &displayOS = "");

	void registerGuide(const QString &sourceText, WidgetPtr aliginWidget, WidgetPtr refrenceWidget, const WidgetsPtrList &otherListenedWidgets,
			   int watchType = int(PLSGuideTipsFrame::WatchType::ButtonType), const QString &displayOS = "");

	void closeAll();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

signals:
	void allShowed();

private:
	QMap<QString, GuideRegister> mRegisters;

	WidgetPtr mCover;
};

#endif // GUIDETIPSFRAME_H
