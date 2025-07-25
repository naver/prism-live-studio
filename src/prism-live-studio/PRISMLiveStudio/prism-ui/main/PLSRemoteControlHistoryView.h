#ifndef PLSREMOTECONTROLHISTORYVIEW_H
#define PLSREMOTECONTROLHISTORYVIEW_H

#include <QFrame>
#include <qlabel.h>
#include <qpushbutton.h>
#include <libutils-api.h>

namespace Ui {
class PLSRemoteControlHistoryView;
}

class PLSRemoteControlHistoryView : public QFrame {
	Q_OBJECT

public:
	explicit PLSRemoteControlHistoryView(QWidget *parent);
	virtual ~PLSRemoteControlHistoryView() noexcept(true);

	void setName(const QString &name);

private:
	Ui::PLSRemoteControlHistoryView *ui;
	QString _name;
};

#endif // PLSREMOTECONTROLHISTORYVIEW_H
