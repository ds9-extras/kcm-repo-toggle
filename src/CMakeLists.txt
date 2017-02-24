configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/Version.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_NO_KEYWORDS)

set(kcm_SRCS
    main.cpp
    Module.cpp
    OSRelease.cpp
)

ki18n_wrap_ui(kcm_SRCS Module.ui)

add_library(kcmchannelswitch MODULE ${kcm_SRCS})

target_link_libraries(kcmchannelswitch
    Qt5::Core
    KF5::ConfigWidgets
    KF5::CoreAddons
    KF5::I18n
    QApt
)

kauth_install_actions(org.kde.kcontrol.kcmchannelswitch kcmchannelswitch.actions)
set(helper_SRCS
    AuthHelper.cpp
)
add_executable(kcmchannelswitchauthhelper ${helper_SRCS})
target_link_libraries(kcmchannelswitchauthhelper
    Qt5::Core
    KF5::Auth
    KF5::I18n
    QApt
)
kauth_install_helper_files(kcmchannelswitchauthhelper org.kde.kcontrol.kcmchannelswitch root)
install(TARGETS kcmchannelswitchauthhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})

install(TARGETS kcmchannelswitch DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES channel-switch.desktop DESTINATION ${SERVICES_INSTALL_DIR})
install(DIRECTORY channels DESTINATION ${KDE_INSTALL_DATADIR}/release-channels/)
