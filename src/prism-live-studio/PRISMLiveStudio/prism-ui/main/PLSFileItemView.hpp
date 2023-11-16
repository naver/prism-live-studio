#ifndef PLSFILEITEMVIEW_H
#define PLSFILEITEMVIEW_H

#include <QFrame>

namespace Ui {
class PLSFileItemView;
}

class PLSFileItemView : public QFrame {
	Q_OBJECT

public:
	explicit PLSFileItemView(int index, QWidget *parent = nullptr);
	~PLSFileItemView() override;
	QFont fileNameLabelFont() const;
	void setFileName(const QString &filename);

	// QWidget interface
protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private slots:
	void on_deleteIcon_clicked();

signals:
	void deleteItem(int index);

private:
	Ui::PLSFileItemView *ui;
	int m_index;
};

#endif // PLSFILEITEMVIEW_H
