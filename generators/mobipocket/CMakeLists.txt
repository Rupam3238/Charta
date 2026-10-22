remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_mobi")

########### next target ###############

set(okularGenerator_mobi_PART_SRCS
  mobidocument.cpp
  generator_mobi.cpp
  converter.cpp
)

okular_add_generator(okularGenerator_mobi ${okularGenerator_mobi_PART_SRCS})

target_link_libraries(okularGenerator_mobi chartacore QMobipocket6 KF6::I18n Qt6::Gui)

########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_mobi.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_mobi.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-mobipocket.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
