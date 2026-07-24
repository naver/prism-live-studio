#ifndef CUSTOMHELPMENUITEM_H
#define CUSTOMHELPMENUITEM_H

#include <QWidget>
#include <QLabel>
#include <QPointer>

namespace Ui {
class PLSNewIconActionWidget;
}

class PLSNewIconActionWidget : public QWidget {
	Q_OBJECT

	Q_PROPERTY(int textMarginLeft READ getTextMarginLeft WRITE setTextMarginLeft)

public:
	/** @param itemIconQrc Optional `:/...` path; when set, shows a 22×22 icon with help-menu spacing (left 20px, 10px before text). */
	explicit PLSNewIconActionWidget(const QString &title, QWidget *parent = nullptr, const QString &itemIconQrc = QString());
	~PLSNewIconActionWidget() override;
	void setText(const QString &text);
	void setBadgeVisible(bool visible = false);
	void setItemDisabled(bool disabled);
	int getTextMarginLeft() const;
	void setTextMarginLeft(int textMarginLeft);
	void setNoticeTipsVisible(bool visible = false);

	/** Minimum width for sidebar help menu from margins, icons, spacers, and full title text (long i18n). */
	int helpMenuRowMinimumWidth() const;

	// QWidget interface
protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void setupHelpMenuIconRow(const QString &itemIconQrc);

	Ui::PLSNewIconActionWidget *ui;
	bool m_disabled{false};
	int textMarginLeft = 8;
	QPointer<QLabel> m_noticeTipsIcon;
	QLabel *m_itemIcon{nullptr};
};

#endif // CUSTOMHELPMENUITEM_H
