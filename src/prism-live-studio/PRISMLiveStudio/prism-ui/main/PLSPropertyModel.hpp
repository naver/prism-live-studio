#pragma once

#include <frontend-api.h>

class PLSTemplateListPropertyModel : public pls::ITemplateListPropertyModel {
protected:
	~PLSTemplateListPropertyModel() override = default;

	enum class State { Uninitialized, Initializing, Ok, Failed };
	struct Template {
		int value = 0;
		QString rcPath;
		QString rcBackupPath;
		QString rcUrl;
	};

public:
	bool isUninitialized() const { return m_state == State::Uninitialized; }
	bool isInitializing() const { return m_state == State::Initializing; }
	bool isInitialized() const { return isOk() || isFailed(); }
	bool isOk() const { return m_state == State::Ok; }
	bool isFailed() const { return m_state == State::Failed; }

	void getButtons(QObject *receiver, const std::function<void()> &waiting, const std::function<void(bool ok, IButtonGroup *group)> &result) override;

	IButtonGroup *group() const;
	void invokeResults();

	virtual void doInit() = 0;

	State m_state = State::Uninitialized;
	QList<Template> m_templates;
	QList<QPair<QObject *, std::function<void(bool ok, IButtonGroup *group)>>> m_results;
};

class PLSViewerCountTemplateListPropertyModel : public PLSTemplateListPropertyModel {
private:
	~PLSViewerCountTemplateListPropertyModel() override = default;

public:
	static PLSViewerCountTemplateListPropertyModel *instance();

	void release() override;

	void doInit() override;
};
