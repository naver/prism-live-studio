#ifndef PLSLABORATORYITEM_H
#define PLSLABORATORYITEM_H

#include "PLSDialogView.h"
#include <qpushbutton.h>
#include <QtWidgets/qscrollarea.h>

namespace Ui {
class PLSLaboratoryItem;
}

class PLSLaboratoryItem : public QPushButton {
	Q_OBJECT

public:
	explicit PLSLaboratoryItem(const QString &id, const QString &name, QWidget *parent = nullptr);
	~PLSLaboratoryItem() override;
	QString itemId() const;
	QString itemName() const;
	void setScrollArea(QScrollArea *scrollArea);
	QScrollArea *getScrollArea();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void statusChanged(bool isChecked);

private:
	QString m_id;
	QString m_text;
	Ui::PLSLaboratoryItem *ui;
	QScrollArea *m_scrollArea = nullptr;
};

#endif // PLSLABORATORYITEM_H
