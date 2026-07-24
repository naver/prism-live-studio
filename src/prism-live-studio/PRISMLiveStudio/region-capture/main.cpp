#include "region-capture.h"
#include "region-capture-service.h"
#include <QApplication>
#include "libutils-api.h"

int main(int argc, char *argv[])
{
	// NOTE: we must disable qt hidpi scaling!!
	qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
	QApplication a(argc, argv);
	QGuiApplication::setWindowIcon(QIcon(":/images/PRISMLiveStudio.ico"));

	auto pipe_name = pls_cmdline_get_arg(argc, argv, "--pipe=");
	if (pipe_name.has_value()) {
		a.setQuitOnLastWindowClosed(false);
		QString pipeStr = QString::fromUtf8(pipe_name.value());
		pls_new<RegionCaptureService>(pipeStr, &a);
		return QApplication::exec();
	}

	auto max_width = pls_cmdline_get_arg(argc, argv, "--max-width=");
	auto max_height = pls_cmdline_get_arg(argc, argv, "--max-height=");
	if (!max_width.has_value() || !max_height.has_value())
		return 0;

	RegionCapture *capture = pls_new<RegionCapture>();
	capture->setWindowModality(Qt::ApplicationModal);
	capture->StartCapture(std::atoi(max_width.value()), std::atoi(max_height.value()));

	return QApplication::exec();
}
