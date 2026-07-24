#ifndef PLSLOGO_H
#define PLSLOGO_H

#include <QPushButton>

namespace Ui {
class PLSLogo;
}

class PLSLogo : public QPushButton {
	Q_OBJECT

public:
	explicit PLSLogo(QWidget *parent = nullptr);
	~PLSLogo() override;
	void setVisableTips(bool isVisable);
	void refreshMinimumWidthFromLayout();

protected:
	bool event(QEvent *event) override;

	Ui::PLSLogo *ui = nullptr;
	bool m_isVisableTips = false;
	QString m_tipText;
};

#endif // PLSLOGO_H
