cmake_minimum_required(VERSION 3.0)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

include_directories("DevIL Windows SDK/include")
link_directories("DevIL Windows SDK/lib/x64/unicode/Release")

SET(ILIMAGE_SOURCE  ILImage.h
                    ILImageSupport.cpp)

SET(PIXEL_FORMAT_SOURCE pixel_format.cpp
                        pixel_format.h)

SET(TEXTURE_FILE_CREATER_SOURCE TextureFileCreater.h
                                TextureFileCreater.cpp
                                TextureFileCreaterR.cpp
                                TextureFileCreaterRG.cpp
                                TextureFileCreaterRGB.cpp
                                TextureFileCreaterRGBA.cpp)

set(SOURCE_FILE main.cpp
                ConvertImage.cpp)

set(HEADER_FILE )

SOURCE_GROUP("Image File" FILES ${ILIMAGE_SOURCE})
SOURCE_GROUP("Pixel Format" FILES ${PIXEL_FORMAT_SOURCE})
SOURCE_GROUP("Texture File Creater" FILES ${TEXTURE_FILE_CREATER_SOURCE})

add_executable(TexConv ${SOURCE_FILE} ${ILIMAGE_SOURCE} ${PIXEL_FORMAT_SOURCE} ${TEXTURE_FILE_CREATER_SOURCE})

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
										Qt5::Core Qt5::Gui Qt5::Widgets
										DevIL ILU)

add_executable(ComboTexture ComboTexture.cpp ${ILIMAGE_SOURCE})
target_link_libraries(ComboTexture PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)

#add_executable(YUVTest YUVTest.cpp YUV.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(YUVTest PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)

#add_executable(NormalTest NormalTest.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(NormalTest PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)