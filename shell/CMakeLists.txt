
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_BINARY_DIR}/../
)

# Charta desktop application

set(charta_SRCS
   main.cpp
   okular_main.cpp
   shell.cpp
   shellutils.cpp
   welcomescreen.cpp

   ../gui/recentitemsmodel.cpp

   shell.qrc
)

ki18n_wrap_ui(charta_SRCS
    welcomescreen.ui)

add_executable(charta ${charta_SRCS})

target_link_libraries(charta KF6::I18n KF6::Parts KF6::WindowSystem KF6::Crash KF6::IconThemes KF6::KIOGui)
if (TARGET Qt6::DBus)
    target_link_libraries(charta Qt6::DBus)
endif()
if (HAVE_X11)
    target_link_libraries(charta Qt6::GuiPrivate)
endif()

install(TARGETS charta ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(APPLE)
    set_target_properties(charta PROPERTIES
        MACOSX_BUNDLE_DISPLAY_NAME "Charta"
        MACOSX_BUNDLE_BUNDLE_NAME "Charta"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Charta ${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.charta.reader"
        MACOSX_BUNDLE_COPYRIGHT "2002-2026 The Okular Developers")
endif()

# Charta desktop integration files

install(PROGRAMS org.charta.reader.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.charta.reader.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
