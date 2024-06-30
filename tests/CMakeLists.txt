file(GLOB
  SRC_FILES
  "../src/multimedia/recorder/*.cpp"
  "../src/multimedia/device/*.cpp"
  "../src/multimedia/filter/*.cpp"
  "../src/multimedia/io/*.cpp"
  "../src/multimedia/common/*.cpp"
  "../src/multimedia/player/*.cpp"
)
set(PROJECT_LIBS
  fmt::fmt
  yaml-cpp::yaml-cpp
  OpenSSL::SSL
  OpenSSL::Crypto
  ${FFMPEG_LIBRARIES}
  ${SDL_LIBRARIES}
)
if(WIN32)
  list(APPEND PROJECT_LIBS "D3D9.lib")
elseif(LINUX)
  list(APPEND PROJECT_LIBS "libX11.so")
endif()

set(PROJECT_INCLUDES
  "../3rdparty/yaml-cpp/include"
  "../3rdparty/fmt/include"
  # "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/include"
  "${SDL_INCLUDE}"
  "${FFMPEG_INCLUDE}"
)

function (add_test_project test_name test_src)
    add_executable(${test_name} ${SRC_FILES} ${test_src})
    target_include_directories(${test_name}
    PRIVATE
        "../src/include"
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
        # gtest::gtest
        # gtest::gtest_main
    )
    target_compile_features(${test_name}
    PRIVATE
        cxx_std_17
    )
    # add_test(NAME ${test_name} COMMAND "")
endfunction()

add_test_project(play_camera multimedia/play_camera.cpp)
add_test_project(play_media multimedia/play_media.cpp)
add_test_project(play_screen_capture multimedia/play_screen_capture.cpp)
