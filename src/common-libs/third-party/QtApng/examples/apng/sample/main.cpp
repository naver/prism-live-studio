#include "widget.h"
#include <QApplication>
#include <QDir>
#include <QDebug>

int main(int argc, char *argv[])
{
#ifdef QTAPNG_PLUGIN_ROOT
	qputenv("QT_PLUGIN_PATH", QTAPNG_PLUGIN_ROOT + QString{QDir::listSeparator()}.toUtf8() + qgetenv("QT_PLUGIN_PATH"));
#endif
	qDebug() << "QT_PLUGIN_PATH" << qEnvironmentVariable("QT_PLUGIN_PATH");

	QApplication a(argc, argv);
	Widget w;
	w.show();

	return a.exec();
}
