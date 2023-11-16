#ifndef PLSCOMBOBOX_H
#define PLSCOMBOBOX_H

#include <QListView>
#include <QComboBox>
#include <QScrollBar>

#include "libui.h"

class LIBUI_API PLSComboBoxListView : public QListView {
	Q_OBJECT
	Q_PROPERTY(bool scrollBarShow READ scrollBarShow)

public:
	explicit PLSComboBoxListView(QWidget *parent = nullptr);

	bool scrollBarShow() const;

protected:
	bool eventFilter(QObject *i_Object, QEvent *i_Event) override;

private:
	void verticalScrollbarValueChanged(int value) override;
	void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
	void rowsInserted(const QModelIndex &parent, int start, int end) override;
	void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>()) override;
	QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &index, const QEvent *event = nullptr) const override;
};

class LIBUI_API PLSComboBox : public QComboBox {
	Q_OBJECT

public:
	explicit PLSComboBox(QWidget *parent = nullptr);
	~PLSComboBox() override = default;

signals:

	void popupShown(bool);

public:
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

	void showPopup() override;
	void hidePopup() override;

protected:
	void wheelEvent(QWheelEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	bool isAnimateCombo;
};

class LIBUI_API PLSEditableComboBox : public PLSComboBox {
	Q_OBJECT

public:
	explicit PLSEditableComboBox(QWidget *parent = nullptr);
	~PLSEditableComboBox() override = default;

	void showPopup() override;
};

#endif // PLSCOMBOBOX_H
