
install( PROGRAMS org.kde.kmines.desktop  DESTINATION  ${KDE_INSTALL_APPDIR} )
install( FILES kmines.notifyrc  DESTINATION  ${KDE_INSTALL_KNOTIFY5RCDIR} )

ecm_install_icons(ICONS
    16-apps-kmines.png
    22-apps-kmines.png
    32-apps-kmines.png
    48-apps-kmines.png
    64-apps-kmines.png
    128-apps-kmines.png
    DESTINATION ${KDE_INSTALL_ICONDIR}
    THEME hicolor
)

