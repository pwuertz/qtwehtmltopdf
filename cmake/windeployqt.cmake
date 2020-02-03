find_package(Qt5 COMPONENTS Core REQUIRED)

# Get qmake path, find windeployqt in qt binary dir
get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
if(NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "Windeployqt tool not found")
endif()

function(windeployqt target)
    cmake_parse_arguments("DEPLOY_ARG" "" "QMLDIR" "" ${ARGN})
    list(APPEND WINDEPLOYQT_ARGS
        "--verbose" "0" "--no-compiler-runtime" "--no-translations"
        "--no-angle" "--no-opengl-sw"
    )
    if(DEPLOY_ARG_QMLDIR)
        list(APPEND WINDEPLOYQT_ARGS "--qmldir" "${DEPLOY_ARG_QMLDIR}")
    endif()
    add_custom_command(TARGET ${target}
        POST_BUILD
        COMMAND set PATH="${_qt_bin_dir}"
        COMMAND ${CMAKE_COMMAND} -E echo "Deploy Qt for ${target}"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" ARGS ${WINDEPLOYQT_ARGS} $<$<CONFIG:Release>:"--release"> "$<TARGET_FILE:${target}>"
    )
endfunction()
