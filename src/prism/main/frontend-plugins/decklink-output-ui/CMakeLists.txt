project(decklink-output-ui)

add_definitions(-D_PRISM_PLUGIN)
set(decklink-ouput-ui_HEADERS
	${decklink-ouput-ui_HEADERS}
	../../properties-view.hpp
	../../properties-view.moc.hpp
	../../vertical-scroll-area.hpp
	../../double-slider.hpp
	../../slider-ignorewheel.hpp
	../../spinbox-ignorewheel.hpp
	../../spinbox.hpp
	./DecklinkOutputUI.h
	decklink-ui-main.h)
set(decklink-ouput-ui_SOURCES
	${decklink-ouput-ui_SOURCES}
	../../properties-view.cpp
	../../vertical-scroll-area.cpp
	../../double-slider.cpp
	../../slider-ignorewheel.cpp
	../../spinbox-ignorewheel.cpp
	../../spinbox.cpp
	./DecklinkOutputUI.cpp
	decklink-ui-main.cpp)
set(decklink-ouput-ui_UI
	${decklink-ouput-ui_UI}
	forms/output.ui)

qt5_wrap_ui(decklink-ouput-ui_UI_HEADERS
	${decklink-ouput-ui_UI})

add_library(decklink-ouput-ui MODULE
	${decklink-ouput-ui_HEADERS}
	${decklink-ouput-ui_SOURCES}
	${decklink-ouput-ui_UI_HEADERS})
target_link_libraries(decklink-ouput-ui
	${frontend-tools_PLATFORM_LIBS}
	obs-frontend-api
	frontend-api
	Qt5::Widgets
	liblog
	log)
add_obs_dependency(decklink-ouput-ui)
link_obs(decklink-ouput-ui)

install_obs_plugin_with_data(decklink-ouput-ui data)
copy_prism_plugin_target(decklink-ouput-ui)
