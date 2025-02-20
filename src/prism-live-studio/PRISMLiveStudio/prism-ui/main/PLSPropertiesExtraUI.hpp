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
#include "frontend-api.h"

class QMovie;
class QButtonGroup;
class QString;
class PLSDialogView;
class PLSComboBox;

class ChatTemplate : public QPushButton {

	Q_OBJECT
public:
	ChatTemplate(QButtonGroup *buttonGroup, int id, bool checked);
	ChatTemplate(QButtonGroup *buttonGroup, int id, const QString text, const QString iconPath, bool isEditUi = false, const QString &backgroundColor = QString());
	~ChatTemplate() override = default;

protected:
	bool event(QEvent *event) override;
	void createFrame(bool isEdit);
	bool isChatTemplateNameExist(const QString &editName);
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watch, QEvent *event) override;

signals:
	void resetSourceProperties(int removeTemplateId);

private:
	void setEditBtnVisible(bool isVisible);

private:
	QPointer<QFrame> m_frame;
	QLabel *m_textLabel = nullptr;
	std::string m_editName;
	int id = 0;
	QPixmap m_borderPixMap;
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

class FontSelectionWindow : public QFrame {
	Q_OBJECT

public:
	explicit FontSelectionWindow(const QList<ITextMotionTemplateHelper::PLSChatDefaultFamily> &families, const QString &selectFamily, QWidget *parent = nullptr);
	~FontSelectionWindow();

signals:
	void clickFontBtn(QAbstractButton *button);
};

class FontButton : public QPushButton {
public:
	explicit FontButton(const QString &resourceName, int width, int height = 35);
	~FontButton() = default;

protected:
	bool event(QEvent *event);
	void checkStateSet() override;

private:
	void updateFontButtonBgm(const QString &bgmRes);

private:
	QLabel *m_picLabel = nullptr;
	QString m_normalResPath;
	QString m_hoveredResPath;
	QString m_selectedResPath;
};