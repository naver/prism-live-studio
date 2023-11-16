#ifndef GUIDETIPSCONST_H
#define GUIDETIPSCONST_H
#include <qstring.h>

namespace guide_tip_space {
extern const QString g_sourceText;
extern const QString g_aliginWidgetName;
extern const QString g_refrenceWidget;
extern const QString g_otherListenedWidgets;
extern const QString g_displayVersion;
extern const QString g_watchType;
extern const QString g_tipsContent;
constexpr int g_borderWidth = 100;
extern const QString g_tipTextHtml;
#if defined(Q_OS_WIN)
#define RegisterJsonPath QCoreApplication::applicationDirPath() + QDir::separator() + "../../data/prism-studio" + QDir::separator() + "newInfo.json"
#elif defined(Q_OS_MACOS)
#define RegisterJsonPath pls_get_app_resource_dir() + "/data/prism-studio/newInfo.json"

#endif
#ifdef DEBUG
#define TEST_DEBUG(x) qDebug() << __FILE__ << "  " << __LINE__ << " : " << x;
#else
#define TEST_DEBUG(x)
#endif // DEBUG

}
#endif // GUIDETIPSCONST_H
