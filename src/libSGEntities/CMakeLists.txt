if(POLICY CMP0020)
    CMAKE_POLICY(SET CMP0020 NEW)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/cherish
    ${CMAKE_SOURCE_DIR}/libGUI
    ${CMAKE_SOURCE_DIR}/libSGControls
)

set (libSGEntities_SRCS
    ProtectedGroup.h
    Canvas.h
    Canvas.cpp
    RootScene.h
    RootScene.cpp
    Stroke.h
    Stroke.cpp
    Photo.h
    Photo.cpp
    UserScene.h
    UserScene.cpp
    Entity2D.h
    Entity2D.cpp
    ToolGlobal.h
    ToolGlobal.cpp
    Bookmarks.h
    Bookmarks.cpp
    SelectedGroup.h
    SelectedGroup.cpp
    SceneState.h
    SceneState.cpp
)

add_library(libSGEntities
    STATIC ${libSGEntities_SRCS}
)

target_link_libraries( libSGEntities
    ${QT_LIBRARIES}
    ${OPENSCENEGRAPH_LIBRARIES}
)
