#ifndef PLSLOADINGMENUITEM_H
#define PLSLOADINGMENUITEM_H

#include <QWidget>

namespace Ui {
class PLSLoadingMenuItem;
}

class PLSLoadingMenuItem : public QWidget {
	Q_OBJECT

public:
	explicit PLSLoadingMenuItem(QWidget *parent = nullptr);
	~PLSLoadingMenuItem() override;
	void setTitle(const QString &title);
	void setSelected(bool selected);
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSLoadingMenuItem *ui;
	QString m_title;
};

#endif // PLSLOADINGMENUITEM_H
