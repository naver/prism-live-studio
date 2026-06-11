#ifndef PLSERRORCODETRANSFORMTOOL_H
#define PLSERRORCODETRANSFORMTOOL_H

#include "PLSToolView.hpp"

namespace Ui {
class PLSErrorCodeTransformTool;
}

class PLSErrorCodeTransformTool : public PLSToolView<PLSErrorCodeTransformTool> {
	Q_OBJECT

public:
	explicit PLSErrorCodeTransformTool(QWidget *parent = nullptr);
	~PLSErrorCodeTransformTool();
private slots:
	void startTransform();

private:
	Ui::PLSErrorCodeTransformTool *ui;
	QString m_defaultOutputPath{};
};

#endif // PLSERRORCODETRANSFORMTOOL_H
