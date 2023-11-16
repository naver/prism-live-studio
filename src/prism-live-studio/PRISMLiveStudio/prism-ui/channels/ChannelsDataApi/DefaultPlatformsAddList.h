#ifndef DEFAULTPLATFORMSADDLIST_H
#define DEFAULTPLATFORMSADDLIST_H

#include <QFrame>

namespace Ui {
class DefaultPlatformsAddList;
}

class DefaultPlatformsAddList : public QFrame {
	Q_OBJECT

public:
	explicit DefaultPlatformsAddList(QWidget *parent = nullptr);
	~DefaultPlatformsAddList() override;

public slots:
	void runBtnCMD() const;

protected:
	void changeEvent(QEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void initUi();

	//private:
	Ui::DefaultPlatformsAddList *ui;
};

#endif // DEFAULTPLATFORMSADDLIST_H
