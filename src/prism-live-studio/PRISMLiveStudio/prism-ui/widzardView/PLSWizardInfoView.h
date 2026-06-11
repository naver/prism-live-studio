#ifndef PLSWIZARDINFOVIEW_H
#define PLSWIZARDINFOVIEW_H

#include <QPushButton>

namespace Ui {
class PLSWizardInfoView;
}

class PLSWizardInfoView : public QPushButton {
	Q_OBJECT

public:
	enum class ViewType { Alert = 0, LiveInfo = 1, Blog = 2, Que = 3 };
	explicit PLSWizardInfoView(ViewType type, QWidget *parent = nullptr);
	~PLSWizardInfoView() override;

	void setInfo(const QString &title, const QString &timeStr = QString(), const QString &plaform = QString());

	void setInfoText(const QString &text);
	void setTimeText(const QString &text);
	void setTipText(const QString &text);

	void loading(bool start = true);
	void setLoaingImagePath(const QString &path) { mloadingPath = path; }

private:
	void setInfoView(ViewType type);

	Ui::PLSWizardInfoView *ui;
	ViewType m_type;
	QString mloadingPath;
	QTimer *loadingTimer = nullptr;
	friend class PLSLaunchWizardView;
};

#endif // PLSWIZARDINFOVIEW_H
