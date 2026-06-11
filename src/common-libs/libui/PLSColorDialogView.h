#ifndef PLSCOLORDIALOGVIEW_H
#define PLSCOLORDIALOGVIEW_H

#include <qcolordialog.h>

#include "PLSDialogView.h"

class PLSColorDialogImpl;

class LIBUI_API PLSColorDialogView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSColorDialogView(QWidget *parent = nullptr);
	explicit PLSColorDialogView(const QColor &initial, QWidget *parent = nullptr);
	~PLSColorDialogView() override = default;

	using ColorDialogOption = QColorDialog::ColorDialogOption;
	using ColorDialogOptions = QColorDialog::ColorDialogOptions;

	QColorDialog *colorDialog() const;

	void setCurrentColor(const QColor &color);
	QColor currentColor() const;

	QColor selectedColor() const;

	void setOption(ColorDialogOption option, bool on = true);
	bool testOption(ColorDialogOption option) const;
	void setOptions(ColorDialogOptions options);
	ColorDialogOptions options() const;

	void setVisible(bool visible) override;

	static QColor getColor(const QColor &initial = Qt::white, QWidget *parent = nullptr, const QString &title = QString(), ColorDialogOptions options = ColorDialogOptions());

	static int customCount();
	static QColor customColor(int index);
	static void setCustomColor(int index, QColor color);
	static QColor standardColor(int index);
	static void setStandardColor(int index, QColor color);

Q_SIGNALS:
	void currentColorChanged(const QColor &color);
	void colorSelected(const QColor &color);

private:
	PLSColorDialogImpl *impl;

	friend class PLSColorDialogImpl;
};

#endif // PLSCOLORDIALOGVIEW_H
