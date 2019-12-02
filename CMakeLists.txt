cmake_minimum_required(VERSION 3.0)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

include_directories("DevIL Windows SDK/include")
link_directories("DevIL Windows SDK/lib/x64/unicode/Release")

set(SOURCE_FILE main.cpp pixel_format.cpp
                ILImage.h
				ILImageSupport.cpp
                ConvertImage.cpp
				TextureFileCreater.h
				TextureFileCreater.cpp
                TextureFileCreaterR.cpp
                TextureFileCreaterRG.cpp
                TextureFileCreaterRGB.cpp
                TextureFileCreaterRGBA.cpp)

set(HEADER_FILE pixel_format.h ConvertImage.h)

SOURCE_GROUP("Header Files" FILES ${HEADER_FILE})
SOURCE_GROUP("Source Files" FILES ${SOURCE_FILE})

add_executable(TexConv ${SOURCE_FILE} ${HEADER_FILE})

target_link_libraries(TexConv PRIVATE DevIL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_MIN_VERSION "5.8.0")
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include(QtCommon)

# Set additional project information
set(COMPANY "hyzgame.com")
set(COPYRIGHT "Copyright (c) 1997-2019 hyzgame.com. All rights reserved.")
set(IDENTIFIER "com.hyzgame.texconv")

target_link_libraries(TexConv PRIVATE	CMCore CMPlatform CMUtil
										Qt5::Core Qt5::Gui Qt5::Widgets)
