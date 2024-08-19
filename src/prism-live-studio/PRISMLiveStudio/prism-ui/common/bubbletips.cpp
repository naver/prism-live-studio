#include "bubbletips.h"
#include "ui_bubbletips.h"
#include "libutils-api.h"
#include "liblog.h"
#include "PLSMainView.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QFile>

#define BUBBLE_LEFT_MARGIN 10
#define BUBBLE_TOP_MARGIN 10

#define BUBBLE_TEXT_BOTTOM_PADDING 20
#define BUBBLE_TEXT_LEFT_PADDING 10
#define BUBBLE_TEXT_TOP_PADDING 10
#define BUBBLE_TEXT_WIDTH 238

bubbletips::bubbletips(QWidget *buddy, BubbleTips::ArrowPosition pos, const QString &txt, int arrow_pos_offset, int duration_)
	: QFrame(buddy), ui(new Ui::bubbletips), mPos(pos), duration(duration_), offset(arrow_pos_offset), buddy(buddy)
{
	ui->setupUi(this);
	installEventFilter(this);
	initEventFilter();
	setWindowOpacity(0.95);
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
	ui->label->setFixedWidth(BUBBLE_TEXT_WIDTH + BUBBLE_TEXT_LEFT_PADDING * 2);
	ui->label->setText(txt);
	pls_async_call(this, [this, txt]() {
		QFontMetrics fontMetricsRight(font());
		QRect rect = fontMetricsRight.boundingRect(QRect(0, 0, BUBBLE_TEXT_WIDTH, 1000), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, txt);
		ui->label->setFixedHeight(rect.height() + BUBBLE_TEXT_TOP_PADDING + BUBBLE_TEXT_BOTTOM_PADDING);
		setFixedSize(ui->label->width() + BUBBLE_LEFT_MARGIN * 2, ui->label->height() + BUBBLE_TOP_MARGIN * 2);
	});
	generateArrow(mPos);
}

bubbletips::~bubbletips()
{
	timer.stop();
	delete ui;
}

void bubbletips::setArrowPosition(BubbleTips::ArrowPosition pos, int pos_offset)
{
	if (pos != mPos) {
		mPos = pos;
		generateArrow(pos);
	}

	if (offset != pos_offset) {
		offset = pos_offset;
	}
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
		timer.start(duration, this);
	});
}

void bubbletips::onTopLevelChanged(bool)
{
	uninitEventFilter();
	initEventFilter();

	if (buddy)
		doMove(buddy->mapToGlobal(displayPos));
}

void bubbletips::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	if (mArrow.isNull())
		return;

	painter.setPen(Qt::NoPen);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	int margin = ui->verticalLayout->contentsMargins().left();
	if (BubbleTips::Top == mPos) {
		QRect pos(margin + (ui->label->width() - mArrow.width()) * offset / 100, 1, mArrow.width(), mArrow.height());
		painter.drawImage(pos, mArrow);
	} else if (BubbleTips::Bottom == mPos) {
		QRect pos(margin + (ui->label->width() - mArrow.width()) * offset / 100, height() - mArrow.height() - 1, mArrow.width(), mArrow.height());
		painter.drawImage(pos, mArrow);
	} else if (BubbleTips::Left == mPos) {
		QRect pos(1, margin + (ui->label->height() - mArrow.height()) * offset / 100, mArrow.width(), mArrow.height());
		painter.drawImage(pos, mArrow);
	} else if (BubbleTips::Right == mPos) {
		QRect pos(width() - mArrow.width() - 1, margin + (ui->label->height() - mArrow.height()) * offset / 100, mArrow.width(), mArrow.height());
		painter.drawImage(pos, mArrow);
	}
}

void bubbletips::resizeEvent(QResizeEvent *event)
{
	if (auto currrent_dpi = devicePixelRatioF(); currrent_dpi * 100 != dpi) {
		generateArrow(mPos);
		dpi = currrent_dpi;
	}

	QFrame::resizeEvent(event);
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

void bubbletips::moveEvent(QMoveEvent *event)
{
	QFrame::moveEvent(event);
}

void bubbletips::showEvent(QShowEvent *event)
{
	if (buddy)
		doMove(buddy->mapToGlobal(displayPos));
	QFrame::showEvent(event);
}

bool bubbletips::eventFilter(QObject *watcher, QEvent *e)
{
	if (!buddy)
		return QFrame::eventFilter(watcher, e);

	if ((watcher == this || watcher == buddy) && e->type() == QEvent::MouseButtonPress) {
		timer.stop();
		close();
		return QFrame::eventFilter(watcher, e);
	}

	if (watcher == buddy || ancestors.find(watcher) != ancestors.end()) {
		auto type = e->type();
		if (type == QEvent::Resize) {
			if (auto currrent_dpi = buddy->devicePixelRatioF(); currrent_dpi * 100 != dpi) {
				generateArrow(mPos);
				dpi = currrent_dpi;
				update();
			}
			if (buddy)
				doMove(buddy->mapToGlobal(displayPos));
		} else if (type == QEvent::Move) {
			if (auto currrent_dpi = buddy->devicePixelRatioF(); currrent_dpi * 100 != dpi) {
				generateArrow(mPos);
				dpi = currrent_dpi;
				update();
			}
			if (buddy)
				doMove(buddy->mapToGlobal(displayPos));
		}
	}

	return QFrame::eventFilter(watcher, e);
}

void bubbletips::generateArrow(BubbleTips::ArrowPosition pos)
{
	switch (pos) {
	case BubbleTips::Top: {
		QImage pix({18, 10}, QImage::Format_RGBA8888);
		pix.fill(Qt::transparent);

		QPainter dc(&pix);
		dc.setRenderHint(QPainter::Antialiasing);

		QPoint p1(0, pix.height());
		QPoint p2(pix.width() / 2, 0);
		QPoint p3(pix.width(), pix.height());

		QPainterPath path;
		path.moveTo(p1);
		path.lineTo(p2);
		path.lineTo(p3);

		dc.fillPath(path, QColor("#666666"));

		mArrow = pix;

		break;
	}
	case BubbleTips::Right: {
		QImage pix({10, 18}, QImage::Format_RGBA8888);
		pix.fill(Qt::transparent);

		QPainter dc(&pix);
		dc.setRenderHint(QPainter::Antialiasing);
		dc.setPen(Qt::NoPen);

		QPoint p1(0, 0);
		QPoint p2(pix.width(), pix.height() / 2);
		QPoint p3(0, pix.height());

		QPainterPath path;
		path.moveTo(p1);
		path.lineTo(p2);
		path.lineTo(p3);

		dc.fillPath(path, QColor("#666666"));

		mArrow = pix;
		break;
	}
	case BubbleTips::Bottom: {
		QImage pix({18, 10}, QImage::Format_RGBA8888);
		pix.fill(Qt::transparent);

		QPainter dc(&pix);
		dc.setRenderHint(QPainter::Antialiasing);

		QPoint p1(0, 0);
		QPoint p2(pix.width() / 2, pix.height());
		QPoint p3(pix.width(), 0);

		QPainterPath path;
		path.moveTo(p1);
		path.lineTo(p2);
		path.lineTo(p3);

		dc.fillPath(path, QColor("#666666"));

		mArrow = pix;
		break;
	}
	case BubbleTips::Left: {
		QImage pix({10, 18}, QImage::Format_RGBA8888);
		pix.fill(Qt::transparent);

		QPainter dc(&pix);
		dc.setRenderHint(QPainter::Antialiasing);

		QPoint p1(pix.width(), 0);
		QPoint p2(0, pix.height() / 2);
		QPoint p3(pix.width(), pix.height());

		QPainterPath path;
		path.moveTo(p1);
		path.lineTo(p2);
		path.lineTo(p3);

		dc.fillPath(path, QColor("#666666"));

		mArrow = pix;
		break;
	}
	default:
		break;
	}
}

void bubbletips::doMove(const QPoint &pos)
{
	if (pos.isNull())
		return;

	int margin = ui->verticalLayout->contentsMargins().left();
	if (BubbleTips::Top == mPos) {
		move(pos.x() - ((ui->label->width() - mArrow.width()) * offset / 100) - margin - mArrow.width() / 2, pos.y());
	} else if (BubbleTips::Bottom == mPos) {
		move(pos.x() - ((ui->label->width() - mArrow.width()) * offset / 100) - margin - mArrow.width() / 2, pos.y() - height());
	} else if (BubbleTips::Left == mPos) {
		move(pos.x(), pos.y() - ((ui->label->height() - mArrow.height()) * offset / 100) - margin - mArrow.height() / 2);
	} else if (BubbleTips::Right == mPos) {
		move(pos.x() - width(), pos.y() - ((ui->label->height() - mArrow.height()) * offset / 100) - margin - mArrow.height() / 2);
	}
	raise();
}

void bubbletips::initEventFilter()
{
	if (toplevelWidget = buddy->window(); toplevelWidget) {
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

bool pls_show_bubble_tips(QWidget *buddy, BubbleTips::ArrowPosition arrowPos, const QPoint &local_pos, const QString &txt, int offset, int durationMs)
{
	if (!buddy)
		return false;

	if (buddy->visibleRegion().isEmpty())
		return false;

	QPointer<bubbletips> tips = pls_new<bubbletips>(buddy, arrowPos, txt, offset, durationMs);
	tips->moveTo(local_pos);
	return true;
}
