cmake_minimum_required(VERSION 3.16)
project(AssignmentManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set CMAKE_PREFIX_PATH to include MSYS2 mingw64 directory
list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")

# Find Qt6 packages
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network PrintSupport)

# Manually specify Poppler paths
set(Poppler_INCLUDE_DIR "C:/msys64/mingw64/include/poppler/qt6")
set(Poppler_LIBRARY "C:/msys64/mingw64/lib/libpoppler-qt6.dll.a")

# Ensure Poppler headers are found
include_directories(${Poppler_INCLUDE_DIR})

# Ensure Poppler library is linked
link_directories("C:/msys64/mingw64/lib")

# Source files
# set(PROJECT_SOURCES
#     main.cpp
#     Dashboard.cpp
#     Dashboard.h
#     Dashboard.ui
#     resource.qrc
# )
set(PROJECT_SOURCES
    main.cpp
    Dashboard.cpp
    Dashboard.h
    Dashboard.ui
    GoogleAuthManager.cpp
    GoogleAuthManager.h
    resource.qrc
)

# Create the executable
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AssignmentManager
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        googleauthmanager.h googleauthmanager.cpp
        googledriveservice.h googledriveservice.cpp
        plagiarismdetector.h plagiarismdetector.cpp
    )
else()
    add_executable(AssignmentManager ${PROJECT_SOURCES})
endif()

# Include directories and link libraries
target_include_directories(AssignmentManager PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(AssignmentManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::PrintSupport
    ${Poppler_LIBRARY}
)

set_target_properties(AssignmentManager PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS AssignmentManager
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(${QT_VERSION_MAJOR} EQUAL 6)
    qt_finalize_executable(AssignmentManager)
endif()
