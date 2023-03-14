TEMPLATE = app

QT       += core gui widgets

TARGET = sample

HEADERS += \
		widget.h

SOURCES += \
		main.cpp \
		widget.cpp

FORMS += \
		widget.ui

!isEmpty(QTAPNG_MODULE_ROOT): DEFINES += "QTAPNG_PLUGIN_ROOT=\\\"$$QTAPNG_MODULE_ROOT/plugins\\\""

target.path = $$[QT_INSTALL_EXAMPLES]/apng/sample
INSTALLS += target

RESOURCES += \
	apng_sample.qrc
