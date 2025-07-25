#include "moc_window-extra-browsers.cpp"
#include "window-dock-browser.hpp"
#include "window-basic-main.hpp"

#include <qt-wrappers.hpp>
#include "PLSBasic.h"

#include <QLineEdit>
#include <QHBoxLayout>
#include <QUuid>

#include <json11.hpp>

#include "ui_OBSExtraBrowsers.h"

using namespace json11;

#define OBJ_NAME_SUFFIX "_extraBrowser"

enum class Column : int {
	Title,
	Url,
	Delete,

	Count,
};

/* ------------------------------------------------------------------------- */

void ExtraBrowsersModel::Reset()
{
	items.clear();

	OBSBasic *main = OBSBasic::Get();

	for (int i = 0; i < main->extraBrowserDocks.size(); i++) {
		Item item;
		item.prevIdx = i;
		item.title = main->extraBrowserDockNames[i];
		item.url = main->extraBrowserDockTargets[i];
		items.push_back(item);
	}
}

int ExtraBrowsersModel::rowCount(const QModelIndex &) const
{
	int count = items.size() + 1;
	return count;
}

int ExtraBrowsersModel::columnCount(const QModelIndex &) const
{
	return (int)Column::Count;
}

QVariant ExtraBrowsersModel::data(const QModelIndex &index, int role) const
{
	int column = index.column();
	int idx = index.row();
	int count = items.size();
	bool validRole = role == Qt::DisplayRole || role == Qt::AccessibleTextRole;

	if (!validRole)
		return QVariant();

	if (idx >= 0 && idx < count) {
		switch (column) {
		case (int)Column::Title:
			return items[idx].title;
		case (int)Column::Url:
			return items[idx].url;
		}
	} else if (idx == count) {
		switch (column) {
		case (int)Column::Title:
			return newTitle;
		case (int)Column::Url:
			return newURL;
		}
	}

	return QVariant();
}

QVariant ExtraBrowsersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	bool validRole = role == Qt::DisplayRole || role == Qt::AccessibleTextRole;

	if (validRole && orientation == Qt::Orientation::Horizontal) {
		switch (section) {
		case (int)Column::Title:
			return QTStr("ExtraBrowsers.DockName");
		case (int)Column::Url:
			return QStringLiteral("URL");
		}
	}

	return QVariant();
}

Qt::ItemFlags ExtraBrowsersModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = QAbstractTableModel::flags(index);

	if (index.column() != (int)Column::Delete)
		flags |= Qt::ItemIsEditable;

	return flags;
}

class DelButton : public QPushButton {
public:
	inline DelButton(QModelIndex index_) : QPushButton(), index(index_) {}

	QPersistentModelIndex index;
};

class EditWidget : public QLineEdit {
public:
	inline EditWidget(QWidget *parent, QModelIndex index_) : QLineEdit(parent), index(index_) {}

	QPersistentModelIndex index;
};

void ExtraBrowsersModel::AddDeleteButton(int idx, bool disable)
{
	QTableView *widget = reinterpret_cast<QTableView *>(parent());

	QModelIndex index = createIndex(idx, (int)Column::Delete, nullptr);

	QPushButton *del = new DelButton(index);
	del->setDisabled(disable);
	del->setObjectName("extraPanelDelete");
	del->setMinimumSize(QSize(22, 22));
	connect(del, &QPushButton::clicked, this, &ExtraBrowsersModel::DeleteItem);

	widget->setIndexWidget(index, del);
	widget->setRowHeight(idx, 50);
	widget->setColumnWidth(idx, 42);
}

void ExtraBrowsersModel::CheckToAdd()
{
	if (newTitle.isEmpty() || newURL.isEmpty()) {
		AddDeleteButton(items.size(), true);
		return;
	}

	int idx = items.size() + 1;
	beginInsertRows(QModelIndex(), idx, idx);

	Item item;
	item.prevIdx = -1;
	item.title = newTitle;
	item.url = newURL;
	items.push_back(item);

	newTitle = "";
	newURL = "";

	endInsertRows();

	AddDeleteButton(idx - 1);
	//renjinbo/#3510/add the last one row delete btn, and make it disable
	AddDeleteButton(idx, true);
}

void ExtraBrowsersModel::UpdateItem(Item &item)
{
	int idx = item.prevIdx;

	OBSBasic *main = OBSBasic::Get();
	BrowserDock *dock = reinterpret_cast<BrowserDock *>(main->extraBrowserDocks[idx].get());
	dock->setWindowTitle(item.title);
	dock->setObjectName(item.title + OBJ_NAME_SUFFIX);
	dock->setObjectName("browserDock");

	if (main->extraBrowserDockNames[idx] != item.title) {
		main->extraBrowserDockNames[idx] = item.title;
		dock->toggleViewAction()->setText(item.title);
		dock->setTitle(item.title);
	}

	if (main->extraBrowserDockTargets[idx] != item.url) {
		dock->cefWidget->setURL(QT_TO_UTF8(item.url));
		main->extraBrowserDockTargets[idx] = item.url;
	}
}

void ExtraBrowsersModel::DeleteItem()
{
	QTableView *widget = reinterpret_cast<QTableView *>(parent());

	DelButton *del = reinterpret_cast<DelButton *>(sender());
	int row = del->index.row();

	/* there's some sort of internal bug in Qt and deleting certain index
	 * widgets or "editors" that can cause a crash inside Qt if the widget
	 * is not manually removed, at least on 5.7 */
	widget->setIndexWidget(del->index, nullptr);
	del->deleteLater();

	/* --------- */

	beginRemoveRows(QModelIndex(), row, row);

	int prevIdx = items[row].prevIdx;
	items.removeAt(row);

	if (prevIdx != -1) {
		int i = 0;
		for (; i < deleted.size() && deleted[i] < prevIdx; i++)
			;
		deleted.insert(i, prevIdx);
	}

	endRemoveRows();
}

void ExtraBrowsersModel::Apply()
{
	OBSBasic *main = OBSBasic::Get();

	for (Item &item : items) {
		if (item.prevIdx != -1) {
			UpdateItem(item);
		} else {
			QString uuid = QUuid::createUuid().toString();
			uuid.replace(QRegularExpression("[{}-]"), "");
			main->AddExtraBrowserDock(item.title, item.url, uuid, true);
		}
	}

	for (int i = deleted.size() - 1; i >= 0; i--) {
		int idx = deleted[i];
		main->extraBrowserDockTargets.removeAt(idx);
		main->extraBrowserDockNames.removeAt(idx);
		main->extraBrowserDocks.removeAt(idx);
	}

	if (main->extraBrowserDocks.empty())
		main->extraBrowserMenuDocksSeparator.clear();

	deleted.clear();

	Reset();
}

void ExtraBrowsersModel::TabSelection(bool forward)
{
	QListView *widget = reinterpret_cast<QListView *>(parent());
	QItemSelectionModel *selModel = widget->selectionModel();

	QModelIndex sel = selModel->currentIndex();
	int row = sel.row();
	int col = sel.column();

	switch (sel.column()) {
	case (int)Column::Title:
		if (!forward) {
			if (row == 0) {
				return;
			}

			row -= 1;
		}

		col += 1;
		break;

	case (int)Column::Url:
		if (forward) {
			if (row == items.size()) {
				return;
			}

			row += 1;
		}

		col -= 1;
	}

	sel = createIndex(row, col, nullptr);
	selModel->setCurrentIndex(sel, QItemSelectionModel::Clear);
}

void ExtraBrowsersModel::Init()
{
	for (int i = 0; i < items.count(); i++)
		AddDeleteButton(i);
	AddDeleteButton(items.count(), true);
}

/* ------------------------------------------------------------------------- */

QWidget *ExtraBrowsersDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &,
					     const QModelIndex &index) const
{
	QLineEdit *text = new EditWidget(parent, index);
	text->setObjectName("text");
	text->installEventFilter(const_cast<ExtraBrowsersDelegate *>(this));
	text->setSizePolicy(QSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding,
					QSizePolicy::ControlType::LineEdit));
	return text;
}

void ExtraBrowsersDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QLineEdit *text = static_cast<QLineEdit *>(editor);
	text->blockSignals(true);
	text->setText(index.data().toString());
	text->blockSignals(false);
}

bool ExtraBrowsersDelegate::eventFilter(QObject *object, QEvent *event)
{
	QLineEdit *edit = qobject_cast<QLineEdit *>(object);
	if (!edit)
		return false;

	if (LineEditCanceled(event)) {
		RevertText(edit);
	}
	if (LineEditChanged(event)) {
		UpdateText(edit);

		if (event->type() == QEvent::KeyPress) {
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
			if (keyEvent->key() == Qt::Key_Tab) {
				model->TabSelection(true);
			} else if (keyEvent->key() == Qt::Key_Backtab) {
				model->TabSelection(false);
			}
		}
		return true;
	}

	return false;
}

bool ExtraBrowsersDelegate::ValidName(const QString &name) const
{
	for (auto &item : model->items) {
		if (name.compare(item.title, Qt::CaseInsensitive) == 0) {
			return false;
		}
	}
	return true;
}

void ExtraBrowsersDelegate::RevertText(QLineEdit *edit_)
{
	EditWidget *edit = reinterpret_cast<EditWidget *>(edit_);
	int row = edit->index.row();
	int col = edit->index.column();
	bool newItem = (row == model->items.size());

	QString oldText;
	if (col == (int)Column::Title) {
		oldText = newItem ? model->newTitle : model->items[row].title;
	} else {
		oldText = newItem ? model->newURL : model->items[row].url;
	}

	edit->setText(oldText);
}

bool ExtraBrowsersDelegate::UpdateText(QLineEdit *edit_)
{
	EditWidget *edit = reinterpret_cast<EditWidget *>(edit_);
	int row = edit->index.row();
	int col = edit->index.column();
	bool newItem = (row == model->items.size());

	QString text = edit->text().trimmed();

	if (!newItem && text.isEmpty()) {
		return false;
	}

	if (col == (int)Column::Title) {
		QString oldText = newItem ? model->newTitle : model->items[row].title;
		bool same = oldText.compare(text, Qt::CaseInsensitive) == 0;

		if (!same && !ValidName(text)) {
			edit->setText(oldText);
			return false;
		}
	}

	if (!newItem) {
		/* if edited existing item, update it*/
		switch (col) {
		case (int)Column::Title:
			model->items[row].title = text;
			break;
		case (int)Column::Url:
			model->items[row].url = text;
			break;
		}
	} else {
		/* if both new values filled out, create new one */
		switch (col) {
		case (int)Column::Title:
			model->newTitle = text;
			break;
		case (int)Column::Url:
			model->newURL = text;
			break;
		}
		model->CheckToAdd();
	}
	emit commitData(edit);
	return true;
}

/* ------------------------------------------------------------------------- */

OBSExtraBrowsers::OBSExtraBrowsers(QWidget *parent) : PLSDialogView(parent), ui(new Ui::OBSExtraBrowsers)
{
	setupUi(ui);
	pls_add_css(this, {"OBSExtraBrowsers"});
	setAttribute(Qt::WA_DeleteOnClose, true);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
#ifdef __APPLE__
	ui->helpIcon->setToolTip(QTStr("ExtraBrowsers.Info"));
#else
	ui->helpIcon->setVisible(false);
	setHasHelpButton(true);
	setHelpButtonToolTip(QTStr("ExtraBrowsers.Info"), 16);
#endif

	model = new ExtraBrowsersModel(ui->table);

	ui->label->setVisible(false);
	ui->table->horizontalHeader()->setVisible(false);
	ui->table->setModel(model);
	ui->table->setItemDelegateForColumn((int)Column::Title, new ExtraBrowsersDelegate(model));
	ui->table->setItemDelegateForColumn((int)Column::Url, new ExtraBrowsersDelegate(model));
	ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
	ui->table->horizontalHeader()->setSectionResizeMode((int)Column::Delete, QHeaderView::ResizeMode::Fixed);
	ui->table->setColumnWidth((int)Column::Delete, 42);
	ui->table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
	ui->table->verticalHeader()->setDefaultSectionSize(50);
	ui->table->setEditTriggers(QAbstractItemView::EditTrigger::CurrentChanged);
	connect(ui->close, &QPushButton::clicked, this, &OBSExtraBrowsers::close);
	ui->apply->setFocusPolicy(Qt::StrongFocus);
}

OBSExtraBrowsers::~OBSExtraBrowsers() {}

void OBSExtraBrowsers::closeEvent(QCloseEvent *event)
{
	PLSDialogView::closeEvent(event);
	model->Apply();
}

void OBSExtraBrowsers::on_apply_clicked()
{
	model->Apply();
}

/* ------------------------------------------------------------------------- */

void OBSBasic::ClearExtraBrowserDocks()
{
	extraBrowserDockTargets.clear();
	extraBrowserDockNames.clear();
	extraBrowserDocks.clear();
}

void OBSBasic::LoadExtraBrowserDocks()
{
	const char *jsonStr = config_get_string(App()->GetUserConfig(), "BasicWindow", "ExtraBrowserDocks");

	std::string err;
	Json json = Json::parse(jsonStr, err);
	if (!err.empty())
		return;

	Json::array array = json.array_items();
	if (!array.empty())
		extraBrowserMenuDocksSeparator = ui->menuDocks->addSeparator();

	for (Json &item : array) {
		std::string title = item["title"].string_value();
		std::string url = item["url"].string_value();
		std::string uuid = item["uuid"].string_value();

		AddExtraBrowserDock(title.c_str(), url.c_str(), uuid.c_str(), false);
	}
}

void OBSBasic::SaveExtraBrowserDocks()
{
	Json::array array;
	for (int i = 0; i < extraBrowserDocks.size(); i++) {
		QDockWidget *dock = extraBrowserDocks[i].get();
		QString title = extraBrowserDockNames[i];
		QString url = extraBrowserDockTargets[i];
		QString uuid = dock->property("uuid").toString();
		Json::object obj{
			{"title", QT_TO_UTF8(title)},
			{"url", QT_TO_UTF8(url)},
			{"uuid", QT_TO_UTF8(uuid)},
		};
		array.push_back(obj);
	}

	std::string output = Json(array).dump();
	config_set_string(App()->GetUserConfig(), "BasicWindow", "ExtraBrowserDocks", output.c_str());
}

void OBSBasic::ManageExtraBrowserDocks()
{
	if (!extraBrowsers.isNull()) {
		extraBrowsers->show();
		extraBrowsers->raise();
		return;
	}

	extraBrowsers = new OBSExtraBrowsers(this);
	extraBrowsers->show();
}

void OBSBasic::loadNcb2bBrowserSettingsDocks()
{
	const char *jsonStr = config_get_string(App()->GetUserConfig(), "BasicWindow", "Ncb2bBrowserDocks");

	std::string err;
	Json json = Json::parse(jsonStr, err);
	if (!err.empty())
		return;

	Json::array array = json.array_items();
	if (!array.empty())
		ncb2bMenuDocksSeparator = ui->menuDocks->addSeparator();

	for (Json &item : array) {
		std::string title = item["title"].string_value();
		std::string url = item["url"].string_value();
		std::string uuid = item["uuid"].string_value();

		addNcb2bCustomDock(title.c_str(), url.c_str(), uuid.c_str(), false);
	}
}

void OBSBasic::saveNcb2bBrowserSettingsDocks()
{
	Json::array array;
	for (int i = 0; i < ncb2bCustomDocks.size(); i++) {
		QDockWidget *dock = ncb2bCustomDocks[i].get();
		QString title = ncb2bCustomDockNames[i];
		QString url = ncb2bCustomDockUrls[i];
		QString uuid = dock->property("uuid").toString();
		Json::object obj{
			{"title", QT_TO_UTF8(title)},
			{"url", QT_TO_UTF8(url)},
			{"uuid", QT_TO_UTF8(uuid)},
		};
		array.push_back(obj);
	}

	std::string output = Json(array).dump();
	config_set_string(App()->GetUserConfig(), "BasicWindow", "Ncb2bBrowserDocks", output.c_str());
}

void OBSBasic::AddExtraBrowserDock(const QString &title, const QString &url, const QString &uuid, bool firstCreate)
{
	BrowserDock *dock = createBrowserDock(title, url, uuid, firstCreate);
	extraBrowserDocks.push_back(std::shared_ptr<QDockWidget>(dock));
	extraBrowserDockNames.push_back(title);
	extraBrowserDockTargets.push_back(url);
}

BrowserDock *OBSBasic::createBrowserDock(const QString &title, const QString &url, const QString &uuid,
					 bool firstCreate, QByteArray geometry, bool ncb2b)
{
	static int panel_version = -1;
	if (panel_version == -1) {
		panel_version = obs_browser_qcef_version();
	}

	BrowserDock *dock = new BrowserDock(title);
	QString bId(uuid.isEmpty() ? QUuid::createUuid().toString() : uuid);
	bId.replace(QRegularExpression("[{}-]"), "");
	PLSBasic::instance()->CreateAdvancedButtonForBrowserDock(dock, bId);
	dock->setProperty("uuid", bId);
	dock->setObjectName(title + OBJ_NAME_SUFFIX);
	dock->setObjectName("browserDock");
	dock->resize(460, 600);
	dock->setMinimumSize(80, 80);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	mainView->addCloseListener([dock = QPointer<BrowserDock>(dock)]() {
		if (dock) {
			dock->closeBrowser();
		}
	});

	QCefWidget *browser = cef->create_widget(dock, QT_TO_UTF8(url), nullptr);
	if (browser && panel_version >= 1)
		browser->allowAllPopups(true);

	dock->SetWidget(browser);
	dock->setWindowTitle(title);

	/* Add support for Twitch Dashboard panels */
	if (url.contains("twitch.tv/popout") && url.contains("dashboard/live")) {
		QRegularExpression re("twitch.tv\\/popout\\/([^/]+)\\/");
		QRegularExpressionMatch match = re.match(url);
		QString username = match.captured(1);
		if (username.length() > 0) {
			std::string script;
			script = "Object.defineProperty(document, 'referrer', { get: () => '";
			script += "https://twitch.tv/";
			script += QT_TO_UTF8(username);
			script += "/dashboard/live";
			script += "'});";
			browser->setStartupScript(script);
		}
	}
	if (ncb2b) {
		AddDockWidget(dock, Qt::RightDockWidgetArea, false, true);
	} else {
		AddDockWidget(dock, Qt::RightDockWidgetArea, true);
	}
	if (firstCreate) {
		dock->setFloating(true);
		QPoint curPos = pos();
		QSize wSizeD2 = size() / 2;
		QSize dSizeD2 = dock->size() / 2;
		curPos.setX(curPos.x() + qAbs(wSizeD2.width() - dSizeD2.width()));
		curPos.setY(curPos.y() + qAbs(wSizeD2.height() - dSizeD2.height()));
		dock->move(curPos);
		dock->setVisible(true);
		dock->setProperty("vis", true);
	}

	else if (!geometry.isEmpty()) {
		dock->restoreGeometry(geometry);
	}
	return dock;
}
