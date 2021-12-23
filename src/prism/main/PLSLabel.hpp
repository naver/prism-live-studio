#ifndef PLSLABEL_H
#define PLSLABEL_H

#include <QLabel>

class PLSLabel : public QLabel {
	Q_OBJECT
public:
	explicit PLSLabel(QWidget *parent = nullptr, bool showTooltip = true);
	explicit PLSLabel(const QString &text, bool showTooltip = true, QWidget *parent = nullptr);
	~PLSLabel();
	void SetText(const QString &text);
	QString Text();

protected:
	virtual void resizeEvent(QResizeEvent *event) override;

private:
	QString GetNameElideString();

private:
	QString realText{};
	bool showTooltip = true;
};

class CombinedLabel : public QLabel {
	Q_OBJECT

	friend class PLSBgmItemView;

public:
	explicit CombinedLabel(QWidget *parent = nullptr);
	~CombinedLabel();

	void SetShowText(const QString &strText);
	void SetAdditionalText(const QString &strText);
	void SetSpacing(int spacing);
	void SetAdditionalVisible(bool visible);
	void SetMiddleVisible(bool visible);
public slots:
	void UpdataUi();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	int GetTextSpacing();

private:
	QLabel *additionalLabel = nullptr;
	QLabel *middleLabel = nullptr;
	int spacing = 5;
	QString strText;
	QString strAdditional;
};

#endif // PLSLABEL_H
