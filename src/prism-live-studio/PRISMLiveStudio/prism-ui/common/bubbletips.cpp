#include "bubbletips.h"
#include "ui_bubbletips.h"
#include "libutils-api.h"
#include "liblog.h"
#include "PLSMainView.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QFile>
#include "pls-shared-functions.h"

#ifdef __APPLE__
#include "PLSCustomMacWindow.h"
#endif

#define BUBBLE_BOTTOM_MARGIN 10

#define BUBBLE_TEXT_LEFT_PADDING 15
#define BUBBLE_TEXT_RIGHT_PADDING 0
#define BUBBLE_TEXT_BOTTOM_PADDING 14
#define BUBBLE_TEXT_TOP_PADDING 10
#define BUBBLE_AUDIO_TRACK_TEXT_WIDTH 330
#define BUBBLE_AUDIO_TRACK_TIPS_WIDTH 376

#define BUBBLE_ARROW_WIDTH 16
#define BUBBLE_ARROW_HEIGHT 9
#define BUBBLE_ARROW_BOTTOM 1
#define BUBBLE_ARROW_SCALE 4

bubbletips::bubbletips(QWidget *buddy, const QString &txt, int arrow_pos_offset, int duration_, BubbleTips::TipsType tipsType)
	: QFrame(buddy), ui(new Ui::bubbletips), duration(duration_), m_arrowOffset(arrow_pos_offset), buddy(buddy)
{

	ui->setupUi(this);
	installEventFilter(this);
	initEventFilter();
	pls_add_css(this, {"PLSBubbleTips"});
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_DeleteOnClose);

	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	connect(ui->label, &QLabel::linkActivated, this, &bubbletips::onLinkActivated);
	ui->label->setText(txt);
	ui->label->setOpenExternalLinks(false);
	ui->label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);

	pls_async_call(this, [this]() { updateSize(); });

	generateArrow();
}

bubbletips::~bubbletips()
{
#ifdef __APPLE__
    //Under normal circumstances, if the program does not exit, calling Mac to remove the parent window will not cause a crash. However, if the program is exiting, a recursive call to createWinId will appear at the bottom of Qt.
    //After skipping removeCurrentWindowFromParentWindow, the program has been proposed. At this time, all program resources will be recycled by the system.
    if (!pls_get_app_exiting()) {
        PLSCustomMacWindow::removeCurrentWindowFromParentWindow(this);
    }
#endif
	timer.stop();
	delete ui;
}

void bubbletips::moveTo(const QPoint &local_pos)
{
	if (!buddy)
		return;

	displayPos = local_pos;

	auto pos = buddy->mapToGlobal(local_pos);
	pls_async_call(this, [this, pos]() {
		doMove(pos);
		show();
		if (duration != bubbleInfiniteLoopDuration) {
			timer.start(duration, this);
		}
	});
}

void bubbletips::addListenDockTitleWidget(PLSDockTitle *dockTitleWidget)
{
	m_titleWidget = dockTitleWidget;
	if (m_titleWidget) {
		m_titleWidget->installEventFilter(this);
	}
}

void bubbletips::onLinkActivated(const QString &link)
{
	timer.stop();
	close();
	emit clickTextLink(link);
}

void bubbletips::on_bubbleCloseButton_clicked()
{
	timer.stop();
	close();
}

void bubbletips::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	if (m_arrow.isNull())
		return;

	painter.setPen(Qt::NoPen);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect(m_arrowOffset, height() - BUBBLE_ARROW_HEIGHT - BUBBLE_ARROW_BOTTOM, BUBBLE_ARROW_WIDTH, BUBBLE_ARROW_HEIGHT);
	painter.drawPixmap(rect, m_arrow);
}

void bubbletips::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == timer.timerId()) {
		timer.stop();
		close();
		return;
	}
	QFrame::timerEvent(event);
}

void bubbletips::showEvent(QShowEvent *event)
{
#ifdef _WIN32
    if (buddy)
        doMove(buddy->mapToGlobal(displayPos));
#elif defined(__APPLE__)
    pls_async_call(this, [this]() {
        if (buddy)
            doMove(buddy->mapToGlobal(displayPos));
    });
    PLSCustomMacWindow::addCurrentWindowToParentWindow(this);
#endif
	QFrame::showEvent(event);
}

void bubbletips::generateArrow()
{
	QPixmap logoPix(pls_shared_paint_svg(QStringLiteral(":/resource/images/audio-mixer/audio-control/icon-arrow_tooltip.svg"),
					     QSize(BUBBLE_ARROW_WIDTH * BUBBLE_ARROW_SCALE, BUBBLE_ARROW_HEIGHT * BUBBLE_ARROW_SCALE)));
	m_arrow = logoPix;
}

void bubbletips::doMove(const QPoint &pos)
{
	if (pos.isNull())
		return;

	move(pos.x(), pos.y() - height());
	update();
}

bool bubbletips::eventFilter(QObject *watcher, QEvent *e)
{
	if (!buddy)
		return QFrame::eventFilter(watcher, e);

	//Filters events if this object has been installed as an event filter for the watched object.
	//In your reimplementation of this function, if you want to filter the event out, i.e.stop it being handled further, return true; otherwise return false
	if (!watcher || !e) {
		return false;
	}

	if (watcher == m_titleWidget && e->type() == QEvent::MouseButtonPress) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(e);
		if (mouseEvent->button() == Qt::LeftButton) {
			close();
			return QFrame::eventFilter(watcher, e);
		}
	}

	if (watcher == buddy || ancestors.find(watcher) != ancestors.end()) {
		auto type = e->type();
		if (type == QEvent::Resize || type == QEvent::Move) {
			if (buddy)
				doMove(buddy->mapToGlobal(displayPos));
		}
	}

	return QFrame::eventFilter(watcher, e);
}

void bubbletips::onTopLevelChanged(bool)
{
	close();
}

void bubbletips::initEventFilter()
{
	QPointer<QWidget> toplevelWidget = buddy->window();
	if (toplevelWidget) {
		toplevelWidget->installEventFilter(this);
		if (toplevelWidget->inherits("QDockWidget")) {
			auto dock = qobject_cast<QDockWidget *>(toplevelWidget);
			connect(dock, &QDockWidget::topLevelChanged, this, &bubbletips::onTopLevelChanged, Qt::UniqueConnection);
		}
		QWidget *pParent = parentWidget();
		while (pParent) {
			if (pParent->inherits("QDockWidget")) {
				auto dock = qobject_cast<QDockWidget *>(pParent);
				connect(dock, &QDockWidget::topLevelChanged, this, &bubbletips::onTopLevelChanged, Qt::UniqueConnection);
			}
			pParent->installEventFilter(this);
			ancestors.insert(pParent);
			pParent = pParent->parentWidget();
			if (pParent == toplevelWidget)
				break;
		}
		ancestors.insert(toplevelWidget);
	}
}

void bubbletips::uninitEventFilter()
{
	for (const auto &obj : ancestors) {
		obj->removeEventFilter(this);
	}
	ancestors.clear();
}

void bubbletips::updateSize()
{
	QTextDocument textDoc;
	textDoc.setDefaultFont(ui->label->font());
	textDoc.setTextWidth(BUBBLE_AUDIO_TRACK_TEXT_WIDTH);
	textDoc.setHtml(ui->label->text());
	QSizeF size = textDoc.size();

	ui->bubbleBg->setFixedSize(BUBBLE_AUDIO_TRACK_TIPS_WIDTH, size.height() + BUBBLE_TEXT_BOTTOM_PADDING + BUBBLE_TEXT_TOP_PADDING);
	setFixedSize(BUBBLE_AUDIO_TRACK_TIPS_WIDTH, ui->bubbleBg->height() + BUBBLE_BOTTOM_MARGIN);
}

bubbletips *pls_show_bubble_tips(QWidget *buddy, const QPoint &local_pos, const QString &txt, int offset, int durationMs, BubbleTips::TipsType tipsType)
{
	if (!buddy)
		return nullptr;

	if (buddy->visibleRegion().isEmpty())
		return nullptr;

	QPointer<bubbletips> tips = pls_new<bubbletips>(buddy, txt, offset, durationMs, tipsType);
	tips->moveTo(local_pos);
	return tips;
}
