configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/Version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_NO_KEYWORDS)

set(kcm_SRCS
    main.cpp
    Module.cpp
    OSRelease.cpp
)

ki18n_wrap_ui(kcm_SRCS Module.ui)

add_library(kcm_channel_switch MODULE ${kcm_SRCS})

target_link_libraries(kcm_channel_switch
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::I18n
    QApt)

install(TARGETS kcm_channel_switch DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES channel-switch.desktop DESTINATION ${SERVICES_INSTALL_DIR})
