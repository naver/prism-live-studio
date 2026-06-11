#include "PLSLaboratoryItem.h"
#include "ui_PLSLaboratoryItem.h"
#include "libutils-api.h"

PLSLaboratoryItem::PLSLaboratoryItem(const QString &id, const QString &name, QWidget *parent) : QPushButton(parent), m_id(id), m_text(name)
{
	ui = pls_new<Ui::PLSLaboratoryItem>();
	ui->setupUi(this);
	ui->label_text->setText(name);
	connect(this, &QPushButton::toggled, this, &PLSLaboratoryItem::statusChanged);
	installEventFilter(this);
}

PLSLaboratoryItem::~PLSLaboratoryItem()
{
	pls_delete(ui);
}

void PLSLaboratoryItem::statusChanged(bool isChecked)
{
	ui->label_text->setProperty("select", isChecked);
	pls_flush_style(ui->label_text);
}

QString PLSLaboratoryItem::itemId() const
{
	return m_id;
}

QString PLSLaboratoryItem::itemName() const
{
	return m_text;
}

void PLSLaboratoryItem::setScrollArea(QScrollArea *scrollArea)
{
	m_scrollArea = scrollArea;
}

QScrollArea *PLSLaboratoryItem::getScrollArea()
{
	return m_scrollArea;
}

bool PLSLaboratoryItem::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:
		pls_flush_style(this, "hovered", true);
		break;
	case QEvent::HoverLeave:
		pls_flush_style(this, "hovered", false);
		break;
	default:
		break;
	}
	return QPushButton::eventFilter(watched, event);
}
