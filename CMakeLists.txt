cmake_minimum_required(VERSION 3.16)
project(MapMap VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC needs conforming preprocessor for Qt 6.8+ (#warning directive)
if(MSVC)
    add_compile_options(/Zc:preprocessor)
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets
    Xml Network Multimedia Svg
    Core5Compat
)

# Find GStreamer
if(WIN32)
    # Try environment variable first, then fallback paths
    if(DEFINED ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64})
        set(GST_HOME "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}")
        if(NOT EXISTS "${GST_HOME}/include/gstreamer-1.0")
            set(GST_HOME "")
        endif()
    endif()
    if(NOT GST_HOME)
        set(GST_HOME "C:/gstreamer/1.0/msvc_x86_64/1.0/msvc_x86_64")
        if(NOT EXISTS "${GST_HOME}/include/gstreamer-1.0")
            set(GST_HOME "C:/gstreamer/1.0/msvc_x86_64")
            if(NOT EXISTS "${GST_HOME}/include/gstreamer-1.0")
                message(FATAL_ERROR "GStreamer development headers not found.")
            endif()
        endif()
    endif()
    message(STATUS "GStreamer found: ${GST_HOME}")

    set(GST_INCLUDE_DIRS
        ${GST_HOME}/include/gstreamer-1.0
        ${GST_HOME}/include/glib-2.0
        ${GST_HOME}/lib/glib-2.0/include
        ${GST_HOME}/include
    )
    set(GST_LIBRARIES
        ${GST_HOME}/lib/gstapp-1.0.lib
        ${GST_HOME}/lib/gstbase-1.0.lib
        ${GST_HOME}/lib/gstpbutils-1.0.lib
        ${GST_HOME}/lib/gstreamer-1.0.lib
        ${GST_HOME}/lib/gobject-2.0.lib
        ${GST_HOME}/lib/glib-2.0.lib
        ${GST_HOME}/lib/gstaudio-1.0.lib
        ${GST_HOME}/lib/gstvideo-1.0.lib
    )
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED
        gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 gstreamer-pbutils-1.0
    )
elseif(APPLE)
    set(GST_INCLUDE_DIRS /Library/Frameworks/GStreamer.framework/Versions/1.0/Headers)
    set(GST_LIBRARIES "-framework GStreamer")
endif()

# Core sources
set(CORE_SOURCES
    src/core/Commands.cpp
    src/core/CameraImpl.cpp
    src/core/CameraSurface.cpp
    src/core/Element.cpp
    src/core/Mapping.cpp
    src/core/MappingManager.cpp
    src/core/MetaObjectRegistry.cpp
    src/core/MM.cpp
    src/core/Paint.cpp
    src/core/ProjectLabels.cpp
    src/core/ProjectReader.cpp
    src/core/ProjectWriter.cpp
    src/core/Serializable.cpp
    src/core/UidAllocator.cpp
    src/core/VideoImpl.cpp
    src/core/VideoShmSrcImpl.cpp
    src/core/VideoUriDecodeBinImpl.cpp
    src/core/VideoV4l2SrcImpl.cpp
    src/core/Util.cpp
)

set(CORE_HEADERS
    src/core/Commands.h
    src/core/CameraImpl.h
    src/core/CameraSurface.h
    src/core/Element.h
    src/core/Mapping.h
    src/core/MappingManager.h
    src/core/Maths.h
    src/core/MetaObjectRegistry.h
    src/core/MM.h
    src/core/Paint.h
    src/core/ProjectLabels.h
    src/core/ProjectReader.h
    src/core/ProjectWriter.h
    src/core/Serializable.h
    src/core/UidAllocator.h
    src/core/VideoImpl.h
    src/core/VideoShmSrcImpl.h
    src/core/VideoUriDecodeBinImpl.h
    src/core/VideoV4l2SrcImpl.h
    src/core/Util.h
)

# Shape sources
set(SHAPE_SOURCES
    src/shape/Ellipse.cpp
    src/shape/Mesh.cpp
    src/shape/Polygon.cpp
    src/shape/Shape.cpp
)

set(SHAPE_HEADERS
    src/shape/Ellipse.h
    src/shape/Mesh.h
    src/shape/Polygon.h
    src/shape/Quad.h
    src/shape/Shape.h
    src/shape/Shapes.h
    src/shape/Triangle.h
)

# GUI sources
set(GUI_SOURCES
    src/gui/AboutDialog.cpp
    src/gui/ConsoleWindow.cpp
    src/gui/MainWindow.cpp
    src/gui/MapperGLCanvas.cpp
    src/gui/MapperGLCanvasToolbar.cpp
    src/gui/MappingGui.cpp
    src/gui/MappingItemDelegate.cpp
    src/gui/MappingListModel.cpp
    src/gui/OutputGLCanvas.cpp
    src/gui/OutputGLWindow.cpp
    src/gui/PaintGui.cpp
    src/gui/PreferenceDialog.cpp
    src/gui/ShapeControlPainter.cpp
    src/gui/ShapeGraphicsItem.cpp
    src/gui/ShortcutWindow.cpp
)

set(GUI_HEADERS
    src/gui/AboutDialog.h
    src/gui/ConsoleWindow.h
    src/gui/GuiForward.h
    src/gui/MainWindow.h
    src/gui/MapperGLCanvas.h
    src/gui/MapperGLCanvasToolbar.h
    src/gui/MappingGui.h
    src/gui/MappingItemDelegate.h
    src/gui/MappingListModel.h
    src/gui/OutputGLCanvas.h
    src/gui/OutputGLWindow.h
    src/gui/PaintGui.h
    src/gui/PreferenceDialog.h
    src/gui/ShapeControlPainter.h
    src/gui/ShapeGraphicsItem.h
    src/gui/ShortcutWindow.h
)

# QtPropertyBrowser contrib
set(PROPERTYBROWSER_SOURCES
    src/gui/contrib/qtpropertybrowser/src/qtpropertybrowser.cpp
    src/gui/contrib/qtpropertybrowser/src/qtpropertymanager.cpp
    src/gui/contrib/qtpropertybrowser/src/qteditorfactory.cpp
    src/gui/contrib/qtpropertybrowser/src/qtvariantproperty.cpp
    src/gui/contrib/qtpropertybrowser/src/qttreepropertybrowser.cpp
    src/gui/contrib/qtpropertybrowser/src/qtbuttonpropertybrowser.cpp
    src/gui/contrib/qtpropertybrowser/src/qtgroupboxpropertybrowser.cpp
    src/gui/contrib/qtpropertybrowser/src/qtpropertybrowserutils.cpp
)

set(PROPERTYBROWSER_HEADERS
    src/gui/contrib/qtpropertybrowser/src/qtpropertybrowser.h
    src/gui/contrib/qtpropertybrowser/src/qtpropertymanager.h
    src/gui/contrib/qtpropertybrowser/src/qteditorfactory.h
    src/gui/contrib/qtpropertybrowser/src/qtvariantproperty.h
    src/gui/contrib/qtpropertybrowser/src/qttreepropertybrowser.h
    src/gui/contrib/qtpropertybrowser/src/qtbuttonpropertybrowser.h
    src/gui/contrib/qtpropertybrowser/src/qtgroupboxpropertybrowser.h
    src/gui/contrib/qtpropertybrowser/src/qtpropertybrowserutils_p.h
)

# QtPropertyBrowser extension
set(PROPERTYBROWSER_EXT_SOURCES
    src/gui/contrib/qtpropertybrowser-extension/fileedit.cpp
    src/gui/contrib/qtpropertybrowser-extension/filepathmanager.cpp
    src/gui/contrib/qtpropertybrowser-extension/fileeditfactory.cpp
    src/gui/contrib/qtpropertybrowser-extension/variantmanager.cpp
    src/gui/contrib/qtpropertybrowser-extension/variantfactory.cpp
)

set(PROPERTYBROWSER_EXT_HEADERS
    src/gui/contrib/qtpropertybrowser-extension/fileedit.h
    src/gui/contrib/qtpropertybrowser-extension/filepathmanager.h
    src/gui/contrib/qtpropertybrowser-extension/fileeditfactory.h
    src/gui/contrib/qtpropertybrowser-extension/variantmanager.h
    src/gui/contrib/qtpropertybrowser-extension/variantfactory.h
)

# Control/OSC sources
set(CONTROL_SOURCES
    src/control/OscInterface.cpp
    src/control/qosc/contrib/oscpack/OscOutboundPacketStream.cpp
    src/control/qosc/contrib/oscpack/OscPrintReceivedElements.cpp
    src/control/qosc/contrib/oscpack/OscReceivedElements.cpp
    src/control/qosc/contrib/oscpack/OscTypes.cpp
    src/control/qosc/oscreceiver.cpp
    src/control/qosc/oscsender.cpp
)

set(CONTROL_HEADERS
    src/control/ConcurrentQueue.h
    src/control/OscInterface.h
    src/control/qosc/contrib/oscpack/MessageMappingOscPacketListener.h
    src/control/qosc/contrib/oscpack/OscException.h
    src/control/qosc/contrib/oscpack/OscHostEndianness.h
    src/control/qosc/contrib/oscpack/OscOutboundPacketStream.h
    src/control/qosc/contrib/oscpack/OscPacketListener.h
    src/control/qosc/contrib/oscpack/OscPrintReceivedElements.h
    src/control/qosc/contrib/oscpack/OscReceivedElements.h
    src/control/qosc/contrib/oscpack/OscTypes.h
    src/control/qosc/oscreceiver.h
    src/control/qosc/oscsender.h
)

# App sources
set(APP_SOURCES
    src/app/MainApplication.cpp
    src/app/main.cpp
)

set(APP_HEADERS
    src/app/MainApplication.h
)

# Resources
set(RESOURCES
    main.qrc
    translations/translation.qrc
    docs/documentation.qrc
    resources/interface.qrc
    src/gui/contrib/qtpropertybrowser/src/qtpropertybrowser.qrc
)

# Windows resource file
if(WIN32)
    set(WIN_RESOURCES resources/windows_resource.rc)
endif()

# Create executable
add_executable(MapMap WIN32
    ${CORE_SOURCES} ${CORE_HEADERS}
    ${SHAPE_SOURCES} ${SHAPE_HEADERS}
    ${GUI_SOURCES} ${GUI_HEADERS}
    ${PROPERTYBROWSER_SOURCES} ${PROPERTYBROWSER_HEADERS}
    ${PROPERTYBROWSER_EXT_SOURCES} ${PROPERTYBROWSER_EXT_HEADERS}
    ${CONTROL_SOURCES} ${CONTROL_HEADERS}
    ${APP_SOURCES} ${APP_HEADERS}
    ${RESOURCES}
    ${WIN_RESOURCES}
)

# Include directories
target_include_directories(MapMap PRIVATE
    src/core
    src/shape
    src/gui
    src/control
    src/app
    src/gui/contrib/qtpropertybrowser/src
    src/gui/contrib/qtpropertybrowser-extension
    src/control/qosc
    src/control/qosc/contrib/oscpack
    ${GST_INCLUDE_DIRS}
)

# Compile definitions
target_compile_definitions(MapMap PRIVATE
    UNICODE
    QT_THREAD_SUPPORT
    QT_CORE_LIB
    QT_GUI_LIB
    QT_MESSAGELOGCONTEXT
    QT_DISABLE_DEPRECATED_BEFORE=0x040800
    $<$<PLATFORM_ID:Windows>:WIN32 NO_WEBENGINE>
    $<$<PLATFORM_ID:Linux>:UNIX>
    $<$<PLATFORM_ID:Darwin>:MACOSX>
)

if(MSVC)
    target_compile_options(MapMap PRIVATE /wd4996 /wd4267 /wd4244)
    target_compile_definitions(MapMap PRIVATE _USE_MATH_DEFINES)
endif()

# Link libraries
target_link_libraries(MapMap PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Xml
    Qt6::Network
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Core5Compat
    ${GST_LIBRARIES}
)

if(WIN32)
    target_link_libraries(MapMap PRIVATE opengl32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(MapMap PRIVATE GL X11)
elseif(APPLE)
    target_link_libraries(MapMap PRIVATE "-framework OpenGL" "-framework GLUT")
endif()
