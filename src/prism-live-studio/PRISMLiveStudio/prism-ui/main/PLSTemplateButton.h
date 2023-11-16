#ifndef PLSTEMPLATEBUTTON_H
#define PLSTEMPLATEBUTTON_H

#include <memory>
#include <QPushButton>
#include <QMovie>

#include "frontend-api.h"

namespace Ui {
class PLSTemplateButton;
}

class PLSTemplateButton;
class QLabel;
class PLSTemplateButtonGroup : public QWidget, public pls::ITemplateListPropertyModel::IButtonGroup {
	Q_OBJECT

public:
	explicit PLSTemplateButtonGroup(QWidget *parent = nullptr);
	~PLSTemplateButtonGroup() override = default;

	PLSTemplateButton *findButton(int value);
	void addButton(PLSTemplateButton *button);
	void selectButton(PLSTemplateButton *button);

	void release() override { deleteLater(); }

	QWidget *widget() override;

	pls::ITemplateListPropertyModel::IButton *selectedButton() override;
	void selectButton(pls::ITemplateListPropertyModel::IButton *button) override;
	void selectButton(int value) override;
	void selectFirstButton() override;

	int buttonCount() const override;
	pls::ITemplateListPropertyModel::IButton *button(int index) override;

	pls::ITemplateListPropertyModel::IButton *fromValue(int value) override;

	void connectSlot(QObject *receiver, const std::function<void(pls::ITemplateListPropertyModel::IButton *selected, pls::ITemplateListPropertyModel::IButton *previous)> &slot) override;

signals:
	void selectedChanged(PLSTemplateButton *selected, PLSTemplateButton *previous);

private:
	QList<PLSTemplateButton *> m_buttons;
	PLSTemplateButton *m_selectedButton = nullptr;
};

class PLSTemplateButton : public QPushButton, public pls::ITemplateListPropertyModel::IButton {
	Q_OBJECT
	Q_PROPERTY(bool fullGif READ fullGif WRITE setFullGif)

public:
	struct UseForTextMotion {};

	explicit PLSTemplateButton(PLSTemplateButtonGroup *group);
	explicit PLSTemplateButton(QWidget *parent, UseForTextMotion useFor);
	~PLSTemplateButton() override;

	void attachGifResource(const QString &resourcePath, const QString &resourceBackupPath, const QString &resourceUrl, int value = -1);

	void setGroupName(const QString &groupName);
	QString getGroupName() const;

	void setTemplateText(const QString &text);

	void setCheckedState(bool checkedState);

	bool fullGif() const;
	void setFullGif(bool fullGif);

	void release() override { deleteLater(); }
	pls::ITemplateListPropertyModel::IButtonGroup *buttonGroup() override { return dynamic_cast<pls::ITemplateListPropertyModel::IButtonGroup *>(parentWidget()); }
	QPushButton *button() override { return this; }
	int value() const override { return m_value; }

	QString resourcePath() const { return m_resourcePath; }
	QString resourceBackupPath() const { return m_resourceBackupPath; }
	QString resourceUrl() const { return m_resourceUrl; }

private:
	void init();

	// QWidget interface
protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *event) override;

private:
	Ui::PLSTemplateButton *ui;
	QMovie m_movie;
	bool m_fullGif = false;
	int m_value;
	QString m_resourcePath;
	QString m_resourceBackupPath;
	QString m_resourceUrl;
	QString m_groupName;
	QLabel *m_borderLabel;

	friend class PLSTemplateButtonGroup;
};

#endif // PLSTEMPLATEBUTTON_H
