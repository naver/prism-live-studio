#pragma once

#include "libui-globals.h"

#include <QMenu>
#include <QFrame>
#include <QIcon>
#include <QPointer>

class QMenu;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;
class QWidgetAction;

class PLSPopupMenu;

class PLSMenu : public QMenu {
	Q_OBJECT
public:
	using QMenu::QMenu;

signals:
	void actionAdded(QAction *action, QAction *before);
	void actionRemoved(QAction *action);

protected:
	bool event(QEvent *event) override;
};

#if 0

class LIBUI_API PLSPopupMenuItem : public QFrame {
	Q_OBJECT
	Q_PROPERTY(int marginLeft READ marginLeft WRITE setMarginLeft)

public:
	explicit PLSPopupMenuItem(QAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItem() override = default;

	enum class Type { Separator, Action, WidgetAction, Menu };

	QAction *&action() { return m_action; }
	PLSPopupMenu *&menu() { return m_menu; }

	int marginLeft() const { return m_marginLeft; }
	virtual void setMarginLeft(int marginLeft);

	virtual Type type() const = 0;

protected:
	virtual void actionHovered();
	virtual void actionTriggered();
	virtual void actionChanged();

public:
	QString getText(int &shortcutKey, const QFont &font, int width);

private:
	QAction *m_action = nullptr;
	PLSPopupMenu *m_menu = nullptr;
	int m_marginLeft = 0;

//	friend class PLSPopupMenu;
};

class LIBUI_API PLSPopupMenuItemSeparator : public PLSPopupMenuItem {
	Q_OBJECT

public:
	explicit PLSPopupMenuItemSeparator(QAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItemSeparator() override = default;

	Type type() const override;
};

class LIBUI_API PLSPopupMenuItemContent : public PLSPopupMenuItem {
	Q_OBJECT
	Q_PROPERTY(bool hasIcon READ getHasIcon WRITE setHasIcon)
	Q_PROPERTY(bool isLeftIcon READ getIsLeftIcon WRITE setIsLeftIcon)
	Q_PROPERTY(bool toolTipsVisible READ toolTipsVisible WRITE setToolTipsVisible)

public:
	explicit PLSPopupMenuItemContent(QAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItemContent() override = default;

	void showShortcutKey();
	void shortcutKeyPress(int key);

	bool getHasIcon() const;
	void setHasIcon(bool hasIcon);

	bool getIsLeftIcon() const;
	void setIsLeftIcon(bool isLeftIcon);

	bool toolTipsVisible() const;
	void setToolTipsVisible(bool toolTipsVisible);

protected:
	void actionChanged() override;
	void setMarginLeft(int marginLeft) override;

	bool eventFilter(QObject *watched, QEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	virtual int otherWidth() const;

private:
	bool m_hasIcon = false;
	bool m_isLeftIcon = true;
	bool m_toolTipsVisible = false;
	QHBoxLayout *m_layout = nullptr;
	QLabel *m_icon = nullptr;
	QLabel *m_text = nullptr;
	QWidget *m_spacer = nullptr;
	int m_shortcutKey = 0;
};

class LIBUI_API PLSPopupMenuItemAction : public PLSPopupMenuItemContent {
	Q_OBJECT

public:
	explicit PLSPopupMenuItemAction(QAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItemAction() override = default;

	Type type() const override;

protected:
	void actionHovered() override;
	void actionTriggered() override;
	void actionChanged() override;
	int otherWidth() const override;

	void leaveEvent(QEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QLabel *m_shortcut = nullptr;
};

class LIBUI_API PLSPopupMenuItemWidgetAction : public PLSPopupMenuItem {
	Q_OBJECT
public:
	explicit PLSPopupMenuItemWidgetAction(QWidgetAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItemWidgetAction() override = default;

	Type type() const override;

protected:
	void actionHovered() override;
	void actionTriggered() override;
	void actionChanged() override;
};

class LIBUI_API PLSPopupMenuItemMenu : public PLSPopupMenuItemContent {
	Q_OBJECT
public:
	explicit PLSPopupMenuItemMenu(QAction *action, PLSPopupMenu *menu);
	~PLSPopupMenuItemMenu() override = default;

	Type type() const override;

protected:
	void actionHovered() override;
	void actionTriggered() override;
	void actionChanged() override;
	int otherWidth() const override;

	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QLabel *m_arrow = nullptr;
};

class LIBUI_API PLSPopupMenu : public QFrame {
	Q_OBJECT
	Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
	Q_PROPERTY(int marginLeft READ getMarginLeft WRITE setMarginLeft)
	Q_PROPERTY(int marginTop READ getMarginTop WRITE setMarginTop)
	Q_PROPERTY(int marginRight READ getMarginRight WRITE setMarginRight)
	Q_PROPERTY(int marginBottom READ getMarginBottom WRITE setMarginBottom)

public:
	explicit PLSPopupMenu(QWidget *parent = nullptr);
	explicit PLSPopupMenu(bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(const QString &title, bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(const QIcon &icon, const QString &title, bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(QAction *menuAction, QWidget *parent = nullptr);
	explicit PLSPopupMenu(QAction *menuAction, bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(QMenu *menu, QWidget *parent = nullptr);
	explicit PLSPopupMenu(QMenu *menu, bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(const PLSMenu *menu, QWidget *parent = nullptr);
	explicit PLSPopupMenu(const PLSMenu *menu, bool toolTipsVisible, QWidget *parent = nullptr);
	explicit PLSPopupMenu(const QIcon &icon, const QString &title, QAction *menuAction, bool toolTipsVisible, QWidget *parent = nullptr);
	~PLSPopupMenu() override;

	PLSPopupMenu *owner() const;
	void setOwner(PLSPopupMenu *owner);

	QPointer<QAction> menuAction() const;

	QString title() const;
	void setTitle(const QString &title) const;

	QIcon icon() const;
	void setIcon(const QIcon &icon) const;

	using QWidget::addAction;
	QAction *addAction(const QString &text);
	QAction *addAction(const QIcon &icon, const QString &text);
	QAction *addAction(const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut = 0);
	QAction *addAction(const QIcon &icon, const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut = 0);

	// addAction(QString): Connect to a QObject slot / functor or function pointer (with context)
	template<class Obj, typename Func1>
	inline typename std::enable_if_t<!std::is_same_v<const char *, Func1> && QtPrivate::IsPointerToTypeDerivedFromQObject<Obj *>::Value, QAction *>
	addAction(const QString &text, const Obj *object, Func1 slot, const QKeySequence &shortcut = 0)
	{
		QAction *result = addAction(text);
		result->setShortcut(shortcut);
		connect(result, &QAction::triggered, object, std::move(slot));
		return result;
	}
	// addAction(QString): Connect to a functor or function pointer (without context)
	template<typename Func1> inline QAction *addAction(const QString &text, Func1 slot, const QKeySequence &shortcut = 0)
	{
		QAction *result = addAction(text);
		result->setShortcut(shortcut);
		connect(result, &QAction::triggered, std::move(slot));
		return result;
	}
	// addAction(QIcon, QString): Connect to a QObject slot / functor or function pointer (with context)
	template<class Obj, typename Func1>
	inline typename std::enable_if_t<!std::is_same_v<const char *, Func1> && QtPrivate::IsPointerToTypeDerivedFromQObject<Obj *>::Value, QAction *>
	addAction(const QIcon &actionIcon, const QString &text, const Obj *object, Func1 slot, const QKeySequence &shortcut = 0)
	{
		QAction *result = addAction(actionIcon, text);
		result->setShortcut(shortcut);
		connect(result, &QAction::triggered, object, std::move(slot));
		return result;
	}
	// addAction(QIcon, QString): Connect to a functor or function pointer (without context)
	template<typename Func1> inline QAction *addAction(const QIcon &actionIcon, const QString &text, Func1 slot, const QKeySequence &shortcut = 0)
	{
		QAction *result = addAction(actionIcon, text);
		result->setShortcut(shortcut);
		connect(result, &QAction::triggered, std::move(slot));
		return result;
	}

	QAction *addMenu(const PLSPopupMenu *menu);
	PLSPopupMenu *addMenu(const QString &title);
	PLSPopupMenu *addMenu(const QIcon &icon, const QString &title);

	QAction *addSeparator();

	QAction *insertMenu(QAction *before, const PLSPopupMenu *menu);
	QAction *insertSeparator(QAction *before);

	int getMarginLeft() const;
	void setMarginLeft(int left);

	int getMarginTop() const;
	void setMarginTop(int top);

	int getMarginRight() const;
	void setMarginRight(int right);

	int getMarginBottom() const;
	void setMarginBottom(int bottom);

	void asButtonPopupMenu(QPushButton *button, const QPoint &offset = QPoint());

	void popup(const QPoint &pos, const QAction *action = nullptr);
	void popup(const QPushButton *button, const QPoint &offset = QPoint(), const QAction *action = nullptr);
	void exec(const QPoint &pos, const QAction *action = nullptr);
	void exec(const QPushButton *button, const QPoint &offset = QPoint(), const QAction *action = nullptr);

	QAction *selectedAction() const;
	void enterSubmenu();
	void leaveSubmenu();
	void triggeredSelectedOrFirstAction();
	void hoveredPrevOrLastAction();
	void hoveredNextOrFirstAction();
	void clearSelectedAction();
	static void setSelectedAction(QAction *selectedAction);
	static void setHasIcon(const QAction *action, bool hasIcon);
	static void setToolTipsVisible(const QAction *action, bool toolTipsVisible);
	static void setToolTips(QAction *action, const QString &tooltips = QString());

	bool isEmpty() const;

	bool toolTipsVisible() const;
	void setToolTipsVisible(bool visible);

private:
	void bindPLSMenu(const PLSMenu *menu);

signals:
	void triggered(QAction *action);
	void hovered(QAction *action);
	void shown();
	void hidden();
	void hideAllPopupMenu();

private slots:
	void onHideAllPopupMenu() const;

protected:
	bool event(QEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	void event_ActionRemoved(QEvent *event);

	PLSPopupMenu *m_owner;
	QPointer<QAction> m_menuAction;
	QAction *m_selectedAction;
	QVBoxLayout *m_layout;
	bool m_toolTipsVisible;

	friend class PLSPopupMenuItem;
	friend class PLSPopupMenuItemContent;
	friend class PLSPopupMenuItemAction;
};

#endif // Q_OS_WIN
