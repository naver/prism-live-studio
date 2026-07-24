#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class PLSLoadingView;
class QStackedLayout;

class OBSQTDisplayFailedView : public QWidget {
public:
	explicit OBSQTDisplayFailedView(QWidget *parent = nullptr);

	void setContent(const QString &text, bool needLoading);
	void refreshContentLayout();
	void hideContent();

private:
	enum class ContentState {
		Hidden,
		Loading,
		Text,
	};

	void applyState();

private:
	QStackedLayout *m_contentLayout = nullptr;
	QWidget *m_loadingPage = nullptr;
	QLabel *m_textLabel = nullptr;
	PLSLoadingView *m_loadingView = nullptr;
	QString m_text;
	ContentState m_state = ContentState::Hidden;
};
