remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_fax")

########### next target ###############

set(okularGenerator_fax_PART_SRCS generator_fax.cpp faxdocument.cpp faxexpand.cpp faxinit.cpp fax_debug.cpp)

okular_add_generator(okularGenerator_fax ${okularGenerator_fax_PART_SRCS})

target_link_libraries(okularGenerator_fax chartacore KF6::I18n Qt6::Xml)

########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_fax.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_fax.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-fax.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
