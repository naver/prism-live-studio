project(PRISMLiveStudio)

add_subdirectory(frontend-api)
add_subdirectory(plugins)
add_subdirectory(vst-check)
if(OS_WINDOWS)
add_subdirectory(region-capture)
endif()

if(OS_MACOS)
  include(prism_helper)
endif()

set(OAUTH_BASE_URL
  "https://auth.obsproject.com/"
  CACHE STRING "Default OAuth base URL")

mark_as_advanced(OAUTH_BASE_URL)

if(NOT DEFINED TWITCH_CLIENTID
  OR "${TWITCH_CLIENTID}" STREQUAL ""
  OR NOT DEFINED TWITCH_HASH
  OR "${TWITCH_HASH}" STREQUAL ""
  OR NOT TARGET OBS::browser-panels)
  set(TWITCH_ENABLED OFF)
  set(TWITCH_CLIENTID "")
  set(TWITCH_HASH "0")
else()
  set(TWITCH_ENABLED ON)
endif()

if(NOT DEFINED RESTREAM_CLIENTID
  OR "${RESTREAM_CLIENTID}" STREQUAL ""
  OR NOT DEFINED RESTREAM_HASH
  OR "${RESTREAM_HASH}" STREQUAL ""
  OR NOT TARGET OBS::browser-panels)
  set(RESTREAM_ENABLED OFF)
  set(RESTREAM_CLIENTID "")
  set(RESTREAM_HASH "0")
else()
  set(RESTREAM_ENABLED ON)
endif()

if(NOT DEFINED YOUTUBE_CLIENTID
  OR "${YOUTUBE_CLIENTID}" STREQUAL ""
  OR NOT DEFINED YOUTUBE_SECRET
  OR "${YOUTUBE_SECRET}" STREQUAL ""
  OR NOT DEFINED YOUTUBE_CLIENTID_HASH
  OR "${YOUTUBE_CLIENTID_HASH}" STREQUAL ""
  OR NOT DEFINED YOUTUBE_SECRET_HASH
  OR "${YOUTUBE_SECRET_HASH}" STREQUAL "")
  set(YOUTUBE_ENABLED OFF)
else()
  set(YOUTUBE_ENABLED ON)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/obs-ui/ui-config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/ui-config.h)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil avformat)
find_package(CURL REQUIRED)

# add_subdirectory(frontend-plugins)
add_executable(PRISMLiveStudio)

add_subdirectory(prism-ui/channels)
add_subdirectory(prism-ui/PLSPlatformApi)
add_subdirectory(prism-ui/widzardView)
add_subdirectory(prism-ui/ResolutionTips)
add_subdirectory(prism-ui/GuideTips)
add_subdirectory(prism-ui/laboratory)
add_subdirectory(prism-ui/prism-login)

find_package(Qt6 COMPONENTS Widgets Network Svg Xml Gui NetworkAuth WebSockets)

target_link_libraries(PRISMLiveStudio PRIVATE Qt6::Widgets Qt6::Svg Qt6::Xml Qt6::Network Qt6::WebSockets)

set_target_properties(
  PRISMLiveStudio
  PROPERTIES AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
  AUTOUIC_SEARCH_PATHS "obs-ui/forms;obs-ui/forms/source-toolbar;prism-ui/forms")

target_include_directories(
  PRISMLiveStudio
  PRIVATE "$ENV{OBS_SRC_DIR}/UI/obs-frontend-api"
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/obs-ui
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/common
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/scene
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/scene-collection
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/music
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/virtual-template
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/main
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/ChannelsDataApi
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/ShareView
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/ChannelsSetting
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/Chat
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/ChannelsDataApi
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/channels/LiveEnd-Summary
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/naver-shopping-live
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/facebook
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/afreecatv
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/band
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/common
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/navertv
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/prism
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/rtmp
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/twitch
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/twitter
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/vlive
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi/youtube
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/widzardView
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/GuideTips
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/ResolutionTips
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/giphy
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/laboratory
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/prism-login
  ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/prism-login/ui
)

target_sources(PRISMLiveStudio PRIVATE obs-ui/forms/obs.qrc
  prism-ui/prism.qrc
  prism-ui/prism-css.qrc
  prism-ui/prism-images-1.qrc
  prism-ui/audio-mixer.qrc
  prism-ui/prism-images-2.qrc
  prism-ui/giphy-sticker.qrc
  prism-ui/prism-channels-resources.qrc
  prism-ui/defaultSources.qrc
  prism-ui/prism-images-3.qrc
  prism-ui/prism-images-chat.qrc
  prism-ui/prism-login.qrc
  prism-ui/filter-ui.qrc
  prism-ui/obs-dark.qrc
)

target_sources(
  PRISMLiveStudio
  PRIVATE obs-ui/forms/AutoConfigFinishPage.ui
  obs-ui/forms/AutoConfigStartPage.ui
  obs-ui/forms/AutoConfigStartPage.ui
  obs-ui/forms/AutoConfigStreamPage.ui
  obs-ui/forms/AutoConfigTestPage.ui
  obs-ui/forms/AutoConfigVideoPage.ui
  obs-ui/forms/ColorSelect.ui
  obs-ui/forms/OBSAbout.ui
  obs-ui/forms/OBSAdvAudio.ui
  obs-ui/forms/OBSBasic.ui
  obs-ui/forms/OBSBasicFilters.ui
  obs-ui/forms/OBSBasicInteraction.ui
  obs-ui/forms/OBSBasicSettings.ui
  obs-ui/forms/OBSBasicSourceSelect.ui
  obs-ui/forms/OBSBasicTransform.ui
  obs-ui/forms/OBSBasicVCamConfig.ui
  obs-ui/forms/OBSExtraBrowsers.ui
  obs-ui/forms/OBSImporter.ui
  obs-ui/forms/OBSLogReply.ui
  obs-ui/forms/OBSMissingFiles.ui
  obs-ui/forms/OBSRemux.ui
  obs-ui/forms/OBSUpdate.ui
  obs-ui/forms/OBSYoutubeActions.ui
  obs-ui/forms/source-toolbar/browser-source-toolbar.ui
  obs-ui/forms/source-toolbar/color-source-toolbar.ui
  obs-ui/forms/source-toolbar/device-select-toolbar.ui
  obs-ui/forms/source-toolbar/game-capture-toolbar.ui
  obs-ui/forms/source-toolbar/image-source-toolbar.ui
  obs-ui/forms/source-toolbar/media-controls.ui
  obs-ui/forms/source-toolbar/text-source-toolbar.ui)

target_sources(
  PRISMLiveStudio
  PRIVATE obs-ui/auth-oauth.cpp
  obs-ui/auth-oauth.hpp
  obs-ui/auth-listener.cpp
  obs-ui/auth-listener.hpp
  obs-ui/obf.c
  obs-ui/obf.h
  obs-ui/obs-app.cpp
  obs-ui/obs-app.hpp
  obs-ui/obs-proxy-style.cpp
  obs-ui/obs-proxy-style.hpp
  obs-ui/auth-base.cpp
  obs-ui/auth-base.hpp
  obs-ui/crash-report.cpp
  obs-ui/crash-report.hpp
  obs-ui/display-helpers.hpp
  obs-ui/platform.hpp
  obs-ui/qt-display.cpp
  obs-ui/qt-display.hpp
  obs-ui/qt-wrappers.cpp
  obs-ui/qt-wrappers.hpp
  obs-ui/ui-validation.cpp
  obs-ui/ui-validation.hpp
  obs-ui/multiview.cpp
  obs-ui/multiview.hpp
  $ENV{OBS_SRC_DIR}/deps/json11/json11.cpp
  $ENV{OBS_SRC_DIR}/deps/json11/json11.hpp
  $ENV{OBS_SRC_DIR}/deps/libff/libff/ff-util.c
  $ENV{OBS_SRC_DIR}/deps/libff/libff/ff-util.h
  ${CMAKE_CURRENT_BINARY_DIR}/ui-config.h)

target_sources(
  PRISMLiveStudio
  PRIVATE obs-ui/adv-audio-control.cpp
  obs-ui/adv-audio-control.hpp
  obs-ui/audio-encoders.cpp
  obs-ui/audio-encoders.hpp
  obs-ui/balance-slider.hpp
  obs-ui/clickable-label.hpp
  obs-ui/double-slider.cpp
  obs-ui/double-slider.hpp
  obs-ui/horizontal-scroll-area.cpp
  obs-ui/horizontal-scroll-area.hpp
  obs-ui/item-widget-helpers.cpp
  obs-ui/item-widget-helpers.hpp
  obs-ui/context-bar-controls.cpp
  obs-ui/context-bar-controls.hpp
  obs-ui/expand-checkbox.hpp
  obs-ui/focus-list.cpp
  obs-ui/focus-list.hpp
  obs-ui/hotkey-edit.cpp
  obs-ui/hotkey-edit.hpp
  obs-ui/lineedit-autoresize.cpp
  obs-ui/lineedit-autoresize.hpp
  obs-ui/locked-checkbox.cpp
  obs-ui/locked-checkbox.hpp
  obs-ui/log-viewer.cpp
  obs-ui/log-viewer.hpp
  obs-ui/media-controls.cpp
  obs-ui/media-controls.hpp
  obs-ui/media-slider.cpp
  obs-ui/media-slider.hpp
  obs-ui/menu-button.cpp
  obs-ui/menu-button.hpp
  obs-ui/mute-checkbox.hpp
  obs-ui/plain-text-edit.cpp
  obs-ui/plain-text-edit.hpp
  obs-ui/properties-view.hpp
  obs-ui/properties-view.moc.hpp
  obs-ui/record-button.cpp
  obs-ui/record-button.hpp
  obs-ui/remote-text.cpp
  obs-ui/remote-text.hpp
  obs-ui/scene-tree.cpp
  obs-ui/scene-tree.hpp
  obs-ui/screenshot-obj.hpp
  obs-ui/slider-absoluteset-style.cpp
  obs-ui/slider-absoluteset-style.hpp
  obs-ui/slider-ignorewheel.cpp
  obs-ui/slider-ignorewheel.hpp
  obs-ui/source-label.cpp
  obs-ui/source-label.hpp
  obs-ui/spinbox-ignorewheel.cpp
  obs-ui/spinbox-ignorewheel.hpp
  obs-ui/source-tree.cpp
  obs-ui/source-tree.hpp
  obs-ui/url-push-button.cpp
  obs-ui/url-push-button.hpp
  obs-ui/undo-stack-obs.cpp
  obs-ui/undo-stack-obs.hpp
  obs-ui/volume-control.cpp
  obs-ui/volume-control.hpp
  obs-ui/vertical-scroll-area.cpp
  obs-ui/vertical-scroll-area.hpp
  obs-ui/visibility-checkbox.cpp
  obs-ui/visibility-checkbox.hpp
  obs-ui/visibility-item-widget.cpp
  obs-ui/visibility-item-widget.hpp)

target_sources(
  PRISMLiveStudio
  PRIVATE obs-ui/window-basic-about.cpp
  obs-ui/window-basic-about.hpp
  obs-ui/window-basic-auto-config.cpp
  obs-ui/window-basic-auto-config.hpp
  obs-ui/window-basic-auto-config-test.cpp
  obs-ui/window-basic-adv-audio.cpp
  obs-ui/window-basic-adv-audio.hpp
  obs-ui/window-basic-filters.cpp
  obs-ui/window-basic-filters.hpp
  obs-ui/window-basic-interaction.cpp
  obs-ui/window-basic-interaction.hpp
  obs-ui/window-basic-main.cpp
  obs-ui/window-basic-main.hpp
  obs-ui/window-basic-main-browser.cpp
  obs-ui/window-basic-main-dropfiles.cpp
  obs-ui/window-basic-main-icons.cpp
  obs-ui/window-basic-main-outputs.cpp
  obs-ui/window-basic-main-outputs.hpp
  obs-ui/window-basic-main-profiles.cpp
  obs-ui/window-basic-main-scene-collections.cpp
  obs-ui/window-basic-main-screenshot.cpp
  obs-ui/window-basic-main-transitions.cpp
  obs-ui/window-basic-preview.cpp
  obs-ui/window-basic-properties.cpp
  obs-ui/window-basic-properties.hpp
  obs-ui/window-basic-settings.cpp
  obs-ui/window-basic-settings.hpp
  obs-ui/window-basic-settings-a11y.cpp
  obs-ui/window-basic-settings-stream.cpp
  obs-ui/window-basic-source-select.cpp
  obs-ui/window-basic-source-select.hpp
  obs-ui/window-basic-stats.cpp
  obs-ui/window-basic-stats.hpp
  obs-ui/window-basic-status-bar.cpp
  obs-ui/window-basic-status-bar.hpp
  obs-ui/window-basic-transform.cpp
  obs-ui/window-basic-transform.hpp
  obs-ui/window-basic-preview.hpp
  obs-ui/window-basic-vcam-config.cpp
  obs-ui/window-basic-vcam-config.hpp
  obs-ui/window-dock.cpp
  obs-ui/window-dock.hpp
  obs-ui/window-importer.cpp
  obs-ui/window-importer.hpp
  obs-ui/window-log-reply.hpp
  obs-ui/window-main.hpp
  obs-ui/window-missing-files.cpp
  obs-ui/window-missing-files.hpp
  obs-ui/window-namedialog.cpp
  obs-ui/window-namedialog.hpp
  obs-ui/window-log-reply.cpp
  obs-ui/window-projector.cpp
  obs-ui/window-projector.hpp
  obs-ui/window-remux.cpp
  obs-ui/window-remux.hpp)

target_sources(
  PRISMLiveStudio
  PRIVATE obs-ui/importers/importers.cpp
  obs-ui/importers/importers.hpp
  obs-ui/importers/classic.cpp
  obs-ui/importers/sl.cpp
  obs-ui/importers/studio.cpp
  obs-ui/importers/xsplit.cpp)

target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/common/PLSMessageBox.cpp
  prism-ui/common/PLSMessageBox.h
  prism-ui/common/PLSNameDialog.cpp
  prism-ui/common/PLSNameDialog.hpp
  prism-ui/common/PLSNameDialog.ui
  prism-ui/common/flowlayout.h
  prism-ui/common/flowlayout.cpp
  prism-ui/common/PLSToastMsgFrame.h
  prism-ui/common/PLSToastMsgFrame.cpp)

target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/forms/PLSGiphyStickerView.ui
  PRIVATE prism-ui/forms/PLSSearchPopupMenu.ui
  PRIVATE prism-ui/forms/PLSPrismSticker.ui
  PRIVATE prism-ui/forms/PLSVstFilterView.ui)

target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/main/PLSMainView.cpp
  prism-ui/main/PLSMainView.hpp
  prism-ui/main/PLSMainView.ui
  prism-ui/main/PLSLogo.cpp
  prism-ui/main/PLSLogo.h
  prism-ui/main/PLSLogo.ui
  prism-ui/main/PLSBasicStatusBar.cpp
  prism-ui/main/PLSBasicStatusBar.hpp
  prism-ui/main/PLSBasicStatusPanel.cpp
  prism-ui/main/PLSBasicStatusPanel.hpp
  prism-ui/main/PLSBasic.cpp
  prism-ui/main/PLSBasic.h
  prism-ui/main/PLSNewIconActionWidget.hpp
  prism-ui/main/PLSNewIconActionWidget.cpp
  prism-ui/main/PLSNewIconActionWidget.ui
  prism-ui/main/PLSContactView.hpp
  prism-ui/main/PLSContactView.cpp
  prism-ui/main/PLSContactView.ui
  prism-ui/main/PLSFileItemView.hpp
  prism-ui/main/PLSFileItemView.cpp
  prism-ui/main/PLSFileItemView.ui
  prism-ui/main/loading-event.hpp
  prism-ui/main/loading-event.cpp
  prism-ui/main/PLSFileButton.hpp
  prism-ui/main/PLSFileButton.cpp
  prism-ui/main/PLSFileButton.ui
  prism-ui/main/PLSAboutView.hpp
  prism-ui/main/PLSAboutView.cpp
  prism-ui/main/PLSAboutView.ui
  prism-ui/main/PLSUpdateView.hpp
  prism-ui/main/PLSUpdateView.cpp
  prism-ui/main/PLSUpdateView.ui
  prism-ui/main/PLSNoticeView.hpp
  prism-ui/main/PLSNoticeView.cpp
  prism-ui/main/PLSNoticeView.ui
  prism-ui/main/PLSGetPropertiesThread.h
  prism-ui/main/PLSGetPropertiesThread.cpp
  prism-ui/main/PLSPropertyModel.hpp
  prism-ui/main/PLSPropertyModel.cpp
  prism-ui/main/PLSTemplateButton.h
  prism-ui/main/PLSTemplateButton.cpp
  prism-ui/main/PLSTemplateButton.ui
  prism-ui/main/PLSAudioControl.h
  prism-ui/main/PLSAudioControl.cpp
  prism-ui/main/PLSFiltersItemView.ui
  prism-ui/main/PLSFiltersItemView.h
  prism-ui/main/PLSFiltersItemView.cpp
  prism-ui/main/PLSCompleter.hpp
)
target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/scene/PLSSceneItemView.cpp
  prism-ui/scene/PLSSceneItemView.h
  prism-ui/scene/PLSSceneListView.cpp
  prism-ui/scene/PLSSceneListView.h
  prism-ui/scene/PLSSceneTransitionsView.cpp
  prism-ui/scene/PLSSceneTransitionsView.h
  prism-ui/scene/PLSSceneDataMgr.cpp
  prism-ui/scene/PLSSceneDataMgr.h
  prism-ui/scene/PLSScrollAreaContent.cpp
  prism-ui/scene/PLSScrollAreaContent.h
  prism-ui/scene/PLSFloatScrollBarScrollArea.cpp
  prism-ui/scene/PLSFloatScrollBarScrollArea.h
  prism-ui/pls-common-language.hpp
  prism-ui/log/module_names.h
  prism-ui/log/log.h
  prism-ui/log/log.cpp
  prism-ui/forms/PLSSceneItemView.ui
  prism-ui/forms/PLSSceneListView.ui
  prism-ui/forms/PLSSceneTransitionsView.ui
  prism-ui/main/PLSToastButton.hpp
  prism-ui/main/PLSToastButton.cpp
  prism-ui/main/PLSToastButton.ui
  prism-ui/main/login-common-helper.hpp
  prism-ui/main/login-common-helper.cpp
  prism-ui/main/PLSMainViewConstValues.h
  prism-ui/main/PLSToastMsgPopup.hpp
  prism-ui/main/PLSToastMsgPopup.cpp
  prism-ui/main/PLSToastMsgPopup.ui
  prism-ui/main/PLSLivingMsgView.hpp
  prism-ui/main/PLSLivingMsgView.cpp
  prism-ui/main/PLSLivingMsgView.ui
  prism-ui/main/PLSLivingMsgItem.hpp
  prism-ui/main/PLSLivingMsgItem.cpp
  prism-ui/main/PLSLivingMsgItem.ui
  prism-ui/main/channel-login-view.hpp
  prism-ui/main/channel-login-view.cpp
  prism-ui/main/PLSChannelLoginView.ui
  prism-ui/main/login-user-info.hpp
  prism-ui/main/login-user-info.cpp
  prism-ui/main/pls-gpop-data.hpp
  prism-ui/main/pls-gpop-data.cpp
  prism-ui/main/pls-gpop-data-struct.hpp
  prism-ui/main/pls-notice-handler.hpp
  prism-ui/main/pls-notice-handler.cpp
  prism-ui/main/json-data-handler.cpp
  prism-ui/main/json-data-handler.hpp
  prism-ui/main/login-common-struct.hpp
  prism-ui/main/audio-meter-wrapper.h
  prism-ui/main/audio-meter-wrapper.cpp
  prism-ui/main/PLSAction.h
  prism-ui/main/PLSAction.cpp
  prism-ui/main/PLSAddSourceItem.ui
  prism-ui/main/PLSAddSourceView.h
  prism-ui/main/PLSAddSourceView.cpp
  prism-ui/main/PLSAddSourceView.ui
  prism-ui/main/PLSIPCHandler.h
  prism-ui/main/PLSIPCHandler.cpp
  prism-ui/main/window-basic-color-select.hpp
  prism-ui/main/window-basic-color-select.cpp
  prism-ui/forms/ColorSelectNew.ui
  prism-ui/main/PLSSourceTypeList.cpp
  prism-ui/main/api-interface.cpp
  prism-ui/main/PLSPreviewTitle.cpp
  prism-ui/main/PLSPreviewTitle.h
  prism-ui/main/pls-common-language.hpp
  prism-ui/scene-collection/window-basic-prism-scene-collection.cpp
  prism-ui/scene-collection/PLSSceneCollectionItem.h
  prism-ui/scene-collection/PLSSceneCollectionItem.cpp
  prism-ui/scene-collection/PLSSceneCollectionItem.ui
  prism-ui/scene-collection/PLSSceneCollectionView.h
  prism-ui/scene-collection/PLSSceneCollectionView.cpp
  prism-ui/scene-collection/PLSSceneCollectionView.ui
  prism-ui/scene-collection/PLSSceneCollectionManagement.h
  prism-ui/scene-collection/PLSSceneCollectionManagement.cpp
  prism-ui/scene-collection/PLSSceneCollectionManagement.ui
  prism-ui/scene-collection/PLSImporter.h
  prism-ui/scene-collection/PLSImporter.cpp
  prism-ui/scene-collection/PLSImporter.ui
  prism-ui/scene-collection/PLSImporterItem.h
  prism-ui/scene-collection/PLSImporterItem.cpp
  prism-ui/scene-collection/PLSImporterItem.ui
  prism-ui/scene-collection/PLSPrismShareMemory.h
  prism-ui/scene-collection/PLSPrismShareMemory.cpp
  prism-ui/giphy/GiphyDefine.h
  prism-ui/giphy/GiphyDownloader.h
  prism-ui/giphy/GiphyWebHandler.h
  prism-ui/giphy/PLSGiphyStickerView.h
  prism-ui/giphy/PLSGiphyStickerView.cpp
  prism-ui/giphy/GiphyWebHandler.cpp
  prism-ui/giphy/GiphyDownloader.cpp
  prism-ui/giphy/PLSSearchPopupMenu.h
  prism-ui/giphy/PLSSearchPopupMenu.cpp
  prism-ui/prism-sticker/PLSPrismSticker.h
  prism-ui/prism-sticker/PLSPrismSticker.cpp
  prism-ui/prism-sticker/PLSStickerDataHandler.h
  prism-ui/prism-sticker/PLSStickerDataHandler.cpp
  prism-ui/prism-sticker/PLSThumbnailLabel.hpp
  prism-ui/prism-sticker/PLSFileDownloader.h
  prism-ui/prism-sticker/PLSFileDownloader.cpp
  prism-ui/main/PLSBasicProperties.hpp
  prism-ui/main/PLSBasicProperties.cpp
  prism-ui/main/PLSPropertiesView.hpp
  prism-ui/main/PLSPropertiesView.cpp
  prism-ui/main/PLSPropertiesExtraUI.hpp
  prism-ui/main/PLSPropertiesExtraUI.cpp
  prism-ui/main/PLSRegionCapture.h
  prism-ui/main/PLSRegionCapture.cpp
  prism-ui/music/PLSBackgroundMusicView.h
  prism-ui/music/PLSBackgroundMusicView.cpp
  prism-ui/music/PLSBackgroundMusicView.ui
  prism-ui/music/PLSBgmItemCoverView.h
  prism-ui/music/PLSBgmItemCoverView.cpp
  prism-ui/music/PLSBgmItemCoverView.ui
  prism-ui/music/PLSBgmDataManager.h
  prism-ui/music/PLSBgmDataManager.cpp
  prism-ui/music/PLSBgmDragView.h
  prism-ui/music/PLSBgmDragView.cpp
  prism-ui/music/PLSBgmItemDelegate.h
  prism-ui/music/PLSBgmItemDelegate.cpp
  prism-ui/music/PLSBgmItemView.h
  prism-ui/music/PLSBgmItemView.cpp
  prism-ui/music/PLSBgmItemView.ui
  prism-ui/music/PLSBgmLibraryItem.ui
  prism-ui/music/PLSBgmLibraryView.h
  prism-ui/music/PLSBgmLibraryView.cpp
  prism-ui/music/PLSBgmLibraryView.ui
  prism-ui/music/PLSBgmControlsView.h
  prism-ui/music/PLSBgmControlsView.cpp
  prism-ui/music/PLSBgmControlsBase.h
  prism-ui/music/PLSBgmControlsBase.cpp
  prism-ui/music/PLSMediaSlider.h
  prism-ui/music/PLSMediaSlider.cpp
  prism-ui/music/window-basic-main-bgm.cpp
  prism-ui/source-toolbar/PLSSourceToolbar.hpp
  prism-ui/source-toolbar/PLSSourceToolbar.cpp
  prism-ui/main/PLSCompleter.cpp
)

target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/virtual-template/PLSMotionImageListView.h
  prism-ui/virtual-template/PLSMotionImageListView.cpp
  prism-ui/virtual-template/PLSMotionImageListView.ui
  prism-ui/virtual-template/PLSMotionDefine.h
  prism-ui/virtual-template/PLSMyMotionListView.h
  prism-ui/virtual-template/PLSMyMotionListView.cpp
  prism-ui/virtual-template/PLSMyMotionListView.ui
  prism-ui/virtual-template/PLSImageListView.h
  prism-ui/virtual-template/PLSImageListView.cpp
  prism-ui/virtual-template/PLSImageListView.ui
  prism-ui/virtual-template/PLSMotionItemView.h
  prism-ui/virtual-template/PLSMotionItemView.cpp
  prism-ui/virtual-template/PLSMotionItemView.ui
  prism-ui/virtual-template/PLSMotionFileManager.h
  prism-ui/virtual-template/PLSMotionFileManager.cpp
  prism-ui/virtual-template/PLSRecentListView.h
  prism-ui/virtual-template/PLSRecentListView.cpp
  prism-ui/virtual-template/PLSRecentListView.ui
  prism-ui/virtual-template/PLSPrismListView.h
  prism-ui/virtual-template/PLSPrismListView.cpp
  prism-ui/virtual-template/PLSPrismListView.ui
  prism-ui/virtual-template/PLSMotionNetwork.h
  prism-ui/virtual-template/PLSMotionNetwork.cpp
  prism-ui/virtual-template/PLSVirtualBgManager.h
  prism-ui/virtual-template/PLSVirtualBgManager.cpp
  prism-ui/virtual-template/PLSMotionErrorLayer.h
  prism-ui/virtual-template/PLSMotionErrorLayer.cpp
  prism-ui/virtual-template/PLSMotionErrorLayer.ui
  prism-ui/virtual-template/PLSMotionAddButton.h
  prism-ui/virtual-template/PLSMotionAddButton.cpp
  prism-ui/virtual-template/PLSMotionAddButton.ui
  prism-ui/main/PLSApp.h
  prism-ui/main/PLSApp.cpp
  prism-ui/main/PLSVstFilterView.h
  prism-ui/main/PLSVstFilterView.cpp
  prism-ui/main/PLSBrowserView.hpp
  prism-ui/main/PLSBrowserView.cpp
  prism-ui/main/PLSBrowserView.ui
  prism-ui/main/pls-complex-header-icon.hpp
  prism-ui/main/pls-complex-header-icon.cpp
  prism-ui/main/PLSComplexHeaderIcon.ui
  prism-ui/main/TextMotionTemplateButton.h
  prism-ui/main/TextMotionTemplateButton.cpp
  prism-ui/main/PLSTemplateButton.ui
  prism-ui/main/TextMotionTemplateDataHelper.h
  prism-ui/main/TextMotionTemplateDataHelper.cpp
  prism-ui/main/PLSTemplateButton.cpp
  prism-ui/main/PLSTemplateButton.h
  prism-ui/main/PLSSideBarDialogView.h
  prism-ui/main/PLSSideBarDialogView.cpp
  prism-ui/main/PLSOpenSourceView.h
  prism-ui/main/PLSOpenSourceView.cpp
  prism-ui/main/PLSOpenSourceView.ui
)

target_sources(
  PRISMLiveStudio
  PRIVATE prism-ui/main/PLSInfoCollector.cpp
  prism-ui/main/PLSInfoCollector.h)

if(OS_WINDOWS)
  target_sources(
    PRISMLiveStudio
    PRIVATE prism-ui/main/PLSDrawPen/windows/PLSD2DGeometry.h
    prism-ui/main/PLSDrawPen/windows/PLSD2DGeometry.cpp
    prism-ui/main/PLSDrawPen/windows/PLSD2DRenderTarget.h
    prism-ui/main/PLSDrawPen/windows/PLSD2DRenderTarget.cpp
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenCore.h
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenCore.cpp
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenWin.cpp
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenWin.cpp
    prism-ui/main/PLSDrawPen/windows/PLSGraphicsHandler.h
    prism-ui/main/PLSDrawPen/windows/PLSGraphicsHandler.cpp
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenEffect.h
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenEffect.cpp
    prism-ui/main/PLSDrawPen/windows/PLSDrawPenStroke.h
    prism-ui/main/PLSDrawPen/PLSDrawPenDefine.h
    prism-ui/main/PLSDrawPen/PLSDrawPenMgr.h
    prism-ui/main/PLSDrawPen/PLSDrawPenMgr.cpp
    prism-ui/main/PLSDrawPen/PLSDrawPenView.h
    prism-ui/main/PLSDrawPen/PLSDrawPenView.cpp
    prism-ui/main/PLSDrawPen/PLSDrawPenInterface.h
    prism-ui/forms/PLSDrawPenView.ui
    prism-ui/main/windows/PLSBlockDump.cpp
    prism-ui/main/windows/PLSBlockDump.h
    prism-ui/main/windows/PLSModuleMonitor.cpp
		prism-ui/main/windows/PLSModuleMonitor.h
    prism-ui/main/PLSBasicStatusPanel.ui)
elseif(OS_MACOS)
  target_sources(
    PRISMLiveStudio
    PRIVATE prism-ui/main/PLSDrawPen/mac/PLSDrawPenMac.cpp
    prism-ui/main/PLSDrawPen/mac/PLSDrawPenMac.h
    prism-ui/main/PLSDrawPen/mac/PLSStrokable.hpp
    prism-ui/main/PLSDrawPen/mac/PLSStrokable.mm
    prism-ui/main/PLSDrawPen/mac/PLSStrokeManager.hpp
    prism-ui/main/PLSDrawPen/mac/PLSStrokeManager.mm
    prism-ui/main/PLSDrawPen/mac/PLSStrokeManagerOCInterface.hpp
    prism-ui/main/PLSDrawPen/mac/PLSStrokeStruct.hpp
    prism-ui/main/PLSDrawPen/mac/PLSStrokeStruct.mm
    prism-ui/main/PLSDrawPen/PLSDrawPenDefine.h
    prism-ui/main/PLSDrawPen/PLSDrawPenMgr.h
    prism-ui/main/PLSDrawPen/PLSDrawPenMgr.cpp
    prism-ui/main/PLSDrawPen/PLSDrawPenView.h
    prism-ui/main/PLSDrawPen/PLSDrawPenView.cpp
    prism-ui/main/PLSDrawPen/PLSDrawPenInterface.h
    prism-ui/forms/PLSDrawPenView.ui
    prism-ui/main/mac/PLSBlockDump.mm
    prism-ui/main/mac/PLSBlockDump.h
    prism-ui/main/PLSBasicStatusPanel_mac.ui
    prism-ui/main/mac/PLSPermissionHelper.mm
    prism-ui/main/mac/PLSPermissionHelper.h)
endif()

# platform api
target_sources(PRISMLiveStudio PRIVATE ${PLSPlatformApi-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PLSPlatformApi-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PLSPlatformApi-UI_FILES})

# channel source
target_sources(PRISMLiveStudio PRIVATE ${channels-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${channels-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${channels-UI_FILES})
target_sources(PRISMLiveStudio PRIVATE ${channels-RCC_FILES})

# resotulion
target_sources(PRISMLiveStudio PRIVATE ${PLSResolution-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PLSResolution-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PLSResolution-UI_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PLSResolution-RCC_FILES})

# wizaard source
target_sources(PRISMLiveStudio PRIVATE ${WizardView-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${WizardView-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${WizardView-UI_FILES})
target_sources(PRISMLiveStudio PRIVATE ${WizardView-RCC_FILES})

# GuideTips
target_sources(PRISMLiveStudio PRIVATE ${GuideTips-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${GuideTips-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${GuideTips-UI_FILES})
target_sources(PRISMLiveStudio PRIVATE ${GuideTips-RCC_FILES})

# lab
target_sources(PRISMLiveStudio PRIVATE ${laboratory-SRC_FILES})
target_sources(PRISMLiveStudio PRIVATE ${laboratory-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${laboratory-UI_FILES})

# login
target_sources(PRISMLiveStudio PRIVATE ${PRISMLogin-SRC1_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PRISMLogin-SRC2_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PRISMLogin-HDR_FILES})
target_sources(PRISMLiveStudio PRIVATE ${PRISMLogin-UI_FILES})

if(OS_WINDOWS)
  target_sources(PRISMLiveStudio PRIVATE prism-ui/main/unzip.h)
endif()

# PLSPerformance
if(OS_WINDOWS)
  file(GLOB PLSPerformance_FILES prism-ui/PLSPerformance/win32/*)
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPerformance/win32/ PREFIX PLSPerformance FILES ${PLSPerformance_FILES})
elseif(OS_MACOS)
  file(GLOB PLSPerformance_FILES prism-ui/PLSPerformance/macos/*)
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPerformance/macos/ PREFIX PLSPerformance FILES ${PLSPerformance_FILES})
endif()

target_sources(PRISMLiveStudio PRIVATE ${PLSPerformance_FILES})

target_compile_features(PRISMLiveStudio PRIVATE cxx_std_17)

file(GLOB_RECURSE css_files prism-ui/*.css prism-ui/*.qss)
target_sources(PRISMLiveStudio PRIVATE ${css_files})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Css Files" FILES ${css_files})

target_include_directories(
  PRISMLiveStudio
  PRIVATE $ENV{OBS_SRC_DIR}/deps/json11
  $ENV{OBS_SRC_DIR}/deps/libff)

if(OS_WINDOWS)
  target_link_libraries(
    PRISMLiveStudio
    PRIVATE CURL::libcurl
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::avformat
    OBS::libobs
    OBS::frontend-api
    common-libs::ui
    common-libs::dump-analyzer
    common-libs::ipc
    common-libs::utils-api
    common-libs::log
    common-libs::resource
    common::browser
    common-libs::third-party::mqtt
    Qt6::NetworkAuth
    common::log
    Qt6::NetworkAuth)
elseif(OS_MACOS)
  target_link_libraries(
    PRISMLiveStudio
    PRIVATE CURL::libcurl
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::avformat
    OBS::libobs
    OBS::frontend-api
    common-libs::ui
    common-libs::dump-analyzer
    common-libs::ipc
    common-libs::utils-api
    common-libs::log
    common-libs::resource
    common::browser
    common-libs::third-party::mqtt
    common-libs::third-party::openssl
    Qt6::NetworkAuth
  )
endif()

set_target_properties(PRISMLiveStudio PROPERTIES FOLDER "prism/frontend")

if(TARGET OBS::browser-panels)
  get_target_property(_PANEL_INCLUDE_DIRECTORY OBS::browser-panels
    INTERFACE_INCLUDE_DIRECTORIES)
  target_include_directories(PRISMLiveStudio PRIVATE ${_PANEL_INCLUDE_DIRECTORY})

  target_compile_definitions(PRISMLiveStudio PRIVATE BROWSER_AVAILABLE)

  target_sources(
    PRISMLiveStudio
    PRIVATE obs-ui/window-dock-browser.cpp
    obs-ui/window-dock-browser.hpp
    obs-ui/window-extra-browsers.cpp
    obs-ui/window-extra-browsers.hpp)

  if(TWITCH_ENABLED)
    target_sources(PRISMLiveStudio PRIVATE auth-twitch.cpp auth-twitch.hpp)
  endif()

  if(RESTREAM_ENABLED)
    target_sources(PRISMLiveStudio PRIVATE auth-restream.cpp auth-restream.hpp)
  endif()

  if(OS_WINDOWS OR OS_MACOS)
    set(ENABLE_WHATSNEW
      ON
      CACHE INTERNAL "Enable WhatsNew dialog")
  elseif(OS_LINUX)
    option(ENABLE_WHATSNEW "Enable WhatsNew dialog" ON)
  endif()

  if(ENABLE_WHATSNEW)
    target_compile_definitions(PRISMLiveStudio PRIVATE WHATSNEW_ENABLED)
  endif()
endif()

if(YOUTUBE_ENABLED)
  target_sources(
    PRISMLiveStudio
    PRIVATE auth-youtube.cpp auth-youtube.hpp youtube-api-wrappers.cpp
    youtube-api-wrappers.hpp window-youtube-actions.cpp
    window-youtube-actions.hpp)
endif()

prism_exe(PRISMLiveStudio "PRISM Live Studio")

if(OS_WINDOWS)
  set_target_properties(PRISMLiveStudio PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME PRISMLiveStudio)

  find_package(Detours REQUIRED)

  target_sources(
    PRISMLiveStudio
    PRIVATE obs-ui/obs.manifest
    obs-ui/platform-windows.cpp
    prism-ui/main/win-dll-blocklist.c
    obs-ui/win-update/update-window.cpp
    obs-ui/win-update/update-window.hpp
    obs-ui/win-update/win-update.cpp
    obs-ui/win-update/win-update.hpp
    obs-ui/win-update/win-update-helpers.cpp
    obs-ui/win-update/win-update-helpers.hpp
    prism-ui/main/unzip.h
    prism-ui/main/unzip.cpp)

  if(_QT_VERSION EQUAL 5)
    find_qt(COMPONENTS WinExtras)
    target_link_libraries(PRISMLiveStudio PRIVATE Qt::WinExtras)
  endif()

  target_link_libraries(PRISMLiveStudio PRIVATE crypt32 OBS::blake2 Detours::Detours)

  target_compile_definitions(
    PRISMLiveStudio PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS PSAPI_VERSION=2)

  if(MSVC)
    target_link_options(PRISMLiveStudio PRIVATE "LINKER:/IGNORE:4098" "LINKER:/IGNORE:4099")
    target_link_libraries(PRISMLiveStudio PRIVATE OBS::w32-pthreads)

    set_source_files_properties(
      $ENV{OBS_SRC_DIR}/deps/libff/libff/ff-util.c
      PROPERTIES COMPILE_FLAGS -Dinline=__inline)
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(PRISMLiveStudio PRIVATE /LARGEADDRESSAWARE)
  endif()

  add_subdirectory(obs-ui/win-update/updater)
elseif(OS_MACOS)
  set_target_properties(
    PRISMLiveStudio

    # PROPERTIES OUTPUT_NAME ${OBS_BUNDLE_NAME}
    PROPERTIES OUTPUT_NAME "PRISMLiveStudio"
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST $ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/Info.plist.in
  )

  if(XCODE)
    set_target_properties(
      PRISMLiveStudio
      PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
      "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
      XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
      XCODE_ATTRIBUTE_PRODUCT_NAME "PRISMLiveStudio")

    set(APP_ICON_TARGET $ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/Assets.xcassets)

    target_sources(PRISMLiveStudio PRIVATE ${APP_ICON_TARGET})
    set_source_files_properties(${APP_ICON_TARGET}
      PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    # Set code signing options
    if(DEFINED ENV{PRISM_BUNDLE_PP})
      message("XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY: ${PRISM_BUNDLE_CODESIGN_IDENTITY}")
      message("XCODE_ATTRIBUTE_DEVELOPMENT_TEAM: ${PRISM_BUNDLE_CODESIGN_TEAM}")
      message("XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER: $ENV{PRISM_BUNDLE_PP}")
      set_target_properties(
        PRISMLiveStudio
        PROPERTIES XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${PRISM_BUNDLE_CODESIGN_IDENTITY}"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PRISM_BUNDLE_CODESIGN_TEAM}"
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER $ENV{PRISM_BUNDLE_PP})
    endif()

  else()
    set(APP_ICON_TARGET $ENV{PRISM_SRC_DIR}/cmake/bundle/macOS/AppIcon.iconset)
    set(APP_ICON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns)

    add_custom_command(
      OUTPUT ${APP_ICON_OUTPUT} COMMAND iconutil -c icns "${APP_ICON_TARGET}"
      -o "${APP_ICON_OUTPUT}")

    set(MACOSX_BUNDLE_ICON_FILE AppIcon.icns)
    target_sources(PRISMLiveStudio PRIVATE ${APP_ICON_OUTPUT}
      $ENV{OBS_SRC_DIR}/AUTHORS)

    set_source_files_properties(${APP_ICON_OUTPUT}
      PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()

  find_library(APPKIT Appkit)
  find_library(AVFOUNDATION AVFoundation)
  find_library(APPLICATIONSERVICES ApplicationServices)
  mark_as_advanced(APPKIT AVFOUNDATION APPLICATIONSERVICES)

  target_link_libraries(PRISMLiveStudio PRIVATE ${APPKIT} ${AVFOUNDATION}
    ${APPLICATIONSERVICES})

  if(ENABLE_SPARKLE_UPDATER)
    find_library(SPARKLE Sparkle)
    mark_as_advanced(SPARKLE)

    target_sources(PRISMLiveStudio PRIVATE obs-ui/sparkle-updater.mm)
    target_compile_definitions(PRISMLiveStudio PRIVATE ENABLE_SPARKLE_UPDATER)

    target_link_libraries(PRISMLiveStudio PRIVATE ${SPARKLE})
  endif()

  target_sources(PRISMLiveStudio PRIVATE obs-ui/platform-osx.mm)
  target_sources(PRISMLiveStudio PRIVATE obs-ui/forms/OBSPermissions.ui
    obs-ui/window-permissions.cpp
    obs-ui/window-permissions.hpp)

  if(ENABLE_WHATSNEW)
    find_library(SECURITY Security)
    mark_as_advanced(SECURITY)
    target_link_libraries(PRISMLiveStudio PRIVATE ${SECURITY} OBS::blake2)

    target_sources(
      PRISMLiveStudio
      PRIVATE obs-ui/nix-update/crypto-helpers.hpp
      obs-ui/nix-update/crypto-helpers-mac.mm
      obs-ui/nix-update/nix-update.cpp
      obs-ui/nix-update/nix-update.hpp
      obs-ui/nix-update/nix-update-helpers.cpp
      obs-ui/nix-update/nix-update-helpers.hpp)
  endif()

  set_source_files_properties(obs-ui/platform-osx.mm prism-ui/main/PLSDrawPen/mac/PLSStrokable.mm prism-ui/main/PLSDrawPen/mac/PLSStrokeManager.mm prism-ui/main/PLSDrawPen/mac/PLSStrokeStruct.mm PROPERTIES COMPILE_FLAGS
    -fobjc-arc)

elseif(OS_POSIX)
  target_sources(PRISMLiveStudio PRIVATE platform-x11.cpp)
  target_link_libraries(PRISMLiveStudio PRIVATE Qt::GuiPrivate)

  if(TARGET obspython)
    find_package(Python REQUIRED COMPONENTS Interpreter Development)
    target_link_libraries(PRISMLiveStudio PRIVATE Python::Python)
    target_link_options(PRISMLiveStudio PRIVATE "LINKER:-no-as-needed")
  endif()

  if(NOT LINUX_PORTABLE)
    add_subdirectory(xdg-data)
  endif()

  if(OS_FREEBSD)
    target_link_libraries(PRISMLiveStudio PRIVATE procstat)
  endif()

  if(OS_LINUX AND ENABLE_WHATSNEW)
    find_package(MbedTLS)

    if(NOT MBEDTLS_FOUND)
      obs_status(
        FATAL_ERROR
        "mbedTLS not found, but required for WhatsNew support on Linux")
    endif()

    target_sources(
      PRISMLiveStudio
      PRIVATE obs-ui/nix-update/crypto-helpers.hpp
      obs-ui/nix-update/crypto-helpers-mbedtls.cpp
      obs-ui/nix-update/nix-update.cpp
      obs-ui/nix-update/nix-update.hpp
      obs-ui/nix-update/nix-update-helpers.cpp
      obs-ui/nix-update/nix-update-helpers.hpp)
    target_link_libraries(PRISMLiveStudio PRIVATE Mbedtls::Mbedtls OBS::blake2)
  endif()
endif()

target_link_libraries(PRISMLiveStudio PRIVATE prism::frontend-api)
add_dependencies(PRISMLiveStudio obs-build obs-copy libbrowser)

get_target_property(_SOURCES PRISMLiveStudio SOURCES)
set(_UI ${_SOURCES})
list(FILTER _UI INCLUDE REGEX ".*\\.ui?")

source_group(
  TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "UI Files"
  FILES ${_UI})
unset(_SOURCES)
unset(_UI)

# group platformapi group all files
set(PlatformALllFiles ${PLSPlatformApi-HDR_FILES} ${PLSPlatformApi-SRC_FILES} ${PLSPlatformApi-UI_FILES})

foreach(FILE ${PlatformALllFiles})
  # Get the directory of the source file
  get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

  # message(STATUS "PARENT_DIR IS ${PARENT_DIR}")
  # Remove common directory prefix to make the group
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/prism-ui/PLSPlatformApi" "" RELATIVE_GROUP "${PARENT_DIR}")

  # Make sure we are using windows slashes
  string(REPLACE "/" "\\" RELATIVE_GROUP "${RELATIVE_GROUP}")

  # group platform folder
  if("${FILE}" MATCHES ".*\\.cpp" OR "${FILE}" MATCHES ".*\\.c" OR "${FILE}" MATCHES ".*\\.cxx" OR "${FILE}" MATCHES ".*\\.cc" OR "${FILE}" MATCHES ".*\\.mm")
    set(RELATIVE_GROUP "Source Files${RELATIVE_GROUP}")
    source_group("${RELATIVE_GROUP}" FILES "${FILE}")
  elseif("${FILE}" MATCHES ".*\\.h" OR "${FILE}" MATCHES ".*\\.hpp" OR "${FILE}" MATCHES ".*\\.hxx" OR "${FILE}" MATCHES ".*\\.hh")
    set(RELATIVE_GROUP "Header Files${RELATIVE_GROUP}")
    source_group("${RELATIVE_GROUP}" FILES "${FILE}")
  endif()
endforeach()

if(OS_WINDOWS)
  target_compile_definitions(
    PRISMLiveStudio PRIVATE
    DL_METAL=""
    DL_D3D11="libobs-d3d11.dll"
    DL_OPENGL="libobs-opengl.dll"
    DL_D3D9="")
elseif(OS_MACOS)
  target_compile_definitions(
    PRISMLiveStudio PRIVATE
    DL_METAL=""
    DL_D3D11="libobs-d3d11.dll"
    DL_OPENGL="libobs-opengl.dylib"
    DL_D3D9="")
endif()

# define_graphic_modules(PRISMLiveStudio)
# setup_obs_app(PRISMLiveStudio)
# setup_target_resources(PRISMLiveStudio obs-studio)
# add_target_resource(PRISMLiveStudio $ENV{OBS_SRC_DIR}/AUTHORS
# obs-studio/authors)
if(OS_MACOS)
  copy_common_libs_data(PRISMLiveStudio data data/prism-studio)
  copy_common_libs_data(PRISMLiveStudio prism-ui/main/PLSDrawPen/data data/libobs)
  setup_prism_app(PRISMLiveStudio)
elseif(OS_WINDOWS)
  copy_prism_binary(PRISMLiveStudio)
  copy_prism_data(PRISMLiveStudio)
  copy_data(PRISMLiveStudio PRISMLiveStudio prism-ui/resource/LUTs data/obs-plugins/obs-filters/LUTs)
  copy_data(PRISMLiveStudio PRISMLiveStudio prism-ui/main/PLSDrawPen/data data/libobs)
  copy_third_party_mqtt(PRISMLiveStudio)
  deploy_qt(PRISMLiveStudio)
endif()

copy_third_party_apng(PRISMLiveStudio)
