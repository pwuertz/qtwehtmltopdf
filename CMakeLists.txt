cmake_minimum_required(VERSION 3.13)

project("qtwehtmltopdf" VERSION 1.0 LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Qt5 COMPONENTS Core WebEngine WebEngineWidgets PrintSupport REQUIRED)

# Application
add_executable(${PROJECT_NAME} "main.cpp")
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::WebEngine
    Qt5::WebEngineWidgets
    Qt5::PrintSupport
)

# Optionally deploy Qt runtime
option(WITH_DEPENDENCIES "Deploy Qt runtime files" OFF)
if(WITH_DEPENDENCIES)
    include(windeployqt)
    windeployqt(${PROJECT_NAME})
endif()
