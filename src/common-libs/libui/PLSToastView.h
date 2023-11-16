#ifndef PLSTOASTVIEW_H
#define PLSTOASTVIEW_H

#include "PLSDialogView.h"

#include <QTimer>

namespace Ui {
class PLSToastView;
}

/**
 * @class PLSToastView
 * @brief common popup toast
 */
class LIBUI_API PLSToastView : public PLSDialogView {
	Q_OBJECT
	Q_PROPERTY(Icon icon READ getIcon WRITE setIcon)
	Q_PROPERTY(QPixmap pixmap READ getPixmap WRITE setPixmap)

public:
	enum Icon { Error = 1, Warning = 2 };
	Q_ENUM(Icon)

	explicit PLSToastView(QWidget *parent = nullptr);
	~PLSToastView() override;

	Icon getIcon() const;
	void setIcon(Icon icon);

	QPixmap getPixmap() const;
	void setPixmap(const QPixmap &pixmap);

	void setMessage(const QString &message);

	void show(Icon icon, const QString &message, int autoHide = 0);

private slots:
	void autoHide();

private:
	Ui::PLSToastView *ui = nullptr;
	Icon icon = Warning;
	QTimer *timer = nullptr;
};

#endif // PLSTOASTVIEW_H
