#ifndef PLSPERFORMANCETOOL_HPP
#define PLSPERFORMANCETOOL_HPP

#include "libui-globals.h"
#include "PLSToolView.hpp"
#include "pls-performance.h"
#include <qjsonarray.h>
#include <qjsonobject.h>

namespace Ui {
class PLSPerformanceTool;
}

enum class PLSTimeUnit { s, ms, us, ns };

class QTreeWidgetItem;

class LIBUI_API PLSPerformanceTool : public PLSToolView<PLSPerformanceTool> {
	Q_OBJECT

public:
	explicit PLSPerformanceTool(QWidget *parent = nullptr);
	~PLSPerformanceTool() override;

private:
	void add(const pls::performance::PLSStatsDataPtr statsData) const;
	void add(QTreeWidgetItem *parent, const pls::performance::PLSStatsDataPtr statsData) const;
	QTreeWidgetItem *createItem(pls::performance::PLSStatsDataPtr statsData) const;
	QTreeWidgetItem *findPrevItem(const QString &keyword, QTreeWidgetItem *current);
	QTreeWidgetItem *findNextItem(const QString &keyword, QTreeWidgetItem *current);
	bool filterItem(const QString &keyword, QTreeWidgetItem *parent = nullptr);
	QString elapsedToString(std::int64_t elapsed) const;
	std::int64_t minimumTime(std::int64_t time) const;
	void save(QByteArray &csv, pls::performance::PLSStatsDataPtr statsData);
	void refresh();
	void filter();

	static pls::performance::PLSStatsDataPtr load(const QJsonObject &obj);
	static QJsonObject save(pls::performance::PLSStatsDataPtr statsData);
	static void merge(pls::performance::PLSStatsDataPtr target, pls::performance::PLSStatsDataPtr source);
	static void expandParent(QTreeWidgetItem *item, bool expand);
	static void expandChild(QTreeWidgetItem *item, bool expand);

private slots:
	void on_browseSaveJsonFileButton_clicked();
	void on_browseMergeJsonFileButton_clicked();
	void on_saveButton_clicked();
	void on_mergeButton_clicked();
	void on_refreshButton_clicked();
	void on_closeButton_clicked();
	void on_minimumTime_textChanged(const QString &text);
	void on_minimumTimeUnit_currentIndexChanged(int index);
	void on_expandButton_clicked();
	void on_collapseButton_clicked();
	void on_findPrevButton_clicked();
	void on_findNextButton_clicked();
	void on_filterButton_clicked();
	void on_browseSaveCsvFileButton_clicked();
	void on_saveCsvButton_clicked();
	void on_clearButton_clicked();
	void on_configCheckBox_stateChanged(int state);

private:
	Ui::PLSPerformanceTool *ui;
	std::int64_t m_minimumTime = 0; // in ns
	PLSTimeUnit m_minimumTimeUnit = PLSTimeUnit::ns;
};

#endif // PLSPERFORMANCETOOL_HPP
