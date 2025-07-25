#include "PLSSceneDataMgr.h"
#include <array>

#include "window-basic-main.hpp"
#include "libutils-api.h"
#include "PLSDrawPen/PLSDrawPenMgr.h"
#include <QDebug>

PLSSceneDataMgr *PLSSceneDataMgr::Instance()
{
	static PLSSceneDataMgr mgr;
	return &mgr;
}

void PLSSceneDataMgr::AddSceneData(const QString &name, PLSSceneItemView *view)
{
	PLSDrawPenMgr::Instance()->AddSceneData(name);
	auto iter = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iter != sceneDisplay.end()) {
		SceneDisplayVector &vec = iter->second;
		for (auto iter2 = vec.begin(); iter2 != vec.end(); ++iter2) {
			if (iter2->first == name) {
				iter2->second = view;
				return;
			}
		}
		vec.emplace_back(SceneDisplayVector::value_type(name, view));
	} else {
		SceneDisplayVector vec;
		vec.emplace_back(SceneDisplayVector::value_type(name, view));
		sceneDisplay[GetCurrentSceneCollectionName()] = vec;
	}
}

void PLSSceneDataMgr::RenameSceneData(const QString &preName, const QString &nextName)
{
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector &vec = iterMap->second;
		auto iter = vec.begin();
		for (; iter != vec.end(); ++iter) {
			if (0 == strcmp(iter->first.toStdString().c_str(), preName.toStdString().c_str())) {
				iter->first = nextName;
				break;
			}
		}
	}
	PLSDrawPenMgr::Instance()->RenameSceneData(preName, nextName);
}

void PLSSceneDataMgr::CopySrcToDest(const QString &srcName, const QString &destName)
{
	auto iterMap = sceneDisplay.find(srcName);
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector vec = iterMap->second;
		sceneDisplay[destName] = vec;
	}
}

void PLSSceneDataMgr::MoveSrcToDest(const QString &srcName, const QString &destName)
{
	auto iterMap = sceneDisplay.find(srcName);
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector vec = iterMap->second;
		sceneDisplay.erase(iterMap);
		sceneDisplay[destName] = vec;
	}
}

QString PLSSceneDataMgr::GetFirstSceneName()
{
	for (auto iter = sceneDisplay.begin(); iter != sceneDisplay.end(); ++iter) {
		SceneDisplayVector &vec = iter->second;
		for (auto iter2 = vec.begin(); iter2 != vec.end();) {
			const PLSSceneItemView *view = iter2->second;
			if (view) {
				return view->GetName();
			}
		}
	}
	return QString();
}

PLSSceneItemView *PLSSceneDataMgr::FindSceneData(const QString &name)
{
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector &vec = iterMap->second;
		auto iter = vec.begin();
		for (; iter != vec.end(); ++iter) {
			if (0 == strcmp(iter->first.toStdString().c_str(), name.toStdString().c_str())) {
				return iter->second;
			}
		}
	}

	return nullptr;
}

PLSSceneItemView *PLSSceneDataMgr::DeleteSceneData(const QString &name)
{
	PLSDrawPenMgr::Instance()->DeleteSceneData(name);

	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap == sceneDisplay.end()) {
		return nullptr;
	}

	SceneDisplayVector &vec = iterMap->second;

	auto iter = std::find_if(vec.begin(), vec.end(), [&name](const auto &item) { return item.first == name; });

	if (iter != vec.end()) {
		if (auto view = iter->second; nullptr != view) {
			view->SetRenderFlag(false);
			view->deleteLater();
			view = nullptr;
		}
		iter = vec.erase(iter);
		if (iter != vec.end()) {
			return iter->second;
		} else if (!vec.empty()) {
			return prev(iter)->second;
		}
	}

	return nullptr;
}

void PLSSceneDataMgr::DeleteAllData()
{
	PLSDrawPenMgr::Instance()->DeleteAllData();

	for (auto iter = sceneDisplay.begin(); iter != sceneDisplay.end(); ++iter) {
		SceneDisplayVector &vec = iter->second;
		for (auto iter2 = vec.begin(); iter2 != vec.end();) {
			PLSSceneItemView *view = iter2->second;
			if (view) {
				view->SetRenderFlag(false);
				view->deleteLater();
				view = nullptr;
				iter2 = vec.erase(iter2);
			} else {
				++iter2;
			}
		}
		vec.clear();
	}
	sceneDisplay.clear();
}

void PLSSceneDataMgr::SetDisplayVector(const SceneDisplayVector &dVec, QString file)
{
	if (file.isEmpty()) {
		file = GetCurrentSceneCollectionName();
	}
	auto iter = sceneDisplay.find(file);
	if (iter != sceneDisplay.end()) {
		iter->second = dVec;
	} else {
		sceneDisplay[GetCurrentSceneCollectionName()] = dVec;
	}
}

void PLSSceneDataMgr::SwapData(const int &romoveRow, const int &removeCol, const int &appendRow, const int &appendCol, const int &columnCount)
{
	if (removeCol == appendCol && romoveRow == appendRow) {
		return;
	}

	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap == sceneDisplay.end()) {
		return;
	}

	SceneDisplayVector &vec = iterMap->second;
	auto iter = vec.begin() + romoveRow * columnCount + removeCol;
	if (iter == vec.end()) {
		return;
	}

	PLSSceneItemView *view = iter->second;
	vec.erase(iter);
	if (!view) {
		return;
	}

	SwapDataToVec(romoveRow, removeCol, appendRow, appendCol, columnCount, view, vec);
}

void PLSSceneDataMgr::SwapDataInListMode(const int &romoveRow, const int &appendRow)
{
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap == sceneDisplay.end()) {
		return;
	}

	SceneDisplayVector &vec = iterMap->second;
	if (romoveRow == appendRow || appendRow < 0 || romoveRow < 0 || romoveRow >= vec.size() || appendRow >= vec.size()) {
		return;
	}

	auto iters = vec.begin();
	iters += romoveRow;

	PLSSceneItemView *view = iters->second;
	if (!view) {
		return;
	}
	vec.erase(iters);

	auto iter = vec.begin() + appendRow;
	vec.emplace(iter, SceneDisplayVector::value_type(view->GetName(), view));
}

void PLSSceneDataMgr::SwapToUp(const QString &name)
{
	PLSSceneItemView *view = nullptr;
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap == sceneDisplay.end()) {
		return;
	}

	SceneDisplayVector &vec = iterMap->second;
	auto condition = [name](std::pair<QString, PLSSceneItemView *> item) { return item.first == name; };
	auto result = std::find_if(std::begin(vec), std::end(vec), condition);
	if (result != std::end(vec) && result != std::begin(vec)) {
		view = result->second;
		vec.emplace(vec.erase(result) - 1, SceneDisplayVector::value_type(name, view));
	}
}

void PLSSceneDataMgr::SwapToDown(const QString &name)
{
	PLSSceneItemView *view = nullptr;
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap == sceneDisplay.end()) {
		return;
	}

	SceneDisplayVector &vec = iterMap->second;
	auto condition = [name](std::pair<QString, PLSSceneItemView *> item) { return item.first == name; };
	auto result = std::find_if(std::begin(vec), std::end(vec), condition);
	if (result != std::end(vec) && result != std::end(vec) - 1) {
		view = result->second;
		vec.emplace(vec.erase(result) + 1, SceneDisplayVector::value_type(name, view));
	}
}

void PLSSceneDataMgr::SwapToBottom(const QString &name)
{
	PLSSceneItemView *view = nullptr;
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector &vec = iterMap->second;
		auto condition = [name](std::pair<QString, PLSSceneItemView *> item) { return item.first == name; };
		auto result = std::find_if(std::begin(vec), std::end(vec), condition);
		if (result != std::end(vec)) {
			view = result->second;
			if (vec.erase(result) != std::end(vec)) {
				vec.emplace_back(SceneDisplayVector::value_type(name, view));
			}
		}
	}
}

void PLSSceneDataMgr::SwapToTop(const QString &name)
{
	PLSSceneItemView *view = nullptr;
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector &vec = iterMap->second;
		auto condition = [name](std::pair<QString, PLSSceneItemView *> item) { return item.first == name; };
		auto result = std::find_if(std::begin(vec), std::end(vec), condition);
		if (result != std::end(vec)) {
			view = result->second;
			vec.erase(result);
			vec.emplace(vec.begin(), SceneDisplayVector::value_type(name, view));
		}
	}
}

void PLSSceneDataMgr::SwapBackToIndex(int destIndex)
{
	auto iterMap = sceneDisplay.find(GetCurrentSceneCollectionName());
	if (iterMap != sceneDisplay.end()) {
		SceneDisplayVector &vec = iterMap->second;
		if (destIndex < 0 || destIndex >= vec.size()) {
			return;
		}

		auto back = vec.back();
		vec.emplace(vec.begin() + destIndex, back);
		vec.pop_back();
	}
}

int PLSSceneDataMgr::GetSceneSize(QString file)
{
	if (file.isEmpty()) {
		file = GetCurrentSceneCollectionName();
	}
	auto iterMap = sceneDisplay.find(file);
	if (iterMap != sceneDisplay.end()) {
		const SceneDisplayVector &vec = iterMap->second;
		return static_cast<int>(vec.size());
	}

	return 0;
}

SceneDisplayVector PLSSceneDataMgr::GetDisplayVector(QString file)
{
	if (file.isEmpty()) {
		file = GetCurrentSceneCollectionName();
	}

	auto iterMap = sceneDisplay.find(file);
	if (iterMap != sceneDisplay.end()) {
		return iterMap->second;
	}

	return SceneDisplayVector();
}

SceneDisplayMap PLSSceneDataMgr::GetAllData() const
{
	return sceneDisplay;
}

QStringList PLSSceneDataMgr::GetAllSceneName()
{
	QStringList list{};
	SceneDisplayVector vec = GetDisplayVector();
	for (auto iter = vec.begin(); iter != vec.end(); ++iter) {
		list << iter->first;
	}
	return list;
}

QString PLSSceneDataMgr::GetCurrentSceneCollectionName() const
{
	return QString(config_get_string(App()->GetUserConfig(), "Basic", "SceneCollectionFile"));
}

QString PLSSceneDataMgr::GetCurrentSceneCollectionAbsName() const
{
	std::array<char, 512> path;
	if (GetAppConfigPath(path.data(), path.size(), "PRISMLiveStudio/basic/scenes") <= 0)
		return "";

	return QString(path.data()).append("/").append(GetCurrentSceneCollectionName());
}

void PLSSceneDataMgr::SwapDataToVec(const int &romoveRow, const int &removeCol, const int &appendRow, const int &appendCol, const int &columnCount, PLSSceneItemView *view,
				    SceneDisplayVector &vec) const
{
	if (!view) {
		return;
	}

	if (appendRow * columnCount + appendCol >= vec.size() + 1) { // append last
		vec.emplace_back(SceneDisplayVector::value_type(view->GetName(), view));
		return;
	}
	if (appendRow * columnCount + appendCol == 0) { // append first
		vec.emplace(vec.begin(), SceneDisplayVector::value_type(view->GetName(), view));
		return;
	}

	if (removeCol + romoveRow > appendCol + appendRow) { // append middle
		if (romoveRow < appendRow) {
			vec.emplace(vec.begin() + appendRow * columnCount + appendCol - 1, SceneDisplayVector::value_type(view->GetName(), view));
		} else {
			vec.emplace(vec.begin() + appendRow * columnCount + appendCol, SceneDisplayVector::value_type(view->GetName(), view));
		}
	} else {
		if (romoveRow > appendRow) {
			vec.emplace(vec.begin() + appendRow * columnCount + appendCol, SceneDisplayVector::value_type(view->GetName(), view));
		} else {
			vec.emplace(vec.begin() + appendRow * columnCount + appendCol - 1, SceneDisplayVector::value_type(view->GetName(), view));
		}
	}
}
