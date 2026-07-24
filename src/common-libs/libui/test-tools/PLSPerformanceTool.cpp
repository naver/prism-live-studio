#include "PLSPerformanceTool.hpp"
#include "ui_PLSPerformanceTool.h"
#include <libutils-api.h>
#include <qjsondocument.h>
#include <qfiledialog.h>
#include <qdatetime.h>
#include <qset.h>
#include <qtreewidget.h>

PLSPerformanceTool::PLSPerformanceTool(QWidget * /*parent*/) : PLSToolView<PLSPerformanceTool>(nullptr, Qt::Window), ui(new Ui::PLSPerformanceTool)
{
	initSize(1400, 720);
	setupUi(ui);
	ui->tree->header()->resizeSection(0, 300);
	connect(ui->searchKeyword, &QLineEdit::returnPressed, this, &PLSPerformanceTool::on_filterButton_clicked);
	pls_async_invoke(this, [this]() { refresh(); });

	ui->configWidget->setVisible(false);
	ui->configCheckBox->setChecked(false);
}
PLSPerformanceTool::~PLSPerformanceTool()
{
	delete ui;
}

void PLSPerformanceTool::add(const pls::performance::PLSStatsDataPtr statsData) const
{
	std::unique_lock lock2(statsData->mutex);
	if (auto item = createItem(statsData); item) {
		ui->tree->addTopLevelItem(item);
		item->setExpanded(true);
		for (auto child : statsData->children) {
			add(item, child);
		}
	}
}
void PLSPerformanceTool::add(QTreeWidgetItem *parent, const pls::performance::PLSStatsDataPtr statsData) const
{
	std::unique_lock lock2(statsData->mutex);
	if (auto item = createItem(statsData); item) {
		parent->addChild(item);
		item->setExpanded(true);
		for (auto child : statsData->children) {
			add(item, child);
		}
	}
}

static QString getPosition(const std::string &file, int line)
{
	if (!file.empty() && line > 0)
		return QStringLiteral("%1:%2").arg(pls_get_path_file_name(QString::fromStdString(file))).arg(line);
	return QString();
}

QTreeWidgetItem *PLSPerformanceTool::createItem(pls::performance::PLSStatsDataPtr statsData) const
{
	if (statsData->file == "tree-group" || statsData->line == -1000)
		return new QTreeWidgetItem({QString::fromStdString(statsData->function)});
	else if (auto minElapsed = statsData->getMinElapsed(); minElapsed >= m_minimumTime)
		return new QTreeWidgetItem({QString::fromStdString(statsData->function),              //
					    getPosition(statsData->file, statsData->line),            //
					    QString::number(statsData->count),                        //
					    elapsedToString(qMax(minElapsed, 0)),                     //
					    elapsedToString(qMax(statsData->getMaxElapsed(), 0)),     //
					    elapsedToString(qMax(statsData->getAverageElapsed(), 0)), //
					    QString::fromStdString(statsData->comment)});
	return nullptr;
}
static bool matchItem(const QTreeWidgetItem *item, const QString &keyword)
{
	if (keyword.isEmpty())
		return true;

	auto name = item->text(0);
	auto position = item->text(1);
	auto comment = item->text(6);
	if (name.contains(keyword, Qt::CaseInsensitive) || position.contains(keyword, Qt::CaseInsensitive) || comment.contains(keyword, Qt::CaseInsensitive))
		return true;
	return false;
}
static QTreeWidgetItem *prevFind(const QTreeWidgetItem *parent, const QString &keyword)
{
	for (auto i = parent->childCount() - 1; i >= 0; --i) {
		auto child = parent->child(i);
		if (auto found = prevFind(child, keyword); found) {
			return found;
		} else if (matchItem(child, keyword)) {
			return child;
		}
	}
	return nullptr;
}
QTreeWidgetItem *PLSPerformanceTool::findPrevItem(const QString &keyword, QTreeWidgetItem *current)
{
	if (!current) {
		for (auto i = ui->tree->topLevelItemCount() - 1; i >= 0; --i) {
			auto item = ui->tree->topLevelItem(i);
			if (auto found = prevFind(item, keyword); found) {
				return found;
			} else if (matchItem(item, keyword)) {
				return item;
			}
		}
		return nullptr;
	}

	for (auto parent = current->parent(); parent; parent = current->parent()) {
		for (auto i = parent->indexOfChild(current) - 1; i >= 0; --i) {
			auto child = parent->child(i);
			if (auto found = prevFind(child, keyword); found) {
				return found;
			} else if (matchItem(child, keyword)) {
				return child;
			}
		}

		if (matchItem(parent, keyword)) {
			return parent;
		}
		current = parent;
	}
	return nullptr;
}
static QTreeWidgetItem *nextFind(const QTreeWidgetItem *parent, const QString &keyword)
{
	for (auto i = 0, count = parent->childCount(); i < count; ++i) {
		if (auto child = parent->child(i); matchItem(child, keyword)) {
			return child;
		} else if (auto found = nextFind(child, keyword); found) {
			return found;
		}
	}
	return nullptr;
}
QTreeWidgetItem *PLSPerformanceTool::findNextItem(const QString &keyword, QTreeWidgetItem *current)
{
	if (!current) {
		for (auto i = 0, count = ui->tree->topLevelItemCount(); i < count; ++i) {
			if (auto item = ui->tree->topLevelItem(i); matchItem(item, keyword)) {
				return item;
			} else if (auto found = nextFind(item, keyword); found) {
				return found;
			}
		}
		return nullptr;
	} else if (auto item = nextFind(current, keyword); item) {
		return item;
	}

	for (auto parent = current->parent(); parent; parent = current->parent()) {
		for (auto i = parent->indexOfChild(current) + 1, count = parent->childCount(); i < count; ++i) {
			if (auto child = parent->child(i); matchItem(child, keyword)) {
				return child;
			} else if (auto found = nextFind(child, keyword); found) {
				return found;
			}
		}
		current = parent;
	}
	return nullptr;
}
bool PLSPerformanceTool::filterItem(const QString &keyword, QTreeWidgetItem *parent)
{
	bool found = false;
	if (!parent) {
		for (auto i = ui->tree->topLevelItemCount() - 1; i >= 0; --i) {
			auto item = ui->tree->topLevelItem(i);
			auto filtered = filterItem(keyword, item);
			if (filtered || matchItem(item, keyword)) {
				item->setHidden(false);
				found = true;
			} else {
				item->setHidden(true);
			}
		}
	} else {
		for (auto i = 0, count = parent->childCount(); i < count; ++i) {
			auto child = parent->child(i);
			auto filtered = filterItem(keyword, child);
			if (filtered || matchItem(child, keyword)) {
				child->setHidden(false);
				found = true;
			} else {
				child->setHidden(true);
			}
		}
	}
	return found;
}
static QString elapsed_toString(double elapsed)
{
	auto s = QString::number(elapsed, 'f', 3);
	while (s.endsWith('0'))
		s.chop(1);
	if (s.endsWith('.'))
		s.chop(1);
	return s;
}
QString PLSPerformanceTool::elapsedToString(std::int64_t elapsed) const
{
	if (elapsed >= 1000000000)
		return elapsed_toString(elapsed / 1000000000.0) + QStringLiteral("s");
	else if (elapsed >= 1000000)
		return elapsed_toString(elapsed / 1000000.0) + QStringLiteral("ms");
	else if (elapsed >= 1000)
		return elapsed_toString(elapsed / 1000.0) + QStringLiteral("us");
	else
		return QString::number(elapsed) + QStringLiteral("ns");
}

std::int64_t PLSPerformanceTool::minimumTime(std::int64_t time) const
{
	switch (m_minimumTimeUnit) {
	case PLSTimeUnit::s:
		return time * 1000000000;
	case PLSTimeUnit::ms:
		return time * 1000000;
	case PLSTimeUnit::us:
		return time * 1000;
	case PLSTimeUnit::ns:
	default:
		return time;
	}
}
void PLSPerformanceTool::save(QByteArray &csv, pls::performance::PLSStatsDataPtr statsData)
{
	std::unique_lock lock(statsData->mutex);

	if (auto minElapsed = statsData->getMinElapsed(); minElapsed >= m_minimumTime && statsData->comment.compare("root")) {
		auto maxElapsed = qMax(statsData->getMaxElapsed(), 0);
		auto avgElapsed = qMax(statsData->getAverageElapsed(), 0);
		auto minNs = qMax(minElapsed, 0);
		csv.append(statsData->function);
		csv.append(',');
		csv.append(std::to_string(statsData->count));
		csv.append(',');
		csv.append(elapsedToString(minNs).toUtf8());
		csv.append(',');
		csv.append(elapsedToString(maxElapsed).toUtf8());
		csv.append(',');
		csv.append(elapsedToString(avgElapsed).toUtf8());
		csv.append(',');
		csv.append(QString::number(minNs / 1000000.0, 'f', 6).toUtf8());
		csv.append(',');
		csv.append(QString::number(maxElapsed / 1000000.0, 'f', 6).toUtf8());
		csv.append(',');
		csv.append(QString::number(avgElapsed / 1000000.0, 'f', 6).toUtf8());
		csv.append(',');
		csv.append(statsData->comment);
		csv.append('\n');
	}

	for (auto child : statsData->children)
		save(csv, child);
}
pls::performance::PLSStatsDataPtr PLSPerformanceTool::load(const QJsonObject &obj)
{
	auto comment = obj["comment"].toString();
	auto function = obj["function"].toString();
	auto file = obj["file"].toString();
	auto line = obj["line"].toInt();
	auto count = obj["count"].toInt();
	auto minElapsed = obj["minElapsed"].toInteger();
	auto maxElapsed = obj["maxElapsed"].toInteger();
	auto averageElapsed = obj["averageElapsed"].toInteger();

	auto data = std::make_shared<pls::performance::PLSStatsData>(comment.toStdString(), function.toStdString(), file.toStdString(), line);
	data->count = count;
	data->minElapsed = minElapsed;
	data->maxElapsed = maxElapsed;
	data->averageElapsed = averageElapsed;
	for (auto i : obj["children"].toArray()) {
		auto child = load(i.toObject());
		data->children.push_back(child);
		data->map[std::make_tuple(child->function, child->comment, child->file, child->line)] = child;
	}
	return data;
}
QJsonObject PLSPerformanceTool::save(pls::performance::PLSStatsDataPtr statsData)
{
	std::unique_lock lock(statsData->mutex);

	QJsonObject obj;
	obj["comment"] = QString::fromStdString(statsData->comment);
	obj["function"] = QString::fromStdString(statsData->function);
	obj["file"] = QString::fromStdString(statsData->file);
	obj["line"] = statsData->line;
	if (statsData->elapsed >= 0) {
		obj["count"] = statsData->count;
		obj["minElapsed"] = statsData->getMinElapsed();
		obj["maxElapsed"] = statsData->getMaxElapsed();
		obj["averageElapsed"] = statsData->getAverageElapsed();
	} else {
		obj["count"] = statsData->count + 1;
		auto elapsed = statsData->getElapsed();
		obj["minElapsed"] = qMin(elapsed, statsData->getMinElapsed());
		obj["maxElapsed"] = qMax(elapsed, statsData->getMaxElapsed());
		auto averageElapsed = statsData->getAverageElapsed();
		obj["averageElapsed"] = averageElapsed + (elapsed - averageElapsed) / (statsData->count + 1);
	}
	QJsonArray array;
	for (auto child : statsData->children)
		array.append(save(child));
	obj["children"] = array;
	return obj;
}
void PLSPerformanceTool::merge(pls::performance::PLSStatsDataPtr target, pls::performance::PLSStatsDataPtr source)
{
	target->mutex.lock();
	auto count = target->count;
	target->count += source->count;
	target->minElapsed = (target->minElapsed >= 0) ? qMin(target->minElapsed, source->minElapsed) : source->minElapsed;
	target->maxElapsed = (target->maxElapsed >= 0) ? qMax(target->maxElapsed, source->maxElapsed) : source->maxElapsed;
	target->averageElapsed = (target->averageElapsed >= 0) ? ((count * target->averageElapsed + source->count * source->averageElapsed) / target->count) : source->averageElapsed;
	target->mutex.unlock();
	for (auto child : source->children) {
		merge(pls::performance::PLSStatsData::statsData(target, child->function, child->file, child->line, child->comment), child);
	}
}
void PLSPerformanceTool::expandParent(QTreeWidgetItem *item, bool expand)
{
	if (auto parent = item->parent(); parent)
		expandParent(parent, expand);
	item->setExpanded(expand);
}
void PLSPerformanceTool::expandChild(QTreeWidgetItem *item, bool expand)
{
	for (int i = 0, count = item->childCount(); i < count; ++i)
		expandChild(item->child(i), expand);
	item->setExpanded(expand);
}
static void collectExpandedPaths(QTreeWidgetItem *item, const QString &path, QSet<QString> &out)
{
	if (item->isExpanded())
		out.insert(path);
	for (int i = 0, count = item->childCount(); i < count; ++i) {
		auto child = item->child(i);
		collectExpandedPaths(child, path + QLatin1Char('/') + child->text(0), out);
	}
}
static void collectExpandedPaths(QTreeWidget *tree, QSet<QString> &out)
{
	for (int i = 0, count = tree->topLevelItemCount(); i < count; ++i)
		collectExpandedPaths(tree->topLevelItem(i), tree->topLevelItem(i)->text(0), out);
}
static void restoreExpandedState(QTreeWidgetItem *item, const QString &path, const QSet<QString> &expandedPaths)
{
	item->setExpanded(expandedPaths.contains(path));
	for (int i = 0, count = item->childCount(); i < count; ++i) {
		auto child = item->child(i);
		restoreExpandedState(child, path + QLatin1Char('/') + child->text(0), expandedPaths);
	}
}
static void restoreExpandedState(QTreeWidget *tree, const QSet<QString> &expandedPaths)
{
	for (int i = 0, count = tree->topLevelItemCount(); i < count; ++i) {
		auto item = tree->topLevelItem(i);
		restoreExpandedState(item, item->text(0), expandedPaths);
	}
}
void PLSPerformanceTool::refresh()
{
	QSet<QString> expandedPaths;
	collectExpandedPaths(ui->tree, expandedPaths);

	ui->tree->clear();
	auto mtid = pls_current_thread_id();
	auto &[mutex1, map] = pls::performance::PLSStatsData::statsData();
	std::shared_lock lock1(mutex1);
	for (const auto &[tid, root] : map) {
		QTreeWidgetItem *item = nullptr;
		if (!tid) {
			item = new QTreeWidgetItem({QStringLiteral("Global")});
			ui->tree->insertTopLevelItem(0, item);
		} else if (tid == (uint32_t)-1) { // Action Stats
			std::unique_lock lock2(root->mutex);
			for (auto child : root->children) {
				add(child);
			}
		} else if (mtid == tid) {
			item = new QTreeWidgetItem({QStringLiteral("Main Thread: %2").arg(tid)});
			ui->tree->insertTopLevelItem(0, item);
		} else {
			item = new QTreeWidgetItem({QStringLiteral("Worker Thread: %2").arg(tid)});
			ui->tree->addTopLevelItem(item);
		}

		if (tid != (uint32_t)-1) {
			std::unique_lock lock2(root->mutex);
			for (auto child : root->children) {
				add(item, child);
			}
		}
	}
	if (!expandedPaths.isEmpty())
		restoreExpandedState(ui->tree, expandedPaths);
}
void PLSPerformanceTool::filter()
{
	filterItem(ui->searchKeyword->text());
}

void PLSPerformanceTool::on_browseSaveJsonFileButton_clicked()
{
	auto filename = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
	auto path = QFileDialog::getSaveFileName(this, tr("Save Performance Data"), "C:/" + filename + ".json", tr("JSON Files (*.json)"));
	ui->saveJsonFilePath->setText(path);
}
void PLSPerformanceTool::on_browseMergeJsonFileButton_clicked()
{
	auto path = QFileDialog::getOpenFileName(this, tr("Merge Performance Data"), "C:/", tr("JSON Files (*.json)"));
	ui->mergeJsonFilePath->setText(path);
}

void PLSPerformanceTool::on_saveButton_clicked()
{
	if (auto path = ui->saveJsonFilePath->text(); !path.isEmpty()) {
		QJsonObject obj;
		auto &[mutex1, map] = pls::performance::PLSStatsData::statsData();
		std::shared_lock lock1(mutex1);
		for (const auto &[tid, root] : map) {
			obj[QString::number(tid)] = save(root);
		}
		pls_write_json(path, obj);
	}
}
void PLSPerformanceTool::on_mergeButton_clicked()
{
	if (auto jsonFilePath = ui->mergeJsonFilePath->text(); jsonFilePath.isEmpty())
		return;
	else if (QJsonObject obj; !pls_read_json(obj, jsonFilePath))
		return;
	else {
		pls_async_invoke([obj]() {
			for (auto i = obj.begin(); i != obj.end(); ++i) {
				auto statsData = load(i.value().toObject());
				for (auto child : statsData->children) {
					if (auto target = pls::performance::PLSStatsData::findTopLevel(child->function, child->comment, child->file, child->line); target) {
						merge(target, child);
					}
				}
			}
		});
	}
}
void PLSPerformanceTool::on_refreshButton_clicked()
{
	refresh();
	filter();
}
void PLSPerformanceTool::on_closeButton_clicked()
{
	hide();
}
void PLSPerformanceTool::on_minimumTime_textChanged(const QString &text)
{
	m_minimumTime = minimumTime(text.toLongLong());
}
void PLSPerformanceTool::on_minimumTimeUnit_currentIndexChanged(int index)
{
	m_minimumTimeUnit = static_cast<PLSTimeUnit>(index);
	m_minimumTime = minimumTime(ui->minimumTime->text().toLongLong());
}
void PLSPerformanceTool::on_expandButton_clicked()
{
	if (auto item = ui->tree->currentItem(); item) {
		expandChild(item, true);
	} else {
		ui->tree->expandAll();
	}
}
void PLSPerformanceTool::on_collapseButton_clicked()
{
	if (auto item = ui->tree->currentItem(); item) {
		expandChild(item, false);
	} else {
		ui->tree->collapseAll();
	}
}
void PLSPerformanceTool::on_findPrevButton_clicked()
{
	if (auto keyword = ui->searchKeyword->text(); keyword.isEmpty()) {
		return;
	} else if (auto item = findPrevItem(keyword, ui->tree->currentItem()); item) {
		expandParent(item, true);
		ui->tree->setCurrentItem(item);
	}
}
void PLSPerformanceTool::on_findNextButton_clicked()
{
	if (auto keyword = ui->searchKeyword->text(); keyword.isEmpty()) {
		return;
	} else if (auto item = findNextItem(keyword, ui->tree->currentItem()); item) {
		expandParent(item, true);
		ui->tree->setCurrentItem(item);
	}
}
void PLSPerformanceTool::on_filterButton_clicked()
{
	filter();
}
void PLSPerformanceTool::on_browseSaveCsvFileButton_clicked()
{
	auto filename = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
	auto path = QFileDialog::getSaveFileName(this, tr("Save Performance Data"), "C:/" + filename + ".csv", tr("CSV Files (*.csv)"));
	ui->saveCsvFilePath->setText(path);
}
void PLSPerformanceTool::on_saveCsvButton_clicked()
{
	if (auto path = ui->saveCsvFilePath->text(); !path.isEmpty()) {
		QByteArray csv = "Name,Count,Min,Max,Average,Min(ms),Max(ms),Average(ms),Comment\n";
		auto &[mutex1, map] = pls::performance::PLSStatsData::statsData();
		std::shared_lock lock1(mutex1);
		for (const auto &[tid, root] : map) {
			save(csv, root);
		}
		pls_write_data(path, csv);
	}
}
void PLSPerformanceTool::on_clearButton_clicked()
{
	pls::performance::clear();
	on_refreshButton_clicked();
}
void PLSPerformanceTool::on_configCheckBox_stateChanged(int state)
{
	bool checked = state == (int)Qt::Checked;
	ui->configWidget->setVisible(checked);
}