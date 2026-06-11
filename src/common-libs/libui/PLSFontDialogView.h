#ifndef _PRISM_COMMON_LIBHDPI_FONTDIALOGVIEW_H
#define _PRISM_COMMON_LIBHDPI_FONTDIALOGVIEW_H

#include <QFontDialog>
#include "libui.h"
#include "PLSDialogView.h"

class PLSFontDialogImpl;

class LIBUI_API PLSFontDialogView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSFontDialogView(QWidget *parent = nullptr);
	explicit PLSFontDialogView(const QFont &initial, QWidget *parent = nullptr);
	~PLSFontDialogView() override = default;

	using FontDialogOption = QFontDialog::FontDialogOption;
	using FontDialogOptions = QFontDialog::FontDialogOptions;

	QFontDialog *fontDialog() const;

	void setCurrentFont(const QFont &font);
	QFont currentFont() const;

	QFont selectedFont() const;

	void setOption(FontDialogOption option, bool on = true);
	bool testOption(FontDialogOption option) const;
	void setOptions(FontDialogOptions options);
	FontDialogOptions options() const;

	void setVisible(bool visible) override;

	static QFont getFont(bool *ok, QWidget *parent = nullptr);
	static QFont getFont(bool *ok, const QFont &initial, QWidget *parent = nullptr, const QString &title = QString(), FontDialogOptions options = FontDialogOptions());

Q_SIGNALS:
	void currentFontChanged(const QFont &font);
	void fontSelected(const QFont &font);

private:
	PLSFontDialogImpl *impl;

	friend class PLSFontDialogImpl;
};

#endif // _PRISM_COMMON_LIBHDPI_FONTDIALOGVIEW_H
