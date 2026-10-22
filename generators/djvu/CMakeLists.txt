remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_djvu")

########### next target ###############

set(okularGenerator_djvu_SRCS
   generator_djvu.cpp
   kdjvu.cpp
)


okular_add_generator(okularGenerator_djvu ${okularGenerator_djvu_SRCS})

target_include_directories(okularGenerator_djvu PRIVATE ${DJVULIBRE_INCLUDE_DIR})
target_link_libraries(okularGenerator_djvu chartacore ${DJVULIBRE_LIBRARY} KF6::I18n)

########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_djvu.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_djvu.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-djvu.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )

