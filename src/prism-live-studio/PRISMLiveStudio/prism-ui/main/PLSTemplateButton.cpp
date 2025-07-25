#include "PLSTemplateButton.h"
#include "ui_PLSTemplateButton.h"
#include "frontend-api.h"

#include <libutils-api.h>
#include <libui.h>
#include <QFileInfo>
#include <QResizeEvent>

PLSTemplateButtonGroup::PLSTemplateButtonGroup(QWidget *parent) : QWidget(parent) {}

PLSTemplateButton *PLSTemplateButtonGroup::findButton(int value)
{
	for (auto btn : m_buttons) {
		if (btn->value() == value) {
			return btn;
		}
	}
	return nullptr;
}

void PLSTemplateButtonGroup::addButton(PLSTemplateButton *button)
{
	connect(button, &QPushButton::toggled, this, [button, this]() { selectButton(button); });
	m_buttons.append(button);
}

void PLSTemplateButtonGroup::selectButton(PLSTemplateButton *button)
{
	if (m_selectedButton == button || !m_buttons.contains(button)) {
		return;
	}

	auto previous = m_selectedButton;

	if (previous) {
		previous->setChecked(false);
		previous->setCheckedState(false);
		pls_flush_style_recursive(previous);
	}

	m_selectedButton = button;

	if (button) {
		button->setChecked(true);
		button->setCheckedState(true);
		pls_flush_style_recursive(button);
	}

	emit selectedChanged(button, previous);
}

QWidget *PLSTemplateButtonGroup::widget()
{
	return this;
}

pls::ITemplateListPropertyModel::IButton *PLSTemplateButtonGroup::selectedButton()
{
	return m_selectedButton;
}
void PLSTemplateButtonGroup::selectButton(pls::ITemplateListPropertyModel::IButton *button)
{
	if (auto btn = dynamic_cast<PLSTemplateButton *>(button); btn) {
		selectButton(btn);
	}
}
void PLSTemplateButtonGroup::selectButton(int value)
{
	if (auto btn = findButton(value); btn) {
		selectButton(btn);
	}
}
void PLSTemplateButtonGroup::selectFirstButton()
{
	if (!m_buttons.isEmpty()) {
		selectButton(m_buttons.first());
	}
}

int PLSTemplateButtonGroup::buttonCount() const
{
	return static_cast<int>(m_buttons.size());
}
pls::ITemplateListPropertyModel::IButton *PLSTemplateButtonGroup::button(int index)
{
	return m_buttons.at(index);
}

pls::ITemplateListPropertyModel::IButton *PLSTemplateButtonGroup::fromValue(int value)
{
	return findButton(value);
}

void PLSTemplateButtonGroup::connectSlot(QObject *receiver, const std::function<void(pls::ITemplateListPropertyModel::IButton *selected, pls::ITemplateListPropertyModel::IButton *previous)> &slot)
{
	connect(this, &PLSTemplateButtonGroup::selectedChanged, receiver, slot);
}

PLSTemplateButton::PLSTemplateButton(PLSTemplateButtonGroup *group) : QPushButton(group)
{
	init();
	setAutoExclusive(true);
	group->addButton(this);
	ui->templateGifLabel->installEventFilter(this);
}
PLSTemplateButton::PLSTemplateButton(QWidget *parent, UseForTextMotion /*useFor*/) : QPushButton(parent)
{
	init();
	ui->templateGifLabel->installEventFilter(this);
	connect(this, &QAbstractButton::toggled, [this](bool isChecked) { setCheckedState(isChecked); });
}
PLSTemplateButton::~PLSTemplateButton()
{
	pls_delete(ui);
}

void PLSTemplateButton::attachGifResource(const QString &resourcePath, int value)
{
	m_value = value;
	ui->templateGifLabel->setMovie(&m_movie);
	m_movie.setCacheMode(QMovie::CacheAll);
	m_movie.setFileName(resourcePath);
	if (resourcePath.toLower().endsWith(".gif")) {
		m_movie.setFormat("GIF");
	} else if (resourcePath.toLower().endsWith(".png")) {
		m_movie.setFormat("APNG");
	}
}

void PLSTemplateButton::setGroupName(const QString &groupName)
{
	m_groupName = groupName;
}

QString PLSTemplateButton::getGroupName() const
{
	return m_groupName;
}

void PLSTemplateButton::setTemplateText(const QString &text)
{
	ui->templateGifLabel->setText(text);
}
void PLSTemplateButton::setPaid(bool isPaid)
{
	m_isPaid = isPaid;
}

void PLSTemplateButton::setCheckedState(bool checkedState)
{
#if 0
	ui->templateGifLabel->setProperty("checked", checkedState);
	ui->container->setProperty("checked", checkedState);
#endif
	m_borderLabel->setProperty("checked", checkedState);
}

bool PLSTemplateButton::fullGif() const
{
	return m_fullGif;
}

void PLSTemplateButton::setFullGif(bool fullGif)
{
	m_fullGif = fullGif;
}

void PLSTemplateButton::init()
{
	ui = pls_new<Ui::PLSTemplateButton>();
	ui->setupUi(this);
	ui->container->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->templateGifLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	auto borderLayout = pls_new<QHBoxLayout>();
	m_borderLabel = pls_new<QLabel>();
	m_borderLabel->setObjectName("templateBorderlabel");
	m_borderLabel->setScaledContents(true);
	borderLayout->addWidget(m_borderLabel);
	borderLayout->setContentsMargins(0, 0, 0, 0);
	ui->templateGifLabel->setLayout(borderLayout);

	setCheckedState(false);
}

void PLSTemplateButton::showEvent(QShowEvent *event)
{
	if (m_movie.state() != QMovie::Running) {
		m_movie.start();
	}

	QPushButton::showEvent(event);
}

bool PLSTemplateButton::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->templateGifLabel) {
		if (event->type() == QEvent::Resize) {
			m_movie.setScaledSize(static_cast<QResizeEvent *>(event)->size());
		}
	}
	return QPushButton::eventFilter(watcher, event);
}
