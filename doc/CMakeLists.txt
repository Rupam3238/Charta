########### install files ###############
#
#
kdoctools_create_handbook(index.docbook INSTALL_DESTINATION ${KDE_INSTALL_DOCBUNDLEDIR}/en SUBDIR charta)
kdoctools_create_manpage(man-charta.1.docbook 1 INSTALL_DESTINATION ${KDE_INSTALL_MANDIR})
