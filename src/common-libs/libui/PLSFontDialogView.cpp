#include "PLSFontDialogView.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVariant>
#include <QPushButton>
#include <QEvent>

#include "PLSComboBox.h"
#include "PLSDialogButtonBox.h"

#include <libutils-api.h>
#include <QLineEdit.h>

class PLSFontDialogImpl : public QFontDialog {
public:
	PLSFontDialogImpl(PLSFontDialogView *cdv, const QFont &initial, QWidget *parent) : QFontDialog(initial, parent), fontDialogView(cdv)
	{
		setWindowFlags(Qt::Widget);

		if (auto buttonBox = findChild<QDialogButtonBox *>(); buttonBox) {
			PLSDialogButtonBox::updateStandardButtonsStyle(buttonBox);
		}
		for (auto &child : findChildren<QComboBox *>()) {
			child->setView(new PLSComboBoxListView());
		}
		for (auto &child : findChildren<QVBoxLayout *>()) {
			child->setSpacing(10);
		}

		QLineEdit *sampleEdit = this->findChild<QLineEdit *>("qt_fontDialog_sampleEdit");
		if (sampleEdit) {
			sampleEdit->setStyleSheet("QLineEdit{ min-height: 128px; max-height: 128px; border-radius: 2px;}");
		}
	}

	void done(int result) override
	{
		QFontDialog::done(result);
		fontDialogView->done(result);
	}

private:
	PLSFontDialogView *fontDialogView;
};

PLSFontDialogView::PLSFontDialogView(QWidget *parent) : PLSFontDialogView(parent->font(), parent) {}

PLSFontDialogView::PLSFontDialogView(const QFont &initial, QWidget *parent) : PLSDialogView(parent)
{
	impl = pls_new<PLSFontDialogImpl>(this, initial, this->content());
	initSize(QSize(630, 638));

	QHBoxLayout *l = pls_new<QHBoxLayout>(this->content());
	l->setSpacing(0);
	l->setContentsMargins(0, 0, 0, 0);
	l->addWidget(impl);

	connect(impl, &QFontDialog::currentFontChanged, this, &PLSFontDialogView::currentFontChanged);
	connect(impl, &QFontDialog::fontSelected, this, &PLSFontDialogView::fontSelected);
}

QFontDialog *PLSFontDialogView::fontDialog() const
{
	return impl;
}

void PLSFontDialogView::setCurrentFont(const QFont &font)
{
	impl->setCurrentFont(font);
}

QFont PLSFontDialogView::currentFont() const
{
	return impl->currentFont();
}

QFont PLSFontDialogView::selectedFont() const
{
	return impl->selectedFont();
}

void PLSFontDialogView::setOption(FontDialogOption option, bool on)
{
	impl->setOption(option, on);
}

bool PLSFontDialogView::testOption(FontDialogOption option) const
{
	return impl->testOption(option);
}

void PLSFontDialogView::setOptions(FontDialogOptions options)
{
	impl->setOptions(options);
}

PLSFontDialogView::FontDialogOptions PLSFontDialogView::options() const
{
	return impl->options();
}

void PLSFontDialogView::setVisible(bool visible)
{
	impl->setVisible(visible);
	PLSDialogView::setVisible(visible);
}

QFont PLSFontDialogView::getFont(bool *ok, QWidget *parent)
{
	QFont initial;
	return PLSFontDialogView::getFont(ok, initial, parent, QString());
}

QFont PLSFontDialogView::getFont(bool *ok, const QFont &initial, QWidget *parent, const QString &title, FontDialogOptions options)
{
	PLSFontDialogView dlg(initial, parent);
	dlg.setOptions(options);
	if (!title.isEmpty()) {
		dlg.setWindowTitle(title);
	}

	bool ret = (dlg.exec() || (options & QFontDialog::NoButtons));
	if (ok) {
		*ok = !!ret;
	}
	if (ret) {
		return dlg.selectedFont();
	} else {
		return initial;
	}
}
