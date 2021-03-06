project(kmines)

cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.2.0")

find_package(ECM 1.7.0 REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Widgets Test Qml)
find_package(KF5 REQUIRED COMPONENTS 
  CoreAddons 
  Config 
  WidgetsAddons 
  Config 
  DBusAddons 
  I18n 
  ConfigWidgets 
  TextWidgets 
  XmlGui 
  KIO 
  NotifyConfig
  DocTools
)
    
find_package(KF5KDEGames 4.9.0 REQUIRED)
find_package(Phonon4Qt5)

include(FeatureSummary)
include(ECMAddAppIcon)
include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

add_subdirectory( data ) 
add_subdirectory( themes ) 
add_subdirectory( doc )

include_directories( ${CMAKE_SOURCE_DIR}/KF5KDEGames/highscore  )
add_definitions("-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII")


########### next target ###############

set(kmines_SRCS
   mainwindow.cpp
   cellitem.cpp
   borderitem.cpp
   minefielditem.cpp
   scene.cpp
   main.cpp )

ki18n_wrap_ui(kmines_SRCS customgame.ui generalopts.ui)
kconfig_add_kcfg_files(kmines_SRCS settings.kcfgc )
file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/data/*-apps-kmines.png")
ecm_add_app_icon(kmines_SRCS ICONS ${ICONS_SRCS})
add_executable(kmines ${kmines_SRCS})

target_link_libraries(kmines 
  KF5::TextWidgets 
  KF5::WidgetsAddons
  KF5::DBusAddons 
  Qt5::Qml 
  KF5::XmlGui
  KF5KDEGames)

install(TARGETS kmines  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )

########### install files ###############

install( FILES kminesui.rc  DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/kmines )
install( FILES kmines.knsrc  DESTINATION  ${KDE_INSTALL_CONFDIR} )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
