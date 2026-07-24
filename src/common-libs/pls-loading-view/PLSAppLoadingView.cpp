#include "PLSAppLoadingView.h"
#include "ui_PLSAppLoadingView.h"

#define WIDTH 340
#define HEIGHT 105

PLSAppLoadingView::PLSAppLoadingView(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSAppLoadingView)
{
	setupUi(ui);
	pls_add_css(this, {"PLSAppLoadingView"});
	setFixedSize(WIDTH, HEIGHT);
	initSize(WIDTH, HEIGHT);
	setResizeEnabled(false);
	setHasCloseButton(false);
	setHasMaxResButton(false);
	setHasCaption(false);
	setHasHelpButton(false);
	setHasMinButton(false);
	connect(&m_timer, &QTimer::timeout, this, &PLSAppLoadingView::pollingTextContent);
	m_timer.start(800);
}

PLSAppLoadingView::~PLSAppLoadingView()
{
	delete ui;
}
void PLSAppLoadingView::setPosition(const QString &parentGeometry)
{
	if (parentGeometry.isEmpty())
		return;
	QScreen *currentScreen = nullptr;
	QWidget temp;
	temp.restoreGeometry(QByteArray::fromBase64(QByteArray(parentGeometry.toUtf8())));
	QList<QScreen *> screens = QGuiApplication::screens();
	for (QScreen *screen : screens) {
		if (screen->geometry().contains(temp.frameGeometry().center())) {
			currentScreen = screen;
			break;
		}
	}
	if (!currentScreen)
		return;
	QRect screenGeometry = currentScreen->geometry();
	int x = screenGeometry.x() + (screenGeometry.width() - WIDTH) / 2;
	int y = screenGeometry.y() + (screenGeometry.height() - HEIGHT) / 2;

	move(x, y);
}
void PLSAppLoadingView::pollingTextContent()
{

	switch (m_currentIndex) {
	case 0:
		ui->label->setText(".");
		break;
	case 1:
		ui->label->setText("..");

		break;
	case 2:
		ui->label->setText("...");
		break;
	default:
		break;
	}
	m_currentIndex = (++m_currentIndex) % 3;
}
