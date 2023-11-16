#ifndef PLSSHOPPINGRHYTHMICITY_H
#define PLSSHOPPINGRHYTHMICITY_H

#include "PLSDialogView.h"
#include <browser-panel.hpp>
#include "libbrowser.h"

namespace Ui {
class PLSShoppingRhythmicity;
}

class PLSShoppingRhythmicity : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSShoppingRhythmicity(QWidget *parent = nullptr);
	~PLSShoppingRhythmicity() override;
	void setURL(const QString &url);

private slots:
	void on_declineButton_clicked();
	void on_agreeButton_clicked();

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	void createTwoLineButton(QPushButton *button, const QString &line1, const QString &line2) const;
	QString setLineHeight(QString sourceText, uint lineHeight) const;

	Ui::PLSShoppingRhythmicity *ui;
	pls::browser::BrowserWidget *m_browserWidget;
};

#endif // PLSSHOPPINGRHYTHMICITY_H
