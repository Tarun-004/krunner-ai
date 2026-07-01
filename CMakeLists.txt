cmake_minimum_required(VERSION 3.16)
project(krunner-groq)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(KF_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.6.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Network)
find_package(KF6Runner ${KF_MIN_VERSION} REQUIRED)
find_package(KF6CoreAddons ${KF_MIN_VERSION} REQUIRED)
find_package(KF6I18n ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Config ${KF_MIN_VERSION} REQUIRED)

kcoreaddons_add_plugin(krunner_groq
    SOURCES
        src/groqrunner.cpp
        src/groqclient.cpp
    INSTALL_NAMESPACE "kf6/krunner"
)

target_link_libraries(krunner_groq
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    KF6::Runner
    KF6::CoreAddons
    KF6::I18n
    KF6::ConfigCore
)
