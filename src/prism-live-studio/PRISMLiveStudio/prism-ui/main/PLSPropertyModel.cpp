#include "PLSPropertyModel.hpp"
#include "PLSTemplateButton.h"
#include "libutils-api.h"

#include <QApplication>
#include <QPushButton>
#include <QGridLayout>
#include <QTimer>

template<typename Fn, typename... Args> static void invokeFn(const QObject *receiver, const Fn &fn, Args &&...args)
{
	if (pls_object_is_valid(receiver) && fn) {
		fn(std::forward<Args>(args)...);
	}
}

void PLSTemplateListPropertyModel::getButtons(QObject *receiver, const std::function<void()> &waiting, const std::function<void(bool ok, IButtonGroup *group)> &result)
{
	if (isInitialized()) {
		invokeFn(receiver, result, isOk(), group());
		return;
	} else if (isInitializing()) {
		m_results.append({receiver, result});
		invokeFn(receiver, waiting);
		return;
	}

	m_state = State::Initializing;

	m_results.append({receiver, result});
	invokeFn(receiver, waiting);

	doInit();
}

pls::ITemplateListPropertyModel::IButtonGroup *PLSTemplateListPropertyModel::group() const
{
	if (!isOk() || m_templates.isEmpty()) {
		return nullptr;
	}

	auto group = pls_new<PLSTemplateButtonGroup>();
	QGridLayout *gridLayout = pls_new<QGridLayout>(group);
	gridLayout->setContentsMargins(0, 0, 0, 0);
	gridLayout->setSpacing(12);
	gridLayout->setAlignment(Qt::AlignLeft);
	for (qsizetype i = 0, count = m_templates.size(); i < count; ++i) {
		const auto &tpl = m_templates[i];
		PLSTemplateButton *button = pls_new<PLSTemplateButton>(group);
		button->setFullGif(true);
		button->attachGifResource(tpl.rcPath, tpl.value);
		gridLayout->addWidget(button, (int)i / 4, i % 4);
	}
	return group;
}

void PLSTemplateListPropertyModel::invokeResults()
{
	if (isInitialized()) {
		for (const auto &result : m_results) {
			invokeFn(result.first, result.second, isOk(), group());
		}
		m_results.clear();
	}
}

PLSViewerCountTemplateListPropertyModel *PLSViewerCountTemplateListPropertyModel::instance()
{
	static PLSViewerCountTemplateListPropertyModel s_instance;
	return &s_instance;
}

void PLSViewerCountTemplateListPropertyModel::release() {}

void PLSViewerCountTemplateListPropertyModel::doInit()
{
	m_templates.append(Template{0, ":/viewer-count/resource/images/template-01.png", "", ""});
	m_templates.append(Template{1, ":/viewer-count/resource/images/template-02.png", "", ""});
	m_templates.append(Template{2, ":/viewer-count/resource/images/template-03.png", "", ""});
	m_templates.append(Template{3, ":/viewer-count/resource/images/template-04.png", "", ""});
	m_templates.append(Template{4, ":/viewer-count/resource/images/template-05.png", "", ""});
	m_templates.append(Template{5, ":/viewer-count/resource/images/template-06.png", "", ""});

	m_state = State::Ok;

	invokeResults();
}
