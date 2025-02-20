#include "PLSGuideTipsframe.h"
#include "ui_guidetipsframe.h"
#include <qevent.h>
#include <qdebug.h>
#include <QPainterPath>
#include <qpainter.h>
#include <QtGlobal>
#include <QDockWidget>
#include "frontend-api.h"
#include <qdir.h>
#include "PLSGuidetipsConst.h"
#include "libui.h"
#include "ui-config.h"
#include "ChannelCommonFunctions.h"
#include "window-basic-main.hpp"
#include "PLSLaunchWizardView.h"
#include "PLSBasic.h"
#include "prism-version.h"
#include "pls-common-define.hpp"

using namespace guide_tip_space;

PLSGuideTipsFrame::PLSGuideTipsFrame(QWidget *parent) : BaseClass(parent), ui(new Ui::PLSGuideTipsFrame)
{
	ui->setupUi(this);
	this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	this->setAttribute(Qt::WA_TranslucentBackground);
	this->setAttribute(Qt::WA_DeleteOnClose);

	this->installEventFilter(this);

	pls_add_css(this, {"PLSGuideTipsframe"});
	connect(this, &PLSGuideTipsFrame::needToUpdate, this, &PLSGuideTipsFrame::updateUI, Qt::QueuedConnection);
}

PLSGuideTipsFrame::~PLSGuideTipsFrame()
{
	delete ui;
}

void PLSGuideTipsFrame::setText(const QString &text)
{
	mText = text;
	QString htmlTxt = g_tipTextHtml.arg(text);
	ui->TextLabel->setText(htmlTxt);
}

void PLSGuideTipsFrame::setAliginWidget(QWidget *widget)
{
	mAliginWidget = widget;
	if (mAliginWidget == nullptr) {
		return;
	}
	addListenedWidget(mAliginWidget);
	auto parW = mAliginWidget->parentWidget();
	if (parW == nullptr) {
		return;
	}
	addListenedWidget(parW);
}

void PLSGuideTipsFrame::setBackgroundWidget(QWidget *widget)
{
	mBackWindow = widget;
	if (mBackWindow == nullptr) {
		return;
	}
	addListenedWidget(mBackWindow);
	auto parW = mBackWindow->parentWidget();
	if (parW == nullptr) {
		return;
	}
	addListenedWidget(parW);
}

void PLSGuideTipsFrame::setWatchType(int type)
{
	mWatchType = type;
}

void PLSGuideTipsFrame::addListenedWidget(QWidget *widget)
{
	if (widget == nullptr || mListenedWidgets.contains(widget)) {
		return;
	}
	mListenedWidgets.append(widget);
	widget->installEventFilter(this);
	auto dock = dynamic_cast<QDockWidget *>(widget);
	if (dock) {
		this->setIsFloat(dock->isFloating());
		connect(dock, &QDockWidget::topLevelChanged, this, &PLSGuideTipsFrame::onDockStateChanged, Qt::QueuedConnection);
	}
}

void PLSGuideTipsFrame::setIsFloat(bool isFloat)
{
	isFloating = isFloat;
}

void PLSGuideTipsFrame::onDockStateChanged(bool isFloat)
{
	if (mWatchType == int(WatchType::DockType)) {
		this->close();
		return;
	}
	//aligin widget in dock ,not close
	setIsFloat(isFloat);
}

void PLSGuideTipsFrame::addListenedWidgets(const WidgetsPtrList &widgets)
{

	for (auto &widget : widgets) {
		addListenedWidget(widget);
	}
}

void PLSGuideTipsFrame::setBackgroundColor(const QColor &color)
{
	mBackgroundColor = color;
}

void PLSGuideTipsFrame::locate()
{
	this->show();
	checkPosition();
	updateUI();
	//delay show if window not visible
	if (mAliginWidget == nullptr || !mAliginWidget->isVisible()) {
		this->hide();
	}
}

bool PLSGuideTipsFrame::eventFilter(QObject *watched, QEvent *event)
{

	//listened widget event
	if (watched != this) {

		switch (event->type()) {
		case QEvent::Resize:
		case QEvent::Show:
		case QEvent::MouseMove:
			if (!isWidgetInView(mAliginWidget)) {
				this->close();
				break;
			}
			if (!this->isVisible()) {
				this->show();
			}
			if (!isFloating) {
				checkPosition();
			}
			aliginTo();
			break;
		case QEvent::WindowActivate:
			break;
		case QEvent::Hide:
			this->hide();
			break;

		case QEvent::Close:
			this->close();
			break;
		case QEvent::MouseButtonPress:
			GuideRegisterManager::instance()->closeAll();
			break;
		case QEvent::NonClientAreaMouseButtonPress:
			qDebug() << "  non ";
			break;
		default:
			break;
		}
		return QFrame::eventFilter(watched, event);
	}
	//self event
	switch (event->type()) {
	case QEvent::MouseButtonPress:
		GuideRegisterManager::instance()->closeAll();
		break;
	case QEvent::Show:
		if (!isWidgetInView(mAliginWidget)) {
			this->close();
			break;
		}
		emit needToUpdate();
		break;
	case QEvent::WindowActivate:

		break;
	case QEvent::NonClientAreaMouseButtonPress:
		qDebug() << "  non ";
		break;
	default:
		break;
	}
	return QFrame::eventFilter(watched, event);
}

void PLSGuideTipsFrame::updateUI()
{
	updateLayout();
	aliginTo();
	refreshTriangleImage();
	delayAligin();
}

void PLSGuideTipsFrame::updateLayout()
{
	int margin = mTriangleMargin;
	QSize verticalSize(16, 10);
	QSize horizontalSize(10, 16); //for left and right
	QSize triangleMinsize = verticalSize;
	switch (WindowPostion(mDirection)) {

	case WindowPostion::TopLeftPostion:
		ui->TriaLayout->setContentsMargins(isOnBorder ? margin : margin + 5, 0, 0, 0);
		ui->TriaLayout->setAlignment(Qt::AlignRight);
		ui->TriangleLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
		ui->gridLayout->addWidget(ui->TrianleFrame, 0, 0, Qt::AlignLeft);
		ui->gridLayout->addWidget(ui->contentFrame, 1, 0);

		break;
	case WindowPostion::NoPositon:
	case WindowPostion::BottomLeftPositon:
		ui->TriaLayout->setContentsMargins(isOnBorder ? margin : margin + 5, 0, 0, 0);
		ui->TriaLayout->setAlignment(Qt::AlignRight);
		ui->TriangleLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
		ui->gridLayout->addWidget(ui->contentFrame, 0, 0);
		ui->gridLayout->addWidget(ui->TrianleFrame, 1, 0, Qt::AlignLeft);

		break;
	case WindowPostion::LeftPosition:
		ui->TriaLayout->setContentsMargins(margin, 0, 0, 0);
		ui->TriaLayout->setAlignment(Qt::AlignRight);
		ui->TriangleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		ui->gridLayout->addWidget(ui->TrianleFrame, 0, 0);
		ui->gridLayout->addWidget(ui->contentFrame, 0, 1);
		ui->gridLayout->setColumnStretch(0, 1);
		ui->gridLayout->setColumnStretch(1, 10);
		triangleMinsize = horizontalSize;
		break;

	case WindowPostion::RightPosition:
		ui->TriaLayout->setContentsMargins(0, 0, 0, 0);
		ui->TriaLayout->setAlignment(Qt::AlignLeft);
		ui->TriangleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		ui->gridLayout->addWidget(ui->contentFrame, 0, 0);
		ui->gridLayout->addWidget(ui->TrianleFrame, 0, 1);

		ui->gridLayout->setColumnStretch(0, 10);
		ui->gridLayout->setColumnStretch(1, 1);
		triangleMinsize = horizontalSize;
		break;

	case WindowPostion::TopRightPostion:
		ui->TriaLayout->setContentsMargins(0, 0, isOnBorder ? margin : margin + 5, 0);
		ui->gridLayout->addWidget(ui->TrianleFrame, 0, 0, Qt::AlignRight);
		ui->TriangleLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
		ui->gridLayout->addWidget(ui->contentFrame, 1, 0);

		break;
	case WindowPostion::BottonRightPosition:
		ui->TriaLayout->setContentsMargins(0, 0, isOnBorder ? margin : margin + 5, 0);
		ui->TriaLayout->setAlignment(Qt::AlignLeft);
		ui->TriangleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		ui->gridLayout->addWidget(ui->contentFrame, 0, 0);
		ui->gridLayout->addWidget(ui->TrianleFrame, 1, 0, Qt::AlignRight);

		break;
	default:
		break;
	}
	auto dpiSize = triangleMinsize;
	ui->TriangleLabel->setMinimumSize(dpiSize);
	ui->TriangleLabel->setFixedSize(dpiSize);
	ui->TrianleFrame->setMaximumHeight(dpiSize.height());
}

void PLSGuideTipsFrame::checkPosition()
{
	auto tmpPosition = calculatePositon();
	setMyDirection(int(tmpPosition));
	TEST_DEBUG(" position " << int(tmpPosition))
}
void PLSGuideTipsFrame::checkTextLayout()
{
	auto oldSize = ui->TextLabel->size();
	auto fontMetr = ui->TextLabel->fontMetrics();
	auto rect = QRect(0, 0, 221, 1024);
	auto triaDiffer = QSize(0, ui->TrianleFrame->height());
	if (mDirection == int(WindowPostion::LeftPosition) || mDirection == int(WindowPostion::RightPosition)) {
		triaDiffer = QSize(ui->TrianleFrame->width(), 0);
	}

	TEST_DEBUG("label old size " << oldSize)
	TEST_DEBUG("label max rect " << rect)
	TEST_DEBUG("label text " << mText)
	auto tmpTxt = mText;

	auto boundRec = fontMetr.boundingRect(rect, Qt::TextWordWrap | Qt::TextIncludeTrailingSpaces | Qt::AlignTop | Qt::AlignLeft, tmpTxt);
	auto boundSize = boundRec.size();
	auto rowCount = (int)std::ceil(boundSize.height() / 20.0);
	auto dpiDiffer = QSize(0, rowCount * 1);
	TEST_DEBUG("boundSize " << boundSize)
	if (boundSize != oldSize) {

		auto contentSize = boundSize + QSize(12 /*left*/ + 30 /*new icon */ + 6 /*space */ + 11 /*right*/, 10 /*top*/ + 13 /*bottom*/);
		contentSize = contentSize + dpiDiffer;
		TEST_DEBUG("contentSize " << contentSize)

		auto thisSize = contentSize + triaDiffer;

		TEST_DEBUG("thisSize " << thisSize)
		this->resize(thisSize);
		return;
	}
}

bool PLSGuideTipsFrame::isLeftDock() const
{
	auto isDockType = mWatchType == int(WatchType::DockType);
	auto aliginW = dynamic_cast<QDockWidget *>(mAliginWidget);
	auto mainWindow = PLSBasic::instance();
	if (aliginW && mainWindow) {
		auto area = mainWindow->dockWidgetArea(aliginW);
		isDockType = isDockType && (area == Qt::LeftDockWidgetArea);
	}

	return isDockType;
}

PLSGuideTipsFrame::WindowPostion PLSGuideTipsFrame::calculatePositon()
{

	if (isFloating) {
		mDgree = 180;
		return WindowPostion::BottomLeftPositon;
	}

	auto background = mBackWindow;
	if (background == nullptr) {
		background = parentWidget();
	}
	if (background == nullptr) {
		mDgree = 180;
		return WindowPostion::BottomLeftPositon;
	}

	auto backRect = background->rect();
	auto rightBarRect = QRect(QPoint(backRect.width() - g_borderWidth, g_borderWidth), backRect.bottomRight() - QPoint(0, g_borderWidth));

	auto leftBorderRect = QRect(QPoint(0, 0), QPoint(g_borderWidth, backRect.height()));
	auto rightBorderRect = QRect(QPoint(backRect.width() - g_borderWidth, 0), backRect.bottomRight());

	TEST_DEBUG(" rect " << backRect)
	TEST_DEBUG(" right rect" << rightBarRect)

	auto tmpAligin = mAliginWidget == nullptr ? this : mAliginWidget;
	auto aliginPos = tmpAligin->mapToGlobal(tmpAligin->rect().center());
	auto myPos = background->mapFromGlobal(aliginPos);

	TEST_DEBUG(" pos " << myPos)
	auto sq1 = QRect(QPoint(0, 0), backRect.center());
	auto sq2 = QRect(QPoint(0, 0 + backRect.height() / 2), backRect.size() / 2);
	auto sq3 = QRect(QPoint(0 + backRect.width() / 2, 0), backRect.size() / 2);
	auto sq4 = QRect(backRect.center(), backRect.size() / 2);

	if (leftBorderRect.contains(myPos) || rightBorderRect.contains(myPos)) {
		isOnBorder = true;
	}
	bool isDock = isLeftDock();
	//left
	if (sq1.contains(myPos)) {

		if (isDock) {
			isOnBorder = true;
		}
		mDgree = isDock ? -90 : 0;
		return isDock ? WindowPostion::LeftPosition : WindowPostion::TopLeftPostion;
	}
	if (sq2.contains(myPos)) {
		if (isDock) {
			isOnBorder = true;
		}
		mDgree = isDock ? -90 : 180;
		return isDock ? WindowPostion::LeftPosition : WindowPostion::BottomLeftPositon;
	}

	//right
	if (rightBarRect.contains(myPos)) {
		mDgree = 90;
		isOnBorder = true;
		return WindowPostion::RightPosition;
	}

	if (sq3.contains(myPos)) {
		mDgree = 0;
		return WindowPostion::TopRightPostion;
	}
	if (sq4.contains(myPos)) {
		mDgree = 180;
		return WindowPostion::BottonRightPosition;
	}

	//out of rect
	mDgree = 180;

	return WindowPostion::NoPositon /*ui same as BottomLeftPositon*/;
}

void PLSGuideTipsFrame::setMyDirection(int newPosition)
{
	if (newPosition != mDirection) {
		mDirection = newPosition;
		emit needToUpdate();
	}
}
void PLSGuideTipsFrame::delayAligin()
{
	if (mDelayTimer == nullptr) {
		mDelayTimer = new QTimer(this);
		mDelayTimer->setSingleShot(true);
		mDelayTimer->setInterval(100);
		connect(mDelayTimer, &QTimer::timeout, this, &PLSGuideTipsFrame::aliginTo, Qt::QueuedConnection);
	}
	mDelayTimer->start();
}

void PLSGuideTipsFrame::aliginTo()
{
	if (mAliginWidget == nullptr) {
		return;
	}
	checkTextLayout();
	TEST_DEBUG("actual size " << ui->contentFrame->size())
	QPoint targetPos;
	QPoint triaPos;

	auto frameSize = mAliginWidget->frameSize();
	int height = frameSize.height();
	int width = frameSize.width();
	if (mWatchType == int(WatchType::WidgetType)) {
		height = 20;
	}
	switch (WindowPostion(mDirection)) {

	case WindowPostion::TopLeftPostion:
		targetPos = mAliginWidget->mapToGlobal(QPoint(width / 2, height));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(ui->TriangleLabel->width() / 2, 0 + (-2 - 2)));

		break;
	case WindowPostion::NoPositon:
	case WindowPostion::BottomLeftPositon:
		targetPos = mAliginWidget->mapToGlobal(QPoint(width / 2, 0));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(ui->TriangleLabel->width() / 2, ui->TriangleLabel->height()));
		break;
	case WindowPostion::LeftPosition:
		targetPos = mAliginWidget->mapToGlobal(QPoint(width, height / 2));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(0, ui->TriangleLabel->height() / 2));
		break;

	case WindowPostion::RightPosition:

		targetPos = mAliginWidget->mapToGlobal(QPoint(0, height / 2));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(ui->TriangleLabel->width() + 6, ui->TriangleLabel->height() / 2));
		break;

	case WindowPostion::TopRightPostion:

		targetPos = mAliginWidget->mapToGlobal(QPoint(width / 2, height));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(ui->TriangleLabel->width() / 2, 0));
		break;
	case WindowPostion::BottonRightPosition:

		targetPos = mAliginWidget->mapToGlobal(QPoint(width / 2, 0));
		triaPos = ui->TriangleLabel->mapToGlobal(QPoint(ui->TriangleLabel->width() / 2, ui->TriangleLabel->height()));
		break;
	default:
		break;
	}

	auto differ = targetPos - triaPos;
	auto aPos = this->mapToGlobal(QPoint(0, 0)) + differ;
	this->move(aPos);
}

void PLSGuideTipsFrame::refreshTriangleImage()
{
	auto size = ui->TriangleLabel->size();
	TEST_DEBUG(" triangle size " << size)
	int maxV = qMax(size.width(), size.height());

	size = QSize(maxV, maxV);
	QPainterPath path;
	int widDif = 2;
	int heightDif = size.height() / 9 + 1;
	auto pos0 = QPoint(size.width() / 2, heightDif);
	auto pos0T = pos0 + QPointF(-widDif, heightDif + 1);
	auto pos0B = pos0 + QPointF(widDif, heightDif + 1);
	auto pos1 = QPointF(0, size.height());
	auto pos2 = QPointF(size.width(), size.height());

	path.moveTo(pos0T);
	path.lineTo(pos0);
	path.lineTo(pos0B);

	path.lineTo(pos2);
	path.lineTo(pos1);
	auto center = QPoint(size.width() / 2, size.height() / 2);
	path.moveTo(center);

	QImage image(size, QImage::Format_RGBA8888);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.translate(center);

	painter.rotate(mDgree);
	painter.translate(-center);
	painter.setClipPath(path);

	painter.fillPath(path, mBackgroundColor);

	ui->TriangleLabel->setPixmap(QPixmap::fromImage(image));
}

void GuideRegisterManager::registerGuide(const QString &sourceText, const QString &aliginWidgetName, const QString &refrenceWidget, const QStringList &otherListenedWidgets, int watchType,
					 const QString &displayOS)
{
	GuideRegister reg;
	reg.sourceText = sourceText;
	reg.aliginWidgetName = aliginWidgetName;
	reg.refrenceWidget = refrenceWidget;
	reg.otherListenedWidgets = otherListenedWidgets;
	reg.watchType = watchType;
	reg.displayOS = displayOS;

	registerGuide(reg);
}

void GuideRegisterManager::registerGuide(const QString &sourceText, WidgetPtr aliginWidget, WidgetPtr refrenceWidget, const WidgetsPtrList &otherListenedWidgets, int watchType,
					 const QString &displayOS)
{
	GuideRegister reg;
	reg.sourceText = sourceText;
	reg.aliginWidget = aliginWidget;
	reg.refrenceWidgetP = refrenceWidget;
	reg.watchType = watchType;
	reg.otherListenedWidgetsPtrs = otherListenedWidgets;
	reg.displayOS = displayOS;
	reg.isMatched = true;
	registerGuide(reg);
}

void GuideRegisterManager::closeAll()
{
	if (mCover == nullptr) {
		return;
	}
	if (mRegisters.isEmpty()) {
		if (mCover) {
			mCover->close();
			mCover->deleteLater();
		}
		return;
	}

	static int lastTime = QTime::currentTime().msecsSinceStartOfDay();
	auto differ = QTime::currentTime().msecsSinceStartOfDay() - lastTime;
	lastTime = QTime::currentTime().msecsSinceStartOfDay();
	bool isWidzardVisible = PLSLaunchWizardView::instance()->isVisible() && !PLSLaunchWizardView::instance()->isMinimized();

	if (differ < 500 && isWidzardVisible) {
		PLS_INFO("guide tip", "is holding");
		mCover->raise();
		mCover->activateWindow();
		return;
	}

	//act close all
	auto closeTip = [](const GuideRegister &reg) {
		if (reg.mTipUi) {
			reg.mTipUi->close();
			reg.mTipUi->deleteLater();
		}
	};
	std::for_each(mRegisters.begin(), mRegisters.end(), closeTip);
	mRegisters.clear();
	mCover->close();
	mCover->deleteLater();
}

bool GuideRegisterManager::eventFilter(QObject *, QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonPress:
		PLS_INFO("guide ", "close all try first");
		this->closeAll();
		break;
	default:
		break;
	}
	return false;
}

void GuideRegisterManager::registerGuide(const GuideRegister &reg)
{
	mRegisters.insert(reg.sourceText, reg);
}

//only one
QWidget *getWidget(const QString &objName, QWidget *topWidget)
{
	if (topWidget == nullptr) {
		return nullptr;
	}
	if (topWidget->objectName() == objName) {
		return topWidget;
	}
	return topWidget->findChild<QWidget *>(objName);
}

//recursive
QWidget *getWidget(const QStringList &objNamePath, QWidget *topWidget)
{
	QWidget *ret = topWidget;
	for (const auto &name : objNamePath) {
		ret = getWidget(name, ret);
	}
	return ret;
}

//only top
QWidget *getWidgetFromTop(const QString &objName)
{
	auto widgets = QApplication::topLevelWidgets();
	QWidget *childP = nullptr;
	auto isMatched = [&objName, &childP](QWidget *wid) {
		auto child = getWidget(objName, wid);
		if (child != nullptr) {
			childP = child;
			return true;
		}
		return false;
	};

	auto ret = std::find_if(widgets.cbegin(), widgets.cend(), isMatched);
	if (ret == widgets.cend()) {
		TEST_DEBUG(" ret " << objName << "null ")
		return nullptr;
	}
	return childP;
}

//top for path
QWidget *getWidgetFromTop(const QStringList &objNamePath)
{
	QWidget *ret = nullptr;
	auto top = getWidgetFromTop(objNamePath.first());
	if (top != nullptr) {
		ret = getWidget(objNamePath, top);
	}
	return ret;
}

WidgetsPtrList getWidgets(const QStringList &objNames)
{
	WidgetsPtrList ret;
	for (const auto &obj : objNames) {
		auto wid = getWidgetFromTop(obj);
		if (wid) {
			ret.append(wid);
		}
	}

	return ret;
}

GuideRegisterManager *GuideRegisterManager::instance()
{
	static GuideRegisterManager manager;
	return &manager;
}

void GuideRegisterManager::load()
{
	auto displayVer = config_get_string(App()->GlobalConfig(), common::NEWFUNCTIONTIP_CONFIG, common::CONFIG_DISPLAYVERISON);
	if (pls_is_equal(displayVer, PLS_VERSION)) {
		return;
	}
	auto tipsFile = RegisterJsonPath;
	QJsonObject obj;
	pls_read_json(obj, tipsFile);
	if (obj.isEmpty()) {
		return;
	}

	auto tips = obj.value(g_tipsContent).toArray();
	for (auto tip : tips) {
		auto reg = convertJsonToGuideRegister(tip.toObject());

#if defined(Q_OS_MACOS)
		if (reg.displayOS.compare(g_onlyDisplayWin, Qt::CaseInsensitive) == 0) {
			continue;
		}
#else
		if (reg.displayOS.compare(g_onlyDisplayMac, Qt::CaseInsensitive) == 0) {
			continue;
		}
#endif
		registerGuide(reg);
	}

	config_set_string(App()->GlobalConfig(), common::NEWFUNCTIONTIP_CONFIG, common::CONFIG_DISPLAYVERISON, PLS_VERSION);
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
}

void GuideRegisterManager::createCover()
{
	pls_check_app_exiting();
#if defined(Q_OS_MACOS)
	auto cover = new QLabel(nullptr, Qt::Dialog | Qt::FramelessWindowHint);
	cover->setStyleSheet("background-color:transparent;");
#else
	auto cover = new QLabel(pls_get_main_view(), Qt::Dialog | Qt::FramelessWindowHint);
	cover->setWindowOpacity(0.01);
#endif
	cover->installEventFilter(this);
	cover->show();

#if defined(Q_OS_MACOS)
	cover->setGeometry(pls_get_main_view()->frameGeometry());
#else
	cover->setGeometry(pls_get_main_view()->geometry());
#endif

	mCover = cover;
}

GuideRegister convertJsonToGuideRegister(const QJsonObject &obj)
{
	GuideRegister reg;
	reg.sourceText = obj.value(g_sourceText).toString();
	reg.aliginWidgetName = obj.value(g_aliginWidgetName).toString();
	reg.refrenceWidget = obj.value(g_refrenceWidget).toString();
	reg.otherListenedWidgets = obj.value(g_otherListenedWidgets).toString().split(",");
	reg.displayOS = obj.value(g_displayOS).toString();
	auto enumStr = obj.value(g_watchType).toString();
	auto watchTypeEnum = QMetaEnum::fromType<PLSGuideTipsFrame::WatchType>();
	reg.watchType = int(watchTypeEnum.keyToValue(enumStr.toUtf8().constData()));
	QString info = "this new guide display os is %1";
	PLS_INFO("GuideRegister", info.arg(reg.displayOS).toUtf8().constData());
	return reg;
}

void buildGuideTip(GuideRegister &reg)
{
	//current version matched
	if (reg.isMatched) {
		return;
	}

	// aligin
	TEST_DEBUG(" matching ")
	auto aliginPath = reg.aliginWidgetName.split(".", Qt::SkipEmptyParts);
	auto *alig = getWidgetFromTop(aliginPath);

	if (alig == nullptr) {
		return;
	}
	reg.aliginWidget = alig;

	//reference
	auto refrenceWidgetPath = reg.refrenceWidget.split(".", Qt::SkipEmptyParts);
	auto *reference = getWidgetFromTop(refrenceWidgetPath);
	TEST_DEBUG("try match re" << reg.refrenceWidget)
	if (reference == nullptr) {
		return;
	}
	reg.refrenceWidgetP = reference;

	//other
	reg.otherListenedWidgetsPtrs = getWidgets(reg.otherListenedWidgets);
	reg.isMatched = true;
}

void GuideRegisterManager::buildGuideTips()
{
	std::for_each(mRegisters.begin(), mRegisters.end(), buildGuideTip);
}

void GuideRegisterManager::beginShow()
{
	GuideRegisterManager::instance()->buildGuideTips();

	auto showAll = [this]() {
		int time = 10;
		auto create = [&time](GuideRegister &reg) {
			pls_check_app_exiting();
			auto tip = createTipFrameFromRegister(reg);
			tip->hide();
			QTimer::singleShot(time, tip, [tip]() {
				PLS_INFO("GuideRegisterManager", "singleShot create tip");
				pls_check_app_exiting();
				tip->locate();
			});
		};
		std::for_each(mRegisters.begin(), mRegisters.end(), create);
		emit allShowed();
		if (!mRegisters.isEmpty()) {
			QTimer::singleShot(100, this, &GuideRegisterManager::createCover);
		}
		bool isDontShow = config_get_bool(App()->GlobalConfig(), common::LAUNCHER_CONFIG, common::CONFIG_DONTSHOW);
		if (!isDontShow) {
			PLS_INFO(MAINFRAME_MODULE, "bringLauncherToFront the prism is shown");
			PLSLaunchWizardView::instance()->firstShow();
		}
	};
	QTimer::singleShot(500, pls_get_main_view(), showAll);
}

PLSGuideTipsFrame *createTipFrameFromRegister(GuideRegister &reg)
{
	auto guide = new PLSGuideTipsFrame(reg.aliginWidget);
	guide->setAliginWidget(reg.aliginWidget);
	guide->setBackgroundWidget(reg.refrenceWidgetP);
	guide->setBackgroundColor(QColor(255, 255, 255, 244));
	guide->addListenedWidgets(reg.otherListenedWidgetsPtrs);
	guide->setWatchType(reg.watchType);
	guide->setText(QCoreApplication::translate("GuideTip", reg.sourceText.toUtf8().constData()));

	reg.mTipUi = guide;
	return guide;
}
