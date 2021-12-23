#pragma once
#include <map>
#include <vector>
#include <QString>
#include "obs.hpp"
#include "window-basic-main.hpp"
#include "PLSMotionDefine.h"
#include <QObject>
#include <qmetatype.h>
#include <QStringList>

using namespace std;
struct PLSVirtualData;

extern const char *s_virtualBackground;

using VirtualConfigVec = std::vector<std::pair<void *, PLSVirtualData>>;

enum class PLSVrBgModel { NoSelect = 0, Original, BgDelete, OtherResource };

Q_DECLARE_METATYPE(PLSVrBgModel)

struct PLSVirtualData {
	bool isTempOriginModel = false;

	bool isValid = false;

	bool isMotionEnable = true;
	bool isMotionUIChecked = false;

	bool isImageEditMode = false;

	int resrcWidth = 0;
	int resrcHeight = 0;
	int srcWidth = 0;
	int srcHeight = 0;

	double zoomMin = 0;
	double zoomMax = 0;
	double zoomValue = 0;

	bool isFlipHori = false;
	bool isFlipVerti = false;

	int blurValue = 0;

	bool isChromaKey = false;

	PLSVrBgModel bgModelType = PLSVrBgModel::NoSelect;
	MotionType thidrSourceType = MotionType::UNKOWN;
	QString motionItemID{};
	bool isPrismResource = false;

	double srcRectX = 0.0;
	double srcRectY = 0.0;
	double srcRectCX = 1.0;
	double srcRectCY = 1.0;
	double dstRectX = 0.0;
	double dstRectY = 0.0;
	double dstRectCX = 1.0;
	double dstRectCY = 1.0;

	QString originPath;
	QString stopPath;
	QString thumbnailFilePath;

	QString foregroundPath;
	QString foregroundStaticPath;

	QSize enterResolutionSize{};
	QSize currentResolutionSize{};
	bool haveShownChromakeyTip = false;
};

class PLSVirtualBgManager {
public:
	static PLSVirtualBgManager *instance();
	explicit PLSVirtualBgManager();
	~PLSVirtualBgManager();
	VirtualConfigVec getVecDatas() { return m_vecDatas; };
	void updateSourceListDatas(const DShowSourceVecType &list);

	PLSVirtualData &getVirtualData(OBSSceneItem sceneData = nullptr);
	PLSVirtualData &getVirtualDataWithErrCode(bool *isContain, OBSSceneItem sceneData = nullptr);

	const OBSSceneItem getCurrentSceneItem();
	const OBSSource getCurrentSource();

	void setCurrentSceneItem(OBSSceneItem item);

	void resetImageEditData(OBSSceneItem item = nullptr, bool needSendToCore = true);

	void updateDataToCore(OBSSceneItem sceneData = nullptr, bool isSendData = false, PLSVirtualData sendData = PLSVirtualData());
	void updateDataToCore(const OBSSource source, const PLSVirtualData &vrData);
	void updateDataFromCore(OBSSceneItem sceneData);

	bool isNoSourcePage();

	void clearVecData();
	void clearData();

	static bool isDShowSourceAvailable(OBSSceneItem item);
	bool isCurrentSourceExisted(const OBSSceneItem item);

	void enterImageEditMode();
	void leaveImageEditMode(bool isSaved = false);
	void leaveNotSavedImageEditMode(const OBSSceneItem item);

	static void sourceIsDeleted(const QString &itemId, const QStringList &list = {});
	static void checkResourceInvalid(const QStringList &itemIdList, const QString &sourceName);
	static void checkResourceIsUsed(const QString &itemId, bool &vrBgContain, bool &bgSourceContain);

	void checkOtherSourceRunderProcessExit(void *source);

	//property dialog resolution changed method.
	void vrSourcePropertyDialogOpen(const OBSSource source, const QSize &originResolution);
	void vrSourceResolutionChanged(const OBSSource source, const QSize &currentResolution, const QSize &oldResolution);
	void vrSourcePropertyDialogButtonClick(const OBSSource source, bool isOkClick /*ok or cancel*/, const QSize &originalResolution, const QSize &currentResolution);
	void vrCurrentModelChanged(const OBSSource source);

private:
	bool isContainItem(const OBSSceneItem item);
	PLSVirtualData getVrDataByItem(OBSSceneItem sceneData);
	PLSVirtualData getVrDataByItem(OBSSource source);

	static bool isDShowSourceValid(OBSSceneItem item);
	static bool isDShowSourceVisible(OBSSceneItem item);
	static void calcImageImageReset(PLSVirtualData &data);
	static void copyImageEditData(PLSVirtualData &fromData, PLSVirtualData &toData);
	void updateResolutionDataWhenApplyImageEdit(const OBSSource source, PLSVirtualData sendData);

private:
	VirtualConfigVec m_vecDatas;
	PLSVirtualData m_tempData;
	std::pair<void * /*sceneitem*/, PLSVirtualData> m_enterimageEditData{};
	std::pair<void * /*source*/, PLSVirtualData> m_enterResolutionData{};

	void *m_currentSceneItem = nullptr;
};

#define PLS_VIRTUAL_BG_MANAGER PLSVirtualBgManager::instance()
