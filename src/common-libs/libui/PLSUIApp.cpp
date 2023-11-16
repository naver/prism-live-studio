#include "PLSUIApp.h"
#include "libui.h"

#include <qdir.h>
#include <qproxystyle.h>

constexpr QSize CHECKBOX_ICON_SIZE{15, 15};
constexpr QSize RADIOBUTTON_ICON_SIZE{18, 18};
constexpr QSize SWITCHBUTTON_ICON_SIZE{28, 17};
constexpr int FACTOR = 4;

static PLSUiApp *s_instance = nullptr;

class PLSStyle : public QProxyStyle {
public:
	using QProxyStyle::QProxyStyle;
	~PLSStyle() = default;
	virtual void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const override
	{
		if (element != QStyle::PE_FrameTabBarBase) {
			QProxyStyle::drawPrimitive(element, option, painter, widget);
		}
	}
	int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override
	{
		if (hint == SH_TabBar_Alignment) {
			return Qt::AlignLeft;
		}
		return QProxyStyle::styleHint(hint, option, widget, returnData);
	}
	int pixelMetric(QStyle::PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override
	{
		if (metric == PM_SubMenuOverlap) {
			return 0;
		}
		return QProxyStyle::pixelMetric(metric, option, widget);
	}
};
PLSUiApp::PLSUiApp(int &argc, char **argv) : pls::Application<QApplication>(argc, argv)
{
	s_instance = this;

	qApp->setEffectEnabled(Qt::UI_AnimateMenu, false);
	qApp->setEffectEnabled(Qt::UI_AnimateTooltip, false);
	qApp->setEffectEnabled(Qt::UI_FadeMenu, false);
	qApp->setEffectEnabled(Qt::UI_FadeTooltip, false);

	QString fontStyle;
#if defined(Q_OS_WIN)
	fontStyle = "* {font-family : \"Segoe UI\", \"MalgunGothic\", \"Malgun Gothic\", \"Dotum\", \"Gulim\";}";
#endif
	QDir::setCurrent(applicationDirPath());
	pls_add_global_css({"Common",       "QDialog",       "QCheckBox",    "QComboBox",          "QGroupBox",         "QLineEdit",  "QMenu",     "QPlainTextEdit", "QPushButton",
			    "QRadioButton", "QScrollBar",    "QSlider",      "QSpinBox",           "QTableView",        "QTabWidget", "QTextEdit", "QToolButton",    "QToolTip",
			    "CommonDialog", "PLSDialogView", "PLSAlertView", "PLSColorDialogView", "PLSFontDialogView", "PLSEdit"},
			   {fontStyle});
	setStyle(new PLSStyle());

	m_checkBoxIcons[CheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-normal.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedHover] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-over.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedPressed] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-clicked.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[CheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-chekedbox-disable.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-normal.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedHover] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-over.svg"), CHECKBOX_ICON_SIZE * FACTOR);
	m_checkBoxIcons[UncheckedPressed] = m_checkBoxIcons[UncheckedHover];
	m_checkBoxIcons[UncheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/txt-unchekedbox-disable.svg"), CHECKBOX_ICON_SIZE * FACTOR);

	m_radioButtonIcons[CheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-checked.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[CheckedHover] = m_radioButtonIcons[CheckedNormal];
	m_radioButtonIcons[CheckedPressed] = m_radioButtonIcons[CheckedNormal];
	m_radioButtonIcons[CheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-checked-disable.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[UncheckedNormal] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-unchecked.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);
	m_radioButtonIcons[UncheckedHover] = m_radioButtonIcons[UncheckedNormal];
	m_radioButtonIcons[UncheckedPressed] = m_radioButtonIcons[UncheckedNormal];
	m_radioButtonIcons[UncheckedDisabled] = pls_load_pixmap(QStringLiteral(":/libui/resource/images/radio-button-unchecked-disable.svg"), RADIOBUTTON_ICON_SIZE * FACTOR);

	m_switchButtonIcons[CheckedNormal] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedHover] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-over.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedPressed] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-click.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[CheckedDisabled] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-on-disable.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedNormal] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-default.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedHover] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-over.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedPressed] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-click.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
	m_switchButtonIcons[UncheckedDisabled] = pls_load_pixmap(":/libui/resource/images/checkbox/slider-off-disable.svg", SWITCHBUTTON_ICON_SIZE * FACTOR);
}

PLSUiApp::~PLSUiApp()
{
	s_instance = nullptr;
}

PLSUiApp *PLSUiApp::instance()
{
	return s_instance;
}
