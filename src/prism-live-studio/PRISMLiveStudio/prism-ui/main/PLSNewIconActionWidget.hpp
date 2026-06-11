#ifndef CUSTOMHELPMENUITEM_H
#define CUSTOMHELPMENUITEM_H

#include <QWidget>

namespace Ui {
class PLSNewIconActionWidget;
}

class PLSNewIconActionWidget : public QWidget {
	Q_OBJECT

	Q_PROPERTY(int textMarginLeft READ getTextMarginLeft WRITE setTextMarginLeft)

public:
	explicit PLSNewIconActionWidget(const QString &title, QWidget *parent = nullptr);
	~PLSNewIconActionWidget() override;
	void setText(const QString &text);
	void setBadgeVisible(bool visible = false);
	void setItemDisabled(bool disabled);
	int getTextMarginLeft() const;
	void setTextMarginLeft(int textMarginLeft);

	// QWidget interface
protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	Ui::PLSNewIconActionWidget *ui;
	bool m_disabled{false};
	int textMarginLeft = 8;
};

#endif // CUSTOMHELPMENUITEM_H
