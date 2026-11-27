qt_add_executable(DrawBridgeQt MACOSX_BUNDLE
    main.cpp
    MainWindow.cpp
    MainWindow.h
    resources.qrc
)

target_link_libraries(DrawBridgeQt PRIVATE
    drawbridge-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(DrawBridgeQt PROPERTIES
    OUTPUT_NAME "DrawBridge"
    MACOSX_BUNDLE_BUNDLE_NAME "DrawBridge"
    MACOSX_BUNDLE_GUI_IDENTIFIER "uk.co.robsmithdev.DrawBridge"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "2.8.8"
    MACOSX_BUNDLE_BUNDLE_VERSION "2.8.8"
)

if(APPLE)
    # Icon shown for the .app in Finder (CFBundleIconFile). The QIcon set in main.cpp only affects the window.
    set(DRAWBRIDGE_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/DrawBridge.icns)
    set_source_files_properties(${DRAWBRIDGE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(DrawBridgeQt PRIVATE ${DRAWBRIDGE_ICNS})
    set_target_properties(DrawBridgeQt PROPERTIES MACOSX_BUNDLE_ICON_FILE DrawBridge.icns)
endif()


install(TARGETS DrawBridgeQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
