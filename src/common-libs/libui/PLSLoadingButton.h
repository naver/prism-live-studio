//
//  PLSLoadingButton.h
//  PRISMLiveStudio
//
//  Created by Sam Zhang on 2025/10/31.
//

#ifndef PLSLoadingButton_h
#define PLSLoadingButton_h

#include <QPushButton>
#include "libui-globals.h"

class QLabel;
class QWidget;
class PLSLoadingView;

class LIBUI_API PLSLoadingButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSLoadingButton(const QString &text = QString(), QWidget *parent = nullptr);
	~PLSLoadingButton();

	void setLensSelected(bool selected);
	void setLoading(bool loading);

	bool isLensSelected() const;
	bool isLoading() const;

	void setNormalText(const QString &text);
	void setLoadingText(const QString &text);
	void setSelectText(const QString &text);

protected:
	void resizeEvent(QResizeEvent *event) override;
	virtual void updateText();

protected:
	bool m_loading = false;
	bool m_lensSelected = true;
	QString m_normalText;
	QString m_loadingText;
	QString m_selectLensText;

	QWidget *m_container;
	QLabel *m_textLabel;
	PLSLoadingView *m_loadingView;
};

class LIBUI_API PLSLoadingVbButton : public PLSLoadingButton {
	Q_OBJECT

public:
	explicit PLSLoadingVbButton(const QString &text, QWidget *parent = nullptr);
	~PLSLoadingVbButton();

	void setButtonText(const QString &text);

protected:
	void updateText() override;
};

#endif /* PLSLoadingButton_h */
