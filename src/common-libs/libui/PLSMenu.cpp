#include "PLSMenu.h"

#include <QAction>
#include <QMenu>
#include <QPushButton>
#include <QActionEvent>
#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QApplication>
#include <QKeyEvent>
#include <QList>
#include <QIcon>
#include <QEventLoop>
#include <QPushButton>
#include <QWidgetAction>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#include <liblog.h>
#include <memory>


#include <libutils-api.h>

#include "libui.h"

#if 0
namespace {
const unsigned int altBit = 0x20000000;
const unsigned int keydownBit = 0x40000000;

struct LocalGlobalVars {
	static QList<PLSPopupMenu *> menuList;
	static PLSPopupMenu *activeMenu;
	static QPushButton *menuButton;
};

QList<PLSPopupMenu *> LocalGlobalVars::menuList;
PLSPopupMenu *LocalGlobalVars::activeMenu = nullptr;
QPushButton *LocalGlobalVars::menuButton = nullptr;

class ActionUserdata : public QObject {
	Q_OBJECT

public:
	ActionUserdata(bool isMenuAction, PLSPopupMenu *menu, PLSPopupMenu *submenu, PLSPopupMenuItem *menuItem) : m_isMenuAction(isMenuAction), m_menu(menu), m_submenu(submenu), m_menuItem(menuItem)
	{
	}
	~ActionUserdata() override = default;

	static ActionUserdata *getActionUserData(const QAction *action)
	{
		if (action) {
			return (ActionUserdata *)(action->property("g_ActionUserdata").value<void *>());
		}
		return nullptr;
	}

	void showShortcutKey() const
	{
		if (m_menuItem && m_menuItem->type() != PLSPopupMenuItem::Type::Separator) {
			dynamic_cast<PLSPopupMenuItemContent *>(m_menuItem.data())->showShortcutKey();
		}
	}
	void shortcutKeyPress(int key) const
	{
		if (m_menuItem && m_menuItem->type() != PLSPopupMenuItem::Type::Separator) {
			dynamic_cast<PLSPopupMenuItemContent *>(m_menuItem.data())->shortcutKeyPress(key);
		}
	}

	bool m_isMenuAction;
	QPointer<PLSPopupMenu> m_menu;
	QPointer<PLSPopupMenu> m_submenu;
	QPointer<PLSPopupMenuItem> m_menuItem;
};

class HookNativeEvent {
	Q_DISABLE_COPY(HookNativeEvent)

public:
	HookNativeEvent()
	{
		m_keyboardHook.reset(SetWindowsHookExW(WH_KEYBOARD, &keyboardHookProc, GetModuleHandleW(nullptr), GetCurrentThreadId()));
		m_mouseHook.reset(SetWindowsHookExW(WH_MOUSE, &mouseHookProc, GetModuleHandleW(nullptr), GetCurrentThreadId()));
	}

protected:
	static LRESULT CALLBACK keyboardHookProc(_In_ int code, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
		if ((code < 0) || LocalGlobalVars::menuList.isEmpty() || !(lParam & keydownBit)) {
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		switch (wParam) {
		case VK_MENU:
			processVkMenu();
			break;
		case VK_LEFT:
			if (auto menu = activeMenu(); menu)
				menu->leaveSubmenu();
			break;
		case VK_RIGHT:
			if (auto menu = activeMenu(); menu)
				menu->enterSubmenu();
			break;
		case VK_UP:
			if (auto menu = activeMenu(); menu)
				menu->hoveredPrevOrLastAction();
			break;
		case VK_DOWN:
			if (auto menu = activeMenu(); menu)
				menu->hoveredNextOrFirstAction();
			break;
		case VK_RETURN:
			if (auto menu = activeMenu(); menu)
				menu->triggeredSelectedOrFirstAction();
			break;
		case VK_ESCAPE:
			hideAllPopupMenu();
			break;
		default:
			processNumLet(wParam);
			break;
		}

		return CallNextHookEx(nullptr, code, wParam, lParam);
	}
	static LRESULT CALLBACK mouseHookProc(_In_ int code, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
#if 0
		if (wParam == WM_LBUTTONDOWN) {
			QWidget *widget = QApplication::widgetAt(QCursor::pos());
			if (widget) {
				for (QWidget* p = widget; p; p = p->parentWidget()) {
					qDebug() << p->metaObject()->className() << p->objectName();
				}
			}
		}
#endif

		if ((code < 0) || LocalGlobalVars::menuList.isEmpty()) {
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		if ((wParam == WM_LBUTTONDOWN) || (wParam == WM_LBUTTONUP) || (wParam == WM_LBUTTONDBLCLK) || (wParam == WM_RBUTTONDOWN) || (wParam == WM_RBUTTONUP) || (wParam == WM_RBUTTONDBLCLK) ||
		    (wParam == WM_MBUTTONDOWN) || (wParam == WM_MBUTTONUP) || (wParam == WM_MBUTTONDBLCLK) || (wParam == WM_NCLBUTTONDOWN) || (wParam == WM_NCLBUTTONUP) ||
		    (wParam == WM_NCLBUTTONDBLCLK) || (wParam == WM_NCRBUTTONDOWN) || (wParam == WM_NCRBUTTONUP) || (wParam == WM_NCRBUTTONDBLCLK) || (wParam == WM_NCMBUTTONDOWN) ||
		    (wParam == WM_NCMBUTTONUP) || (wParam == WM_NCMBUTTONDBLCLK)) {
			auto mhs = (LPMOUSEHOOKSTRUCT)lParam;
			if (!isInButton(mhs->pt) && !findMenu(mhs->pt)) {
				hideAllPopupMenu();
			}
		}

		return CallNextHookEx(nullptr, code, wParam, lParam);
	}

	static PLSPopupMenu *activeMenu()
	{
		if (LocalGlobalVars::activeMenu) {
			return LocalGlobalVars::activeMenu;
		} else if (!LocalGlobalVars::menuList.isEmpty()) {
			LocalGlobalVars::activeMenu = LocalGlobalVars::menuList.last();
			return LocalGlobalVars::activeMenu;
		}
		return nullptr;
	}
	static PLSPopupMenu *findMenu(const POINT &pt)
	{
		for (PLSPopupMenu *menu : LocalGlobalVars::menuList) {
			RECT rc;
			GetWindowRect((HWND)menu->winId(), &rc);
			if (PtInRect(&rc, pt)) {
				return menu;
			}
		}
		return nullptr;
	}
	static void hideAllPopupMenu()
	{
		for (PLSPopupMenu *menu : LocalGlobalVars::menuList) {
			menu->hide();
		}
	}
	static bool isInButton(const POINT &pt)
	{
		if (!LocalGlobalVars::menuButton) {
			return false;
		}

		QRect rc(LocalGlobalVars::menuButton->mapToGlobal(QPoint(0, 0)), LocalGlobalVars::menuButton->size());
		return rc.contains(pt.x, pt.y);
	}
	static void processVkMenu()
	{
		for (auto menu : LocalGlobalVars::menuList) {
			for (auto action : menu->actions()) {
				if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
					userData->showShortcutKey();
				}
			}
		}
	}
	static void processNumLet(WPARAM wParam)
	{
		if ((wParam >= 0x30) && (wParam <= 0x39) || (wParam >= 0x41) && (wParam <= 0x5A)) {
			for (auto action : activeMenu()->actions()) {
				if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
					userData->shortcutKeyPress(int(wParam));
				}
			}
		}
	}

private:
	struct HHOOKDeleter {
		void operator()(HHOOK &hhook) const { pls_delete(hhook, UnhookWindowsHookEx, nullptr); }
	};
	PLSAutoHandle<HHOOK, HHOOKDeleter> m_keyboardHook;
	PLSAutoHandle<HHOOK, HHOOKDeleter> m_mouseHook;
};

void installNativeEventFilter()
{
	static std::unique_ptr<HookNativeEvent> hookNativeEvent;
	if (!hookNativeEvent) {
		hookNativeEvent.reset(pls_new<HookNativeEvent>());
	}
}

void flushStyle(const QAction *action)
{
	if (action) {
		if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
			pls_flush_style(userData->m_menuItem);
		}
	}
}

void setActionUserData(QAction *action, bool isMenuAction, PLSPopupMenu *menu, PLSPopupMenu *submenu, PLSPopupMenuItem *menuItem)
{
	auto userData = (ActionUserdata *)(action->property("g_ActionUserdata").value<void *>());
	if (!userData) {
		userData = pls_new<ActionUserdata>(isMenuAction, menu, submenu, menuItem);
		action->setProperty("g_ActionUserdata", QVariant::fromValue((void *)userData));
		QObject::connect(action, &QObject::destroyed, userData, &ActionUserdata::deleteLater);
	} else {
		if (menu)
			userData->m_menu = menu;
		if (submenu)
			userData->m_submenu = submenu;
		if (menuItem)
			userData->m_menuItem = menuItem;
	}
}

bool isMenuAction(const QAction *action)
{
	if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_isMenuAction) {
		return true;
	}
	return false;
}

QRect actionBoundingRect(const QAction *action)
{
	if (auto userData = ActionUserdata::getActionUserData(action); userData) {
		return QRect(QPoint(userData->m_menu->mapToGlobal(QPoint(0, 0)).x(), userData->m_menuItem->mapToGlobal(QPoint(0, 0)).y()),
			     QSize(userData->m_menu->width(), userData->m_menuItem->height()));
	}
	return QRect(0, 0, 0, 0);
}

int actionIndex(const QList<QAction *> &actions, QAction *action)
{
	return action ? actions.indexOf(action) : -1;
}

void popupSubmenu(const PLSPopupMenu *menu, const QAction *selected)
{
	for (const QAction *action : menu->actions()) {
		auto userData = ActionUserdata::getActionUserData(action);
		if (!userData || !userData->m_isMenuAction) {
			continue;
		}

		if (action == selected) {
			userData->m_submenu->popup(actionBoundingRect(selected).topRight() + QPoint(1, 0), action);
		} else {
			userData->m_submenu->hide();
		}
	}
}

void hideSubmenu(const PLSPopupMenu *menu)
{
	if (menu) {
		for (auto action : menu->actions()) {
			if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_isMenuAction && userData->m_submenu) {
				hideSubmenu(userData->m_submenu);
				userData->m_submenu->clearSelectedAction();
				userData->m_submenu->hide();
			}
		}
	}
}

void hideMenuByUserData(ActionUserdata *userData)
{
	if (userData && userData->m_menu) {
		userData->m_menu->hide();
		if (auto menuAction = userData->m_menu->menuAction(); menuAction) {
			userData = ActionUserdata::getActionUserData(menuAction);
			hideMenuByUserData(userData);
		}
	}
}

PLSPopupMenu *newSubMenu(PLSPopupMenu *menu, QAction *action)
{
	auto submenu = pls_new<PLSPopupMenu>(action, menu);
	submenu->setOwner(menu);
	pls_flush_style(submenu);
	return submenu;
}

PLSPopupMenuItem *newMenuItem(PLSPopupMenu *menu, QAction *action)
{
	if (!action) {
		return nullptr;
	} else if (auto widgetAction = dynamic_cast<QWidgetAction *>(action)) {
		return pls_new<PLSPopupMenuItemWidgetAction>(widgetAction, menu);
	} else if (action && action->isSeparator()) {
		return pls_new<PLSPopupMenuItemSeparator>(action, menu);
	} else if (action && action->menu()) {
		return pls_new<PLSPopupMenuItemMenu>(newSubMenu(menu, action)->menuAction(), menu);
	} else if (!isMenuAction(action)) {
		return pls_new<PLSPopupMenuItemAction>(action, menu);
	} else {
		return pls_new<PLSPopupMenuItemMenu>(action, menu);
	}
}

bool isPLSPopupMenuType(QObject *object)
{
	if (!object || !object->isWidgetType()) {
		return false;
	}

	auto widget = dynamic_cast<QWidget *>(object);
	if (dynamic_cast<PLSPopupMenu *>(widget) || dynamic_cast<PLSPopupMenuItem *>(widget)) {
		return true;
	} else if (isPLSPopupMenuType(widget->parentWidget())) {
		return true;
	}
	return false;
}

bool isAltPressed()
{
	return GetKeyState(VK_MENU) < 0;
}

QString decodeMenuItemText(const QString &text, int &shortcut)
{
	shortcut = 0;

	if (text.length() < 2) {
		return text;
	}

	if (text.startsWith('&')) {
		shortcut = text.at(1).toUpper().unicode();
		if (isAltPressed()) {
			return QString("<u>%1</u>%2").arg(text.at(1)).arg(text.mid(2));
		}
		return text.mid(1);
	}

	QStringList parts = text.split('&', Qt::SkipEmptyParts);
	if (parts.count() == 2) {
		shortcut = parts.at(1).at(0).toUpper().unicode();
		if (isAltPressed()) {
			return QString("%1<u>%2</u>%3").arg(parts.at(0)).arg(parts.at(1).at(0)).arg(parts.at(1).mid(1));
		}
		return parts.at(0) + parts.at(1);
	}
	return text;
}

void setMenuItemProperty(const QAction *action, const char *name, bool value)
{
	if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
		pls_flush_style_recursive(userData->m_menuItem, name, value);
	}
}

QAction *prevAction(const QList<QAction *> &actions, QAction *current)
{
	if (int index = actions.indexOf(current); (index > 0) && (index < actions.count())) {
		return actions.at(index - 1);
	}
	return actions.last();
}

QAction *prevValidAction(const QList<QAction *> &actions, QAction *current)
{
	QAction *prev = prevAction(actions, current);
	while (prev && (prev != current) && (prev->isSeparator() || !prev->isVisible())) {
		prev = prevAction(actions, prev);
	}

	if (prev && !prev->isSeparator() && prev->isVisible()) {
		return prev;
	}
	return nullptr;
}

QAction *nextAction(const QList<QAction *> &actions, QAction *current)
{
	if (int index = actions.indexOf(current); (index >= 0) && (index < (actions.count() - 1))) {
		return actions.at(index + 1);
	}
	return actions.first();
}

QAction *nextValidAction(const QList<QAction *> &actions, QAction *current)
{
	QAction *next = nextAction(actions, current);
	while (next && (next != current) && (next->isSeparator() || !next->isVisible())) {
		next = nextAction(actions, next);
	}

	if (next && !next->isSeparator() && next->isVisible()) {
		return next;
	}
	return nullptr;
}

void hoveredAction(QAction *action, bool enterSubmenu)
{
	if (action) {
		if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menu) {
			userData->m_menu->hovered(action);
			if (enterSubmenu && userData->m_isMenuAction && userData->m_submenu) {
				LocalGlobalVars::activeMenu = userData->m_submenu;
				LocalGlobalVars::activeMenu->hoveredNextOrFirstAction();
			}
		}
	}
}

void triggeredAction(QAction *action)
{
	if (action) {
		if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menu) {
			userData->m_menu->triggered(action);
		}
	}
}
}
#endif

bool PLSMenu::event(QEvent *event)
{
	bool result = QMenu::event(event);

	switch (event->type()) {
	case QEvent::ActionAdded:
		emit actionAdded(static_cast<QActionEvent *>(event)->action(), static_cast<QActionEvent *>(event)->before());
		break;
	case QEvent::ActionRemoved:
		emit actionRemoved(static_cast<QActionEvent *>(event)->action());
		break;
	default:
		break;
	}

	return result;
}

#if 0
PLSPopupMenuItem::PLSPopupMenuItem(QAction *action, PLSPopupMenu *menu) : QFrame(menu), m_action(action), m_menu(menu)
{
	setObjectName(action->objectName());
	setActionUserData(action, false, menu, nullptr, this);
	QFrame::setVisible(action->isVisible());
	QObject::connect(m_action, &QAction::changed, this, [this]() { actionChanged(); });
	QObject::connect(m_action, &QAction::objectNameChanged, this, &PLSPopupMenuItem::setObjectName);
}

void PLSPopupMenuItem::setMarginLeft(int marginLeft)
{
	m_marginLeft = marginLeft;
}
void PLSPopupMenuItem::actionHovered()
{
	m_action->hover();
}

void PLSPopupMenuItem::actionTriggered()
{
	m_action->trigger();
}

void PLSPopupMenuItem::actionChanged()
{
	setVisible(m_action->isVisible());
}

QString PLSPopupMenuItem::getText(int &shortcutKey, const QFont &font, int width)
{
	QFontMetrics fm(font);
	QString original = decodeMenuItemText(m_action->text(), shortcutKey);

	setToolTip(fm.horizontalAdvance(original) > width ? original : "");

	QString elidedText = fm.elidedText(original, Qt::ElideRight, width);
	return elidedText;
}

PLSPopupMenuItemSeparator::PLSPopupMenuItemSeparator(QAction *action, PLSPopupMenu *menu) : PLSPopupMenuItem(action, menu) {}

PLSPopupMenuItem::Type PLSPopupMenuItemSeparator::type() const
{
	return Type::Separator;
}

PLSPopupMenuItemContent::PLSPopupMenuItemContent(QAction *action, PLSPopupMenu *menu) : PLSPopupMenuItem(action, menu)
{
	m_layout = pls_new<QHBoxLayout>(this);
	m_layout->setSpacing(0);
	m_layout->setContentsMargins(0, 0, 0, 0);

	m_icon = pls_new<QLabel>(this);
	m_icon->setProperty("menuItemRole", "icon");
	m_icon->setVisible(m_hasIcon);

	m_text = pls_new<QLabel>(decodeMenuItemText(action->text(), m_shortcutKey), this);
	m_text->setProperty("menuItemRole", action->isEnabled() ? "text" : "disabled-text");
	m_text->installEventFilter(this);

	m_spacer = pls_new<QWidget>(this);
	m_spacer->installEventFilter(this);
	m_spacer->setProperty("menuItemRole", "spacer");

	m_layout->addWidget(m_icon);
	m_layout->addWidget(m_text);
	m_layout->addWidget(m_spacer, 1);

	if (menu->toolTipsVisible()) {
		setAttribute(Qt::WA_AlwaysShowToolTips);
		setToolTip(action->toolTip());
	}

	setMouseTracking(true);
	setEnabled(action->isEnabled());
	pls_flush_style_recursive(this, "checked", action->isCheckable() && action->isChecked());

	connect(menu, &PLSPopupMenu::hovered, this, [this](const QAction *act) { pls_flush_style_recursive(this, "selected", this->action() == act); });
	connect(menu, &PLSPopupMenu::triggered, this, [this](const QAction *act) { pls_flush_style_recursive(this, "selected", this->action() == act); });
}

void PLSPopupMenuItemContent::showShortcutKey()
{
	m_text->setText(decodeMenuItemText(action()->text(), m_shortcutKey));
}

void PLSPopupMenuItemContent::shortcutKeyPress(int key)
{
	if (m_shortcutKey == key) {
		menu()->triggered(action());
	}
}

bool PLSPopupMenuItemContent::getHasIcon() const
{
	return m_hasIcon;
}

void PLSPopupMenuItemContent::setHasIcon(bool hasIcon)
{
	m_hasIcon = hasIcon;
	m_icon->setVisible(hasIcon);
}

bool PLSPopupMenuItemContent::getIsLeftIcon() const
{
	return m_isLeftIcon;
}

void PLSPopupMenuItemContent::setIsLeftIcon(bool isLeftIcon)
{
	if (m_isLeftIcon == isLeftIcon) {
		return;
	}

	m_isLeftIcon = isLeftIcon;
	if (m_isLeftIcon) {
		m_layout->removeWidget(m_icon);
		m_layout->insertWidget(0, m_icon);
	} else {
		m_layout->removeWidget(m_icon);
		m_layout->insertWidget(1, m_icon);
	}
}

bool PLSPopupMenuItemContent::toolTipsVisible() const
{
	return m_toolTipsVisible;
}

void PLSPopupMenuItemContent::setToolTipsVisible(bool toolTipsVisible)
{
	if (m_toolTipsVisible != toolTipsVisible) {
		m_toolTipsVisible = toolTipsVisible;
		actionChanged();
	}
}

void PLSPopupMenuItemContent::actionChanged()
{
	PLSPopupMenuItem::actionChanged();
	setProperty("checked", action()->isCheckable() && action()->isChecked());

	if (toolTipsVisible() || menu()->toolTipsVisible()) {
		setAttribute(Qt::WA_AlwaysShowToolTips);
		setToolTip(action()->toolTip());
	} else {
		setAttribute(Qt::WA_AlwaysShowToolTips, false);
		setToolTip(QString());
	}

	m_text->setText(decodeMenuItemText(action()->text(), m_shortcutKey));
	m_text->setProperty("menuItemRole", action()->isEnabled() ? "text" : "disabled-text");
	setEnabled(action()->isEnabled());
}

void PLSPopupMenuItemContent::setMarginLeft(int marginLeft)
{
	PLSPopupMenuItem::setMarginLeft(marginLeft);
	auto margins = contentsMargins();
	margins.setLeft(marginLeft);
	m_layout->setContentsMargins(margins);
}

bool PLSPopupMenuItemContent::eventFilter(QObject *watched, QEvent *event)
{
	bool result = PLSPopupMenuItem::eventFilter(watched, event);
	if (((watched == m_text) || (watched == m_spacer)) && event->type() == QEvent::Resize) {
		auto margins = contentsMargins();
		auto layoutMargins = m_layout->contentsMargins();
		int iconWidth = m_icon->isVisible() ? m_icon->width() : 0;
		m_text->setText(getText(m_shortcutKey, m_text->font(), width() - margins.left() - margins.right() - layoutMargins.left() - layoutMargins.right() - iconWidth - otherWidth()));
	}
	return result;
}

void PLSPopupMenuItemContent::enterEvent(QEnterEvent *event)
{
	menu()->hovered(action());
	PLSPopupMenuItem::enterEvent(event);
}

void PLSPopupMenuItemContent::mouseReleaseEvent(QMouseEvent *event)
{
	menu()->triggered(action());
	PLSPopupMenuItem::mouseReleaseEvent(event);
}

int PLSPopupMenuItemContent::otherWidth() const
{
	return 0;
}

PLSPopupMenuItemAction::PLSPopupMenuItemAction(QAction *action, PLSPopupMenu *menu) : PLSPopupMenuItemContent(action, menu)
{
	QString shortcut = action->shortcut().toString();
	m_shortcut = pls_new<QLabel>(shortcut, this);
	m_shortcut->setProperty("menuItemRole", "shortcut");
	m_shortcut->setVisible(!shortcut.isEmpty());

	layout()->addWidget(m_shortcut);
}

PLSPopupMenuItem::Type PLSPopupMenuItemAction::type() const
{
	return Type::Action;
}

void PLSPopupMenuItemAction::actionHovered()
{
	hideSubmenu(menu());
	PLSPopupMenuItemContent::actionHovered();
}

void PLSPopupMenuItemAction::actionTriggered()
{
	menu()->onHideAllPopupMenu();
	PLSPopupMenuItemContent::actionTriggered();
}

void PLSPopupMenuItemAction::actionChanged()
{
	PLSPopupMenuItemContent::actionChanged();

	QString shortcut = action()->shortcut().toString();
	m_shortcut->setText(shortcut);
	m_shortcut->setVisible(!shortcut.isEmpty());
	pls_flush_style_recursive(this);
}

int PLSPopupMenuItemAction::otherWidth() const
{
	return m_shortcut->isVisible() ? m_shortcut->width() : 0;
}

void PLSPopupMenuItemAction::leaveEvent(QEvent *event)
{
	setProperty("selected", false);
	pls_flush_style_recursive(this);
	PLSPopupMenuItemContent::leaveEvent(event);
}

void PLSPopupMenuItemAction::mouseReleaseEvent(QMouseEvent *event)
{
	hideMenuByUserData(ActionUserdata::getActionUserData(action()));
	PLSPopupMenuItemContent::mouseReleaseEvent(event);
}

PLSPopupMenuItemWidgetAction::PLSPopupMenuItemWidgetAction(QWidgetAction *action, PLSPopupMenu *menu) : PLSPopupMenuItem(action, menu)
{
	QWidget *widget = action->defaultWidget();
	widget->setParent(this);
	pls_flush_style_recursive(this);

	auto layout = pls_new<QHBoxLayout>(this);
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	layout->addWidget(widget);
	widget->show();
}

PLSPopupMenuItemWidgetAction::Type PLSPopupMenuItemWidgetAction::type() const
{
	return Type::WidgetAction;
}

void PLSPopupMenuItemWidgetAction::actionHovered() {}

void PLSPopupMenuItemWidgetAction::actionTriggered() {}

void PLSPopupMenuItemWidgetAction::actionChanged()
{
	PLSPopupMenuItem::actionChanged();
	setEnabled(action()->isEnabled());
	pls_flush_style_recursive(this);
}

PLSPopupMenuItemMenu::PLSPopupMenuItemMenu(QAction *action, PLSPopupMenu *menu) : PLSPopupMenuItemContent(action, menu)
{
	m_arrow = pls_new<QLabel>(this);
	m_arrow->setProperty("menuItemRole", "arrow-normal");

	layout()->addWidget(m_arrow);
}

PLSPopupMenuItem::Type PLSPopupMenuItemMenu::type() const
{
	return Type::Menu;
}

void PLSPopupMenuItemMenu::actionHovered()
{
	if (action()->isEnabled()) {
		popupSubmenu(menu(), action());
	} else {
		hideSubmenu(menu());
	}

	PLSPopupMenuItemContent::actionHovered();
}

void PLSPopupMenuItemMenu::actionTriggered()
{
	if (action()->isEnabled()) {
		popupSubmenu(menu(), action());
	} else {
		hideSubmenu(menu());
	}

	PLSPopupMenuItemContent::actionTriggered();
}

void PLSPopupMenuItemMenu::actionChanged()
{
	PLSPopupMenuItemContent::actionChanged();

	m_arrow->setProperty("menuItemRole", action()->isEnabled() ? "arrow-normal" : "arrow-disable");
	pls_flush_style_recursive(this);
}

int PLSPopupMenuItemMenu::otherWidth() const
{
	return m_arrow->width();
}

void PLSPopupMenuItemMenu::mousePressEvent(QMouseEvent *event)
{
	m_arrow->setProperty("menuItemRole", "arrow-click");
	PLSPopupMenuItemContent::mousePressEvent(event);
}

void PLSPopupMenuItemMenu::mouseReleaseEvent(QMouseEvent *event)
{
	m_arrow->setProperty("menuItemRole", action()->isEnabled() ? "arrow-normal" : "arrow-disable");
	PLSPopupMenuItemContent::mouseReleaseEvent(event);
}

PLSPopupMenu::PLSPopupMenu(QWidget *parent) : PLSPopupMenu(false, parent) {}

PLSPopupMenu::PLSPopupMenu(bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(QString(), toolTipsVisible, parent) {}

PLSPopupMenu::PLSPopupMenu(const QString &title, bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(QIcon(), title, toolTipsVisible, parent) {}

PLSPopupMenu::PLSPopupMenu(const QIcon &icon, const QString &title, bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(icon, title, pls_new<QAction>(icon, title), toolTipsVisible, parent)
{
	m_menuAction->setParent(this);
}

PLSPopupMenu::PLSPopupMenu(QAction *menuAction, QWidget *parent) : PLSPopupMenu(menuAction, menuAction->menu()->toolTipsVisible(), parent) {}

PLSPopupMenu::PLSPopupMenu(QAction *menuAction, bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(menuAction->icon(), menuAction->text(), menuAction, toolTipsVisible, parent)
{
	QMenu *menu = menuAction->menu();
	setObjectName(menu->objectName());
	addActions(menu->actions());
	bindPLSMenu(dynamic_cast<PLSMenu *>(menu));
}

PLSPopupMenu::PLSPopupMenu(QMenu *menu, QWidget *parent) : PLSPopupMenu(menu, menu->toolTipsVisible(), parent) {}

PLSPopupMenu::PLSPopupMenu(QMenu *menu, bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(menu->icon(), menu->title(), toolTipsVisible, parent)
{
	setObjectName(menu->objectName());
	addActions(menu->actions());
	bindPLSMenu(dynamic_cast<PLSMenu *>(menu));
}

PLSPopupMenu::PLSPopupMenu(const PLSMenu *menu, QWidget *parent) : PLSPopupMenu(menu, menu->toolTipsVisible(), parent) {}

PLSPopupMenu::PLSPopupMenu(const PLSMenu *menu, bool toolTipsVisible, QWidget *parent) : PLSPopupMenu(menu->icon(), menu->title(), toolTipsVisible, parent)
{
	setObjectName(menu->objectName());
	addActions(menu->actions());
	bindPLSMenu(menu);
}

PLSPopupMenu::PLSPopupMenu(const QIcon &icon, const QString &title, QAction *menuAction, bool toolTipsVisible, QWidget *parent)
	: QFrame(parent, Qt::Tool | Qt::FramelessWindowHint), m_owner(), m_menuAction(menuAction), m_selectedAction(), m_layout(), m_toolTipsVisible(toolTipsVisible)
{
	pls_set_css(this, {"PLSPopupMenu"});

	setActionUserData(m_menuAction, true, nullptr, this, nullptr);
	setMouseTracking(true);
	m_layout = pls_new<QVBoxLayout>(this);
	m_layout->setSpacing(0);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_menuAction->setIcon(icon);
	m_menuAction->setText(title);

	connect(qApp, &QApplication::applicationStateChanged, this, &PLSPopupMenu::hide);
	installNativeEventFilter();
	//SetAlwaysOnTop(this, "PLSPopupMenu", true);

	connect(this, &PLSPopupMenu::triggered, this, [this](QAction *action) {
		m_selectedAction = action;
		if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
			userData->m_menuItem->actionTriggered();
		}
	});
	connect(this, &PLSPopupMenu::hovered, this, [this](QAction *action) {
		if (LocalGlobalVars::activeMenu != this) {
			LocalGlobalVars::activeMenu = this;
		}

		m_selectedAction = action;
		if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menuItem) {
			userData->m_menuItem->actionHovered();
		}
	});
	connect(this, &PLSPopupMenu::hideAllPopupMenu, this, &PLSPopupMenu::onHideAllPopupMenu, Qt::QueuedConnection);
}

PLSPopupMenu::~PLSPopupMenu()
{
	if (LocalGlobalVars::activeMenu == this) {
		LocalGlobalVars::activeMenu = nullptr;
	}
}

PLSPopupMenu *PLSPopupMenu::owner() const
{
	return m_owner;
}

void PLSPopupMenu::setOwner(PLSPopupMenu *owner)
{
	m_owner = owner;
}

QPointer<QAction> PLSPopupMenu::menuAction() const
{
	return m_menuAction;
}

QString PLSPopupMenu::title() const
{
	return m_menuAction->text();
}

void PLSPopupMenu::setTitle(const QString &title) const
{
	m_menuAction->setText(title);
}

QIcon PLSPopupMenu::icon() const
{
	return m_menuAction->icon();
}

void PLSPopupMenu::setIcon(const QIcon &icon) const
{
	m_menuAction->setIcon(icon);
}

QAction *PLSPopupMenu::addAction(const QString &text)
{
	auto action = pls_new<QAction>(text, this);
	addAction(action);
	return action;
}

QAction *PLSPopupMenu::addAction(const QIcon &icon, const QString &text)
{
	auto action = pls_new<QAction>(icon, text, this);
	addAction(action);
	return action;
}

QAction *PLSPopupMenu::addAction(const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut)
{
	auto action = pls_new<QAction>(text, this);
	action->setShortcut(shortcut);
	QObject::connect(action, SIGNAL(triggered(bool)), receiver, member);
	addAction(action);
	return action;
}

QAction *PLSPopupMenu::addAction(const QIcon &icon, const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut)
{
	auto action = pls_new<QAction>(icon, text, this);
	action->setShortcut(shortcut);
	QObject::connect(action, SIGNAL(triggered(bool)), receiver, member);
	addAction(action);
	return action;
}

QAction *PLSPopupMenu::addMenu(const PLSPopupMenu *menu)
{
	auto action = menu->menuAction();
	addAction(action);
	return action;
}

PLSPopupMenu *PLSPopupMenu::addMenu(const QString &title)
{
	auto menu = pls_new<PLSPopupMenu>(title, this);
	addMenu(menu);
	return menu;
}

PLSPopupMenu *PLSPopupMenu::addMenu(const QIcon &icon, const QString &title)
{
	auto menu = pls_new<PLSPopupMenu>(icon, title, this);
	addMenu(menu);
	return menu;
}

QAction *PLSPopupMenu::addSeparator()
{
	auto action = pls_new<QAction>(this);
	action->setSeparator(true);
	addAction(action);
	return action;
}

QAction *PLSPopupMenu::insertMenu(QAction *before, const PLSPopupMenu *menu)
{
	auto action = menu->menuAction();
	insertAction(before, action);
	return action;
}

QAction *PLSPopupMenu::insertSeparator(QAction *before)
{
	auto action = pls_new<QAction>(this);
	action->setSeparator(true);
	insertAction(before, action);
	return action;
}

int PLSPopupMenu::getMarginLeft() const
{
	return m_layout->contentsMargins().left();
}

void PLSPopupMenu::setMarginLeft(int left)
{
	QMargins margins = m_layout->contentsMargins();
	margins.setLeft(left);
	m_layout->setContentsMargins(margins);
}

int PLSPopupMenu::getMarginTop() const
{
	return m_layout->contentsMargins().top();
}

void PLSPopupMenu::setMarginTop(int top)
{
	QMargins margins = m_layout->contentsMargins();
	margins.setTop(top);
	m_layout->setContentsMargins(margins);
}

int PLSPopupMenu::getMarginRight() const
{
	return m_layout->contentsMargins().right();
}

void PLSPopupMenu::setMarginRight(int right)
{
	QMargins margins = m_layout->contentsMargins();
	margins.setRight(right);
	m_layout->setContentsMargins(margins);
}

int PLSPopupMenu::getMarginBottom() const
{
	return m_layout->contentsMargins().bottom();
}

void PLSPopupMenu::setMarginBottom(int bottom)
{
	QMargins margins = m_layout->contentsMargins();
	margins.setBottom(bottom);
	m_layout->setContentsMargins(margins);
}

void PLSPopupMenu::asButtonPopupMenu(QPushButton *button, const QPoint &offset)
{
	connect(button, &QPushButton::clicked, this, [button, offset, this]() {
		if (!isVisible()) {
			LocalGlobalVars::menuButton = button;
			onHideAllPopupMenu();
			exec(button, offset);
		} else {
			LocalGlobalVars::menuButton = nullptr;
			hide();
		}
	});
}

static PLSPopupMenu *getIntersectsMenu(PLSPopupMenu *menu, const QRect &menuRect)
{
	int i = LocalGlobalVars::menuList.indexOf(menu);
	if (i < 0) {
		return nullptr;
	}

	for (; i >= 0; --i) {
		PLSPopupMenu *popupMenu = LocalGlobalVars::menuList.at(i);
		if (popupMenu->geometry().intersects(menuRect)) {
			return popupMenu;
		}
	}
	return nullptr;
}

static QPoint calcPopupPos(PLSPopupMenu *menu, const QPoint &pos, const QAction *action)
{
	menu->adjustSize();

	QRect savr = pls_get_screen_available_rect(pos);
	QSize size = menu->size();

	QPoint newPos(pos);
	if ((pos.x() - savr.x() + size.width()) > savr.width()) {
		newPos.setX(pos.x() > size.width() ? pos.x() - size.width() : 0);
	}

	if ((pos.y() - savr.y() + size.height()) > savr.height()) {
		newPos.setY(pos.y() > size.height() ? pos.y() - size.height() : 0);
	}

	if (auto userData = ActionUserdata::getActionUserData(action); userData && userData->m_menu) {
		if (PLSPopupMenu *intersectsMenu = getIntersectsMenu(userData->m_menu, QRect(newPos, size)); intersectsMenu) {
			if ((pos.x() - savr.x() + size.width()) > savr.width()) {
				newPos.setX(newPos.x() - userData->m_menu->geometry().width());
			} else {
				newPos.setX(newPos.x() - userData->m_menu->geometry().width() - size.width());
			}

			intersectsMenu = getIntersectsMenu(intersectsMenu, QRect(newPos, size));
			while (intersectsMenu) {
				newPos.setX(newPos.x() - intersectsMenu->geometry().width());
				intersectsMenu = getIntersectsMenu(intersectsMenu, QRect(newPos, size));
			}
		}
	}

	return newPos;
}

void PLSPopupMenu::popup(const QPoint &pos, const QAction *action)
{
	if (!LocalGlobalVars::menuList.contains(this)) {
		LocalGlobalVars::menuList.append(this);
	}

	clearSelectedAction();
	ensurePolished();
	pls_flush_style_recursive(this);
	move(calcPopupPos(this, pos, action));
	show();
}

void PLSPopupMenu::popup(const QPushButton *button, const QPoint &offset, const QAction *action)
{
	popup(button->mapToGlobal(QPoint(0, button->size().height())) + offset, action);
}

void PLSPopupMenu::exec(const QPoint &pos, const QAction *action)
{
	QEventLoop eventLoop;
	connect(this, &PLSPopupMenu::hidden, &eventLoop, &QEventLoop::quit);
	popup(pos, action);
	LocalGlobalVars::activeMenu = this;
	eventLoop.exec();
}

void PLSPopupMenu::exec(const QPushButton *button, const QPoint &offset, const QAction *action)
{
	QEventLoop eventLoop;
	connect(this, &PLSPopupMenu::hidden, &eventLoop, &QEventLoop::quit);
	popup(button, offset, action);
	LocalGlobalVars::activeMenu = this;
	eventLoop.exec();
}

QAction *PLSPopupMenu::selectedAction() const
{
	return m_selectedAction;
}

void PLSPopupMenu::enterSubmenu()
{
	if (m_selectedAction) {
		hoveredAction(m_selectedAction, true);
		return;
	}

	QList<QAction *> actions = this->actions();
	if (!actions.isEmpty()) {
		m_selectedAction = actions.first();
		hoveredAction(m_selectedAction, true);
	}
}

void PLSPopupMenu::leaveSubmenu()
{
	LocalGlobalVars::activeMenu = m_owner;
	hideSubmenu(this);
	clearSelectedAction();
	hide();
}

void PLSPopupMenu::triggeredSelectedOrFirstAction()
{
	if (m_selectedAction) {
		triggeredAction(m_selectedAction);
	} else if (QList<QAction *> actions = this->actions(); !actions.isEmpty()) {
		m_selectedAction = actions.first();
		triggeredAction(m_selectedAction);
	}
}

void PLSPopupMenu::hoveredPrevOrLastAction()
{
	QList<QAction *> actions = this->actions();
	if (actions.isEmpty()) {
		return;
	}

	QAction *prev = nullptr;
	if (m_selectedAction) {
		prev = prevValidAction(actions, m_selectedAction);
	} else {
		prev = actions.last();
		if (prev->isSeparator() || !prev->isVisible()) {
			prev = prevValidAction(actions, prev);
		}
	}

	if (prev) {
		hoveredAction(prev, false);
	}
}

void PLSPopupMenu::hoveredNextOrFirstAction()
{
	QList<QAction *> actions = this->actions();
	if (actions.isEmpty()) {
		return;
	}

	QAction *next = nullptr;
	if (m_selectedAction) {
		next = nextValidAction(actions, m_selectedAction);
	} else {
		next = actions.first();
		if (next->isSeparator() || !next->isVisible()) {
			next = nextValidAction(actions, next);
		}
	}

	if (next) {
		hoveredAction(next, false);
	}
}

void PLSPopupMenu::clearSelectedAction()
{
	if (m_selectedAction) {
		setMenuItemProperty(m_selectedAction, "selected", false);
		m_selectedAction = nullptr;
	}
}

void PLSPopupMenu::setSelectedAction(QAction *selectedAction)
{
	hoveredAction(selectedAction, true);
}

void PLSPopupMenu::setHasIcon(const QAction *action, bool hasIcon)
{
	setMenuItemProperty(action, "hasIcon", hasIcon);
}

void PLSPopupMenu::setToolTipsVisible(const QAction *action, bool toolTipsVisible)
{
	setMenuItemProperty(action, "toolTipsVisible", toolTipsVisible);
}

void PLSPopupMenu::setToolTips(QAction *action, const QString &tooltips)
{
	action->setToolTip(tooltips);
	setToolTipsVisible(action, !tooltips.isEmpty());
}

bool PLSPopupMenu::isEmpty() const
{
	auto acts = actions();
	return std::all_of(acts.begin(), acts.end(), [](const QAction *act) { return !(!act->isSeparator() && act->isVisible()); });
}

bool PLSPopupMenu::toolTipsVisible() const
{
	return m_toolTipsVisible;
}

void PLSPopupMenu::setToolTipsVisible(bool visible)
{
	m_toolTipsVisible = visible;

	for (QAction *action : actions()) {
		action->changed();
	}
}

void PLSPopupMenu::bindPLSMenu(const PLSMenu *menu)
{
	if (!menu) {
		return;
	}

	connect(menu, &PLSMenu::actionAdded, this, [this](QAction *action, QAction *before) { insertAction(before, action); });
	connect(menu, &PLSMenu::actionRemoved, this, [this](QAction *action) { removeAction(action); });
}

void PLSPopupMenu::onHideAllPopupMenu() const
{
	for (PLSPopupMenu *menu : LocalGlobalVars::menuList) {
		menu->hide();
	}
}

bool PLSPopupMenu::event(QEvent *event)
{
	bool result = QFrame::event(event);

	switch (event->type()) {
	case QEvent::ActionAdded:
		if (auto menuItem = newMenuItem(this, static_cast<QActionEvent *>(event)->action())) {
			m_layout->insertWidget(actionIndex(actions(), static_cast<QActionEvent *>(event)->before()), menuItem);
		}
		break;
	case QEvent::ActionRemoved:
		event_ActionRemoved(event);
		break;
	default:
		break;
	}

	return result;
}

void PLSPopupMenu::showEvent(QShowEvent *event)
{
	emit shown();
	QFrame::showEvent(event);
}

void PLSPopupMenu::hideEvent(QHideEvent *event)
{
	LocalGlobalVars::menuList.removeOne(this);

	if (LocalGlobalVars::activeMenu == this) {
		LocalGlobalVars::activeMenu = nullptr;
	}

	clearSelectedAction();
	setMenuItemProperty(m_menuAction, "selected", false);
	// hide all submenus when menu hide
	hideSubmenu(this);

	QFrame::hideEvent(event);
	emit hidden();
}

void PLSPopupMenu::event_ActionRemoved(QEvent *event)
{
	auto action = static_cast<QActionEvent *>(event)->action();
	if (m_selectedAction == action) {
		m_selectedAction = nullptr;
	}

	if (ActionUserdata *userData = ActionUserdata::getActionUserData(action); userData) {
		if (userData->m_submenu) {
			userData->m_submenu->deleteLater();
			userData->m_submenu = nullptr;
		}

		if (userData->m_menuItem) {
			userData->m_menuItem->hide();
			m_layout->removeWidget(userData->m_menuItem);
			userData->m_menuItem->deleteLater();
			userData->m_menuItem = nullptr;
		}
	}
}

#include "PLSMenu.moc"

#endif
