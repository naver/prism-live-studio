#include "PLSSideBarDialogView.h"
#include "PLSBasic.h"
#include <libutils-api.h>
#include <QMetaEnum>

PLSSideBarDialogView::PLSSideBarDialogView(DialogInfo info, QWidget *parent) : PLSDialogView(info, parent), defaultInfo(info)
{
#ifdef Q_OS_MACOS
	defaultInfo.defaultHeight -= 40;
#endif // Q_OS_MACOS
}

PLSSideBarDialogView::~PLSSideBarDialogView()
{
	if (!PLSMainView::instance()->isFirstShow()) {

		config_set_string(App()->GlobalConfig(), getConfigId(), "geometry", saveGeometry().toBase64().constData());
		config_save(App()->GlobalConfig());
	}
}

const char *PLSSideBarDialogView::getConfigId() const
{
	return QMetaEnum::fromType<ConfigId>().valueToKey(defaultInfo.configId);
}

void PLSSideBarDialogView::onRestoreGeometry()
{
	auto configId = getConfigId();
	const char *geometry = config_get_string(App()->GlobalConfig(), configId, "geometry");
	if (pls_is_empty(geometry)) {
		auto mainView = PLSBasic::instance()->getMainView();
		if (!mainView)
			return;

		QPoint mainTopRight = mainView->mapToGlobal(QPoint(mainView->frameGeometry().width(), 0));
		auto geometryOfNormal = QRect(mainTopRight.x() + defaultInfo.defaultOffset, mainTopRight.y(), defaultInfo.defaultWidth, defaultInfo.defaultHeight);
		this->setGeometry(geometryOfNormal);
		/* #3503, No reason why Qt calculate dpi wrong. Add some trick code and it works. 
		* Remove this code and test if we upgrade Qt.
		*/
		pls_async_call_mt(this, [this]() {
			auto pos = this->geometry();
			this->move(pos.x() - 1, pos.y());
			this->move(pos.x(), pos.y());
		});
	} else {
		this->restoreGeometry(QByteArray::fromBase64(QByteArray(geometry)));
		if (config_get_bool(App()->GlobalConfig(), configId, "isMaxState")) {
			this->showMaximized();
		}
	}
}

void PLSSideBarDialogView::showEvent(QShowEvent *event)
{
	disableWinSystemBorder();
	config_set_bool(App()->GlobalConfig(), getConfigId(), "showMode", isVisible());
	config_save(App()->GlobalConfig());
	pls_async_call_mt(this, [this]() { pls_window_right_margin_fit(this); });
	PLSDialogView::showEvent(event);
}
void PLSSideBarDialogView::hideEvent(QHideEvent *event)
{
	PLSDialogView::hideEvent(event);

	pls_check_app_exiting();
	config_set_bool(App()->GlobalConfig(), getConfigId(), "showMode", isVisible());
	config_save(App()->GlobalConfig());
}
