#include "combobox.hpp"

#include <QWheelEvent>
#include <QApplication>
#include <QLineEdit>

PLSComboBoxListView::PLSComboBoxListView(QWidget *parent) : QListView(parent)
{
	this->installEventFilter(this);
}
PLSComboBoxListView::~PLSComboBoxListView() {}

QItemSelectionModel::SelectionFlags PLSComboBoxListView::selectionCommand(const QModelIndex &index, const QEvent *event) const
{
	Q_UNUSED(index)
	Q_UNUSED(event)

	return QItemSelectionModel::SelectionFlag::NoUpdate;
}

bool PLSComboBoxListView::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (i_Object == this && i_Event->type() == QEvent::KeyPress) {
		if (i_Object == this && i_Event->type() == QEvent::KeyPress) {
			QKeyEvent *key = static_cast<QKeyEvent *>(i_Event);
			if ((key->key() != Qt::Key_Enter) && (key->key() != Qt::Key_Return)) {
				//disable up down and other key.
				return true;
			}
		}
	}
	return QWidget::eventFilter(i_Object, i_Event);
}

PLSComboBox::PLSComboBox(QWidget *parent) : QComboBox(parent)
{
	isAnimateCombo = qApp->isEffectEnabled(Qt::UI_AnimateCombo);

	setView(new PLSComboBoxListView());
	view()->window()->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
	setMaxVisibleItems(5);
}
PLSComboBox ::~PLSComboBox() {}

void PLSComboBox::showPopup()
{
	qApp->setEffectEnabled(Qt::UI_AnimateCombo, false);
	QComboBox::showPopup();
}

void PLSComboBox::hidePopup()
{
	QComboBox::hidePopup();
	qApp->setEffectEnabled(Qt::UI_AnimateCombo, isAnimateCombo);
}

void PLSComboBox::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}

PLSEditableComboBox::PLSEditableComboBox(QWidget *parent) : PLSComboBox(parent) {}

PLSEditableComboBox::~PLSEditableComboBox() {}

void PLSEditableComboBox::showPopup()
{
	if (QLineEdit *lineEdit = this->lineEdit(); isEditable() && lineEdit) {
		QString lineEditText = lineEdit->text();
		QSignalBlocker comboxSignalBlocker(this);
		QSignalBlocker lineEditSignalBlocker(lineEdit);
		if (int index = findText(lineEditText); index >= 0) {
			setCurrentIndex(index);
		} else {
			setCurrentIndex(-1);
			lineEdit->setText(lineEditText);
		}
	}

	PLSComboBox::showPopup();
}
