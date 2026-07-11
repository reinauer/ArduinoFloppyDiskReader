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

install(TARGETS DrawBridgeQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
