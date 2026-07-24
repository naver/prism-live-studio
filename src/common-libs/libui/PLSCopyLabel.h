#ifndef PLSCOPYLABEL_H
#define PLSCOPYLABEL_H

#include "libui-globals.h"
#include "libui.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

class LIBUI_API PLSCopyLabel : public QFrame {
	Q_OBJECT

public:
	explicit PLSCopyLabel(QWidget *parent = nullptr);
	explicit PLSCopyLabel(const QString &text, QWidget *parent = nullptr);
	~PLSCopyLabel() override = default;

	void setText(const QString &text);
	QString getText() const;

	void setSpacing(int spacing);
	int getSpacing() const;

	void setToolTip(const QString &tip);

protected:
	void setupUI();

private slots:
	void onCopyButtonClicked();

private:
	QLabel *m_label = nullptr;
	QPushButton *m_copyButton = nullptr;
	QHBoxLayout *m_layout = nullptr;
	int m_spacing = 2;
	QString m_text;
};

#endif // PLSCOPYLABEL_H
