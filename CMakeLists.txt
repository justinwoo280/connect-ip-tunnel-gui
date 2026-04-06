cmake_minimum_required(VERSION 3.16)
project(connect-ip-tunnel-gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 配置
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CoreProcess.cpp
    src/NodeManager.cpp
    src/StatsPoller.cpp
    src/EditNodeDialog.cpp
    src/SystemTray.cpp
)

set(HEADERS
    src/MainWindow.h
    src/CoreProcess.h
    src/NodeManager.h
    src/StatsPoller.h
    src/EditNodeDialog.h
    src/SystemTray.h
    src/TunnelNode.h
)

set(UI_FILES
    ui/MainWindow.ui
    ui/EditNodeDialog.ui
)

set(RESOURCES
    resources/resources.qrc
)

# 可执行文件
if(WIN32)
    # Windows：附加 RC 文件以嵌入 .ico 图标到 EXE
    add_executable(${PROJECT_NAME} WIN32
        ${SOURCES}
        ${HEADERS}
        ${UI_FILES}
        ${RESOURCES}
        resources/app.rc
    )
elseif(APPLE)
    # macOS：将 .icns 作为 bundle 资源
    set(MACOSX_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.icns)
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${SOURCES}
        ${HEADERS}
        ${UI_FILES}
        ${RESOURCES}
        ${MACOSX_ICON}
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_ICON_FILE app.icns
        MACOSX_BUNDLE_BUNDLE_NAME "Connect-IP Tunnel"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
else()
    add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        ${UI_FILES}
        ${RESOURCES}
    )
endif()

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_BINARY_DIR}
)

# 设置 AUTOUIC 路径
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# 平台特定配置
if(WIN32)
    target_link_libraries(${PROJECT_NAME} wininet)
endif()

# 安装规则
if(APPLE)
    # macOS：MACOSX_BUNDLE 需要用 BUNDLE DESTINATION
    install(TARGETS ${PROJECT_NAME}
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION bin
    )
else()
    # Windows / Linux：普通可执行文件
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )
endif()
