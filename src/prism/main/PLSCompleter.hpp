#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QScrollArea>

class PLSCompleter : public QFrame {
	Q_OBJECT

public:
	explicit PLSCompleter(QLineEdit *lineEdit, const QStringList &completions);
	~PLSCompleter() override;

signals:
	void activated(const QString &text);

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QLineEdit *lineEdit;
	QScrollArea *scrollArea;
	QList<QLabel *> labels;
};
