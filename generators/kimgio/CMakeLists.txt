remove_definitions(-DTRANSLATION_DOMAIN="charta")
add_definitions(-DTRANSLATION_DOMAIN="charta_kimgio")

########### next target ###############

okular_add_generator(okularGenerator_kimgio generator_kimgio.cpp)
target_link_libraries(okularGenerator_kimgio chartacore KF6::I18n)

if(TARGET KExiv2Qt6)
    target_compile_definitions(okularGenerator_kimgio PRIVATE -DWITH_KEXIV=1)
    target_link_libraries(okularGenerator_kimgio KExiv2Qt6)
endif()

if(BUILD_TESTING AND BUILD_DESKTOP AND KF6KExiv2_FOUND)
    add_definitions( -DKDESRCDIR="${CMAKE_CURRENT_SOURCE_DIR}/" )
    set( kimgiotest_SRCS tests/kimgiotest.cpp ${CMAKE_SOURCE_DIR}/gui/pagepainter.cpp ${CMAKE_SOURCE_DIR}/gui/guiutils.cpp ${CMAKE_SOURCE_DIR}/gui/debug_ui.cpp )
    ecm_add_test(${kimgiotest_SRCS} TEST_NAME "kimgiotest" LINK_LIBRARIES chartacore okularpart Qt6::Svg Qt6::Test)
    target_compile_definitions(kimgiotest PRIVATE -DGENERATOR_PATH="$<TARGET_FILE:okularGenerator_kimgio>")
endif()


########### install files ###############
if (BUILD_DESKTOP)
  install( PROGRAMS okularApplication_kimgio.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
if (BUILD_MOBILE)
  install( PROGRAMS org.kde.mobile.okular_kimgio.desktop DESTINATION ${KDE_INSTALL_APPDIR} )
endif()
install( FILES org.charta.reader-kimgio.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
