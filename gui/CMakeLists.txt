# TexConvQt —— Qt6 Widgets GUI 外壳。
# 仅依赖 TexCommon(格式表静态库)+ TexConvCore(C API);Qt 经 vcpkg toolchain 解析。
# 找不到 Qt 时优雅跳过,不影响 CLI 构建。

option(TEXCONV_BUILD_GUI "构建 TexConv Qt GUI 外壳" ON)

if(NOT TEXCONV_BUILD_GUI)
    return()
endif()

find_package(Qt6 6.5 COMPONENTS Core Gui Widgets QUIET)

if(NOT Qt6_FOUND)
    message(STATUS "TexConv: Qt6 not found -> TexConvQt skipped (vcpkg install qtbase:x64-windows to enable)")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(TexConvQt WIN32
    src/main.cpp
    src/CoreApi.h
    src/CoreApi.cpp
    src/TexFileModel.h
    src/TexFileModel.cpp
    src/FormatDelegate.h
    src/FormatDelegate.cpp
    src/JobRunner.h
    src/JobRunner.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_include_directories(TexConvQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(TexConvQt PRIVATE
    TEXCONV_SHARED_USE      # TexConvCore.dll 导入
    QT_NO_KEYWORDS          # Qt signals/slots 宏与 ULRE 头冲突(CMQT 约定)
)

target_link_libraries(TexConvQt
    PRIVATE
        TexCommon
        TexConvCore
        Qt6::Core Qt6::Gui Qt6::Widgets
)

set_target_properties(TexConvQt PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:TexConvQt>"
)

texconv_deploy_im_runtime(TexConvQt)

# vcpkg applocal 只拷 bin 下的 DLL,不拷 plugins —— 平台插件自己部署
if(TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET TexConvQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TexConvQt>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:TexConvQt>/platforms/"
        COMMENT "Deploy Qt platform plugin (qwindows)"
    )
endif()
