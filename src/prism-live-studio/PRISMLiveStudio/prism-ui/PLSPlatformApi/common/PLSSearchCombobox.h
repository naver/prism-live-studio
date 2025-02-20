#pragma once

#include <QListWidget>
#include <vector>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QJsonObject>

class QShowEvent;

struct PLSSearchData {
	PLSSearchData() = default;
	PLSSearchData(const QJsonObject &obj)
	{
		id = obj["id"].toString();
		name = obj["name"].toString();
		url = obj["logoUrl"].toString();
	}
	QString id{};
	QString name{};
	QString url{};

	void resetData()
	{
		id = "";
		name = "";
		url = "";
	}
};
Q_DECLARE_METATYPE(PLSSearchData)

class PLSLiveInfoSearchListWidget : public QListWidget {
	Q_OBJECT
public:
	explicit PLSLiveInfoSearchListWidget(QWidget *parent);
	void showList(const std::vector<PLSSearchData> &datas, const QString &emptyShowMsg = {});
	int currentDataSize() const;

private:
	std::vector<PLSSearchData> m_datas;
};

class PLSSearchCombobox : public QLineEdit {
	Q_OBJECT

public:
	explicit PLSSearchCombobox(QWidget *parent = nullptr);
	~PLSSearchCombobox() override = default;

	void showListWidget(bool show, bool isClear = false);

	void setSelectData(const PLSSearchData &data);
	PLSSearchData getSelectData();

protected:
	void showEvent(QShowEvent *event) override;

private:
	QPushButton *pushButtonClear{nullptr};
	QPushButton *pushButtonSearch{nullptr};
	PLSLiveInfoSearchListWidget *m_listWidget{nullptr};

	QTimer *m_timer{nullptr};
	PLSSearchData m_selectData;
	bool m_isFirstSearch{true};
	void startSearch(bool immediately);
	void setupListUI();

signals:
	void startSearchText(const QWidget *receiver, const QString &keyword);
	void itemSelect(const PLSSearchData &data);

public slots:
	void receiveSearchData(const std::vector<PLSSearchData> &datas, const QString &keyword, const QString &emptyShowMsg = {});
	void onTextChanged(const QString &text);
};
