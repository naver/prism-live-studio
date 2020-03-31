#pragma once

#include <QListView>
#include <QComboBox>

#include "frontend-api-global.h"

class FRONTEND_API PLSComboBoxListView : public QListView {
	Q_OBJECT

public:
	explicit PLSComboBoxListView(QWidget *parent = nullptr);
	~PLSComboBoxListView();

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event);

private:
	QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &index, const QEvent *event = nullptr) const;
};

class FRONTEND_API PLSComboBox : public QComboBox {
	Q_OBJECT

public:
	explicit PLSComboBox(QWidget *parent = nullptr);
	~PLSComboBox();

public:
	virtual void showPopup();
	virtual void hidePopup();

protected:
	void wheelEvent(QWheelEvent *event);

private:
	bool isAnimateCombo;
};

class FRONTEND_API PLSEditableComboBox : public PLSComboBox {
	Q_OBJECT

public:
	explicit PLSEditableComboBox(QWidget *parent = nullptr);
	~PLSEditableComboBox();

public:
	virtual void showPopup();
};
