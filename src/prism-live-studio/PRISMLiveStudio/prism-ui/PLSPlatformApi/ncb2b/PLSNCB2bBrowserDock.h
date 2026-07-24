#ifndef PLSNCB2BBROWSERDOCK_H
#define PLSNCB2BBROWSERDOCK_H

#include <QObject>
#include "window-dock.hpp"
#include "PLSErrorHandler.h"

class PLSNCB2bBrowserDockContent;
class PLSNCB2bBrowserDock : public OBSDock {
	Q_OBJECT

public:
	PLSNCB2bBrowserDock(const QString &title, const QString &selectedTitle, QWidget *parent = nullptr);
	~PLSNCB2bBrowserDock();
	void refreshUI(PLSErrorHandler::ErrCode errCode);
	void closeBrowser();

protected:
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	PLSNCB2bBrowserDockContent *content{nullptr};
};

#endif // PLSNCB2BBROWSERDOCK_H