
function (add_test_project test_name test_src)
    add_executable(${test_name} ${SRC_FILES} ${test_src})
    target_include_directories(${test_name}
    PRIVATE
        ${PROJECT_INCLUDES}
    )
    target_link_directories(${test_name}
    PRIVATE
        ${FFMPEG_LIBRARIES_DIR}
        ${SDL_LIBRARIES_DIR}
    )
    target_link_libraries(${test_name}
    PRIVATE
        ${PROJECT_LIBS}
        gtest::gtest
        gtest::gtest_main
    )
    target_compile_features(${test_name}
    PRIVATE
        cxx_std_17
    )
    add_test(NAME ${test_name} COMMAND "")
endfunction()

add_test_project(test_SDL multimedia/test_SDL.cpp)
