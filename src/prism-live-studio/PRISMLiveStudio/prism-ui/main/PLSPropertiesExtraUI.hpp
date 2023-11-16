#pragma once

#include <qpushbutton.h>
#include <QMargins>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <qcheckbox.h>
#include <qevent.h>
#include "PLSDialogView.h"
#include "obs.hpp"
#include "pls/pls-properties.h"
#include "PLSCheckBox.h"

class QMovie;
class QButtonGroup;
class QString;
class PLSDialogView;
class PLSComboBox;

class ChatTemplate : public QPushButton {
public:
	ChatTemplate(QButtonGroup *buttonGroup, int id, bool checked);
	~ChatTemplate() override = default;

protected:
	bool event(QEvent *event) override;
};

class TMTextAlignBtn : public QPushButton {
public:
	explicit TMTextAlignBtn(const QString &labelObjStr, bool isChecked = false, bool isAutoExcusive = true, QWidget *parent = nullptr);
	~TMTextAlignBtn() final = default;

protected:
	bool event(QEvent *e) override;

private:
	QLabel *m_iconLabel;
	bool m_isChecked;
};
class TMCheckBox : public PLSCheckBox {

public:
	explicit TMCheckBox([[maybe_unused]] const QWidget *parent = nullptr){};
	~TMCheckBox() final = default;

protected:
	bool event(QEvent *e) override
	{
		switch (e->type()) {
		case QEvent::HoverEnter:
			if (!isEnabled() && !isChecked() && (0 == objectName().compare("checkBox", Qt::CaseInsensitive))) {
				setToolTip(QObject::tr("textmotion.background.tooltip"));
			}
			break;
		default:
			break;
		}
		return PLSCheckBox::event(e);
	}
};

class ImageButton : public QPushButton {
public:
	ImageButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString pixpath, int id, bool checked);
	~ImageButton() final = default;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QPixmap bgPixmap;
};

class BorderImageButton : public QPushButton {
public:
	explicit BorderImageButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString extraStr, int id, bool checked, bool isBgImg = true);
	QString getTabButtonCss(const QString &objectName, int idx, QString url) const;
	~BorderImageButton() final = default;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QLabel *m_boderLabel;
	bool m_isBgImg = true;
};

class ImageAPNGButton : public QPushButton {
public:
	explicit ImageAPNGButton(QButtonGroup *buttonGroup, pls_image_style_type type, QString url, int id, bool checked, const char * /*subName*/, double dpi, QSize scaleSize);
	void setMovieSize(double dpi, QSize _size);
	const QSize &getOriginSize() const { return m_originalSize; };

private:
	QSize m_originalSize;
	QMovie *m_movie{};
};

class CameraVirtualBackgroundStateButton : public QFrame {
	Q_OBJECT
	bool hovered = false;
	bool pressed = false;

public:
	CameraVirtualBackgroundStateButton(const QString &buttonText, QWidget *parent, const std::function<void()> &clicked);
	~CameraVirtualBackgroundStateButton() override = default;

private:
	void setState(const char *name, bool &state, bool value);

signals:
	void clicked();

protected:
	bool event(QEvent *event) override;
};

class AddToListDialog : public PLSDialogView {
	PLSComboBox *bobox = nullptr;

public:
	explicit AddToListDialog(QWidget *parent, OBSSource source);
	QString GetText() const;
};
