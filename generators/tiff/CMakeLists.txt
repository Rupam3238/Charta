remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_tiff")

########### next target ###############

set(okularGenerator_tiff_SRCS
   generator_tiff.cpp
)

okular_add_generator(okularGenerator_tiff ${okularGenerator_tiff_SRCS})
target_link_libraries(okularGenerator_tiff chartacore TIFF::TIFF KF6::I18n)

########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_tiff.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_tiff.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-tiff.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
