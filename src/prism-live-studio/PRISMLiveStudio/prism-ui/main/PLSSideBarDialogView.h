#ifndef PLSSIDEBARDIALOGVIEW_H
#define PLSSIDEBARDIALOGVIEW_H

#include "PLSDialogView.h"
#include "frontend-api.h"

class PLSSideBarDialogView : public PLSDialogView {
public:
	explicit PLSSideBarDialogView(DialogInfo info, QWidget *parent = nullptr, CreateWinId createWinId = CreateWinId::DontCreate);
	~PLSSideBarDialogView();

	const char *getConfigId() const;

protected:
	void onRestoreGeometry() override;

	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	DialogInfo defaultInfo;
	bool m_firstShow = true;
};
#endif // PLSSIDEBARDIALOGVIEW_H
