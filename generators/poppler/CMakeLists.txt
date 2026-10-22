remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_poppler")

add_subdirectory( conf )

set(CMAKE_REQUIRED_LIBRARIES Poppler::Qt6 Qt6::Core Qt6::Gui)

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

########### next target ###############

set(okularGenerator_poppler_PART_SRCS
   generator_pdf.cpp
   formfields.cpp
   annots.cpp
   pdfsignatureutils.cpp
   pdfsettingswidget.cpp
   imagescaling.cpp
)

ki18n_wrap_ui(okularGenerator_poppler_PART_SRCS
   conf/pdfsettingswidget.ui
)

kconfig_add_kcfg_files(okularGenerator_poppler_PART_SRCS GENERATE_MOC conf/pdfsettings.kcfgc )

okular_add_generator(okularGenerator_poppler ${okularGenerator_poppler_PART_SRCS})

target_link_libraries(okularGenerator_poppler chartacore KF6::I18n KF6::Completion KF6::KIOWidgets Poppler::Qt6 Qt6::Xml)
if (HAVE_QPDF)
    target_link_libraries(okularGenerator_poppler ${QPDF_LINK_TARGET})
    target_compile_options(okularGenerator_poppler PRIVATE -fexceptions)
endif()

########### autotests ###############

if(BUILD_TESTING)
    ecm_add_test(autotests/testimagescaling.cpp
        TEST_NAME "imageScalingTest"
        LINK_LIBRARIES Qt6::Test Qt6::Gui
    )
endif()

########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_pdf.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_pdf.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-poppler.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
