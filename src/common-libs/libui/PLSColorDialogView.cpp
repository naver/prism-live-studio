#include "PLSColorDialogView.h"

#include <qboxlayout.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qstyle.h>

#include "PLSDialogButtonBox.h"

#include <libutils-api.h>

class PLSColorDialogImpl : public QColorDialog {
public:
	PLSColorDialogImpl(PLSColorDialogView *cdv, const QColor &initial, QWidget *parent) : QColorDialog(initial, parent), colorDialogView(cdv)
	{
		setWindowFlags(Qt::Widget);

		if (layout()) {
			layout()->setSizeConstraint(QLayout::SetDefaultConstraint);
		}
		if (auto buttonBox = findChild<QDialogButtonBox *>(); buttonBox) {
			PLSDialogButtonBox::updateStandardButtonsStyle(buttonBox);
		}
	}

	void done(int result) override
	{
		QColorDialog::done(result);
		colorDialogView->done(result);
	}

private:
	PLSColorDialogView *colorDialogView;
};

PLSColorDialogView::PLSColorDialogView(QWidget *parent) : PLSColorDialogView(Qt::white, parent) {}

PLSColorDialogView::PLSColorDialogView(const QColor &initial, QWidget *parent) : PLSDialogView(parent)
{
	impl = pls_new<PLSColorDialogImpl>(this, initial, this->content());
	initSize(QSize(630, 635));

	QHBoxLayout *l = pls_new<QHBoxLayout>(this->content());
	l->setSpacing(0);
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(impl);

	connect(impl, &QColorDialog::currentColorChanged, this, &PLSColorDialogView::currentColorChanged);
	connect(impl, &QColorDialog::colorSelected, this, &PLSColorDialogView::colorSelected);
}

QColorDialog *PLSColorDialogView::colorDialog() const
{
	return impl;
}

void PLSColorDialogView::setCurrentColor(const QColor &color)
{
	impl->setCurrentColor(color);
}

QColor PLSColorDialogView::currentColor() const
{
	return impl->currentColor();
}

QColor PLSColorDialogView::selectedColor() const
{
	return impl->selectedColor();
}

void PLSColorDialogView::setOption(ColorDialogOption option, bool on)
{
	impl->setOption(option, on);
}

bool PLSColorDialogView::testOption(ColorDialogOption option) const
{
	return impl->testOption(option);
}

void PLSColorDialogView::setOptions(ColorDialogOptions options)
{

	impl->setOptions(options);
}

PLSColorDialogView::ColorDialogOptions PLSColorDialogView::options() const
{
	return impl->options();
}

void PLSColorDialogView::setVisible(bool visible)
{
	impl->setVisible(visible);
	PLSDialogView::setVisible(visible);
}

QColor PLSColorDialogView::PLSColorDialogView::getColor(const QColor &initial, QWidget *parent, const QString &title, ColorDialogOptions options)
{
	PLSColorDialogView dlg(initial, parent);
	if (!title.isEmpty()) {
		dlg.setWindowTitle(title);
	}
	/* The native dialog on OSX has all kinds of problems, like closing
     * other open QDialogs on exit, and
     * https://bugreports.qt-project.org/browse/QTBUG-34532
     */
#ifndef _WIN32
	options |= QColorDialog::DontUseNativeDialog;
#endif
	dlg.setOptions(options);
	dlg.setCurrentColor(initial);
	dlg.exec();
	return dlg.selectedColor();
}

int PLSColorDialogView::customCount()
{
	return PLSColorDialogImpl::customCount();
}

QColor PLSColorDialogView::customColor(int index)
{
	return PLSColorDialogImpl::customColor(index);
}

void PLSColorDialogView::setCustomColor(int index, QColor color)
{
	PLSColorDialogImpl::setCustomColor(index, color);
}

QColor PLSColorDialogView::standardColor(int index)
{
	return PLSColorDialogImpl::standardColor(index);
}

void PLSColorDialogView::setStandardColor(int index, QColor color)
{
	PLSColorDialogImpl::setStandardColor(index, color);
}
