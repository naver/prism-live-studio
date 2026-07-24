#ifndef PLSTOOLVIEW_HPP
#define PLSTOOLVIEW_HPP

#include <PLSDialogView.h>

enum class PLSCloseWay { Hide, Close };

template<typename Tool, PLSCloseWay CloseWay = PLSCloseWay::Hide> class PLSToolView : public PLSDialogView {
public:
	explicit PLSToolView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : PLSDialogView(parent, f) {}
	~PLSToolView() override = default;

public:
	static QPointer<Tool> instance()
	{
		static QPointer<Tool> s_instance = pls_new<Tool>(pls_get_main_view());
		return s_instance;
	}

protected:
	bool closeButtonHook() override
	{
		if (CloseWay == PLSCloseWay::Close)
			return PLSDialogView::closeButtonHook();
		hide();
		return false;
	}
	void showEvent(QShowEvent *event) override
	{
		PLSDialogView::showEvent(event);
		disableWinSystemBorder();
	}
};

#endif // PLSTOOLVIEW_HPP
