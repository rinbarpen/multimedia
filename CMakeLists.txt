cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

project("multimedia" VERSION 0.4 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build ${PROJECT_NAME} with ${CMAKE_BUILD_TYPE}")

if(WIN32)
  set(SYSTEM_NAME "windows")
  add_definitions(-D__WIN__ ON)
  message(STATUS "This is Window platform")
elseif(UNIX)
  set(SYSTEM_NAME "linux")
  add_definitions(-D__LINUX__ ON)
  message(STATUS "This is Linux platform")
else()
  message(STATUS "Unknown platform")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

# ######################################## Requirments Begin ########################################
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googletest/include")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest/googlemock/include")
# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jsoncpp")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jsoncpp/include")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw")
# include_directories("3rdparty/glfw/include")
# ######################################## Requirments End   ########################################

# ######################################## Library Begin ############################################
set(OPENSSL_ROOT_DIR "D:/vcpkg/vcpkg/installed/x64-windows-static")

find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
if(NOT OPENSSL_FOUND)
  message(FATAL_ERROR "OpenSSL not found")
endif()

set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg")
if(WIN32)
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib/${SYSTEM_NAME}/${CMAKE_BUILD_TYPE}")
else()
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib/${SYSTEM_NAME}")
endif()

set(FFMPEG_INCLUDE "${FFMPEG_DIR}/include")
set(FFMPEG_LIBRARIES
  "avcodec" "avformat" "avutil"
  "avdevice" "avfilter" "postproc"
  "swresample" "swscale")
message(STATUS "Load FFmpeg")

set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")
if(WIN32)
  set(SDL_LIBRARIES_DIR "${SDL_DIR}/lib/${SYSTEM_NAME}/x64")
else()
  set(SDL_LIBRARIES_DIR "${SDL_DIR}/lib/${SYSTEM_NAME}")
endif()
set(SDL_INCLUDE "${SDL_DIR}/include")
set(SDL_LIBRARIES "SDL2" "SDL2main")
message(STATUS "Load SDL")


# ######################################## Library End   ############################################

# find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Sql REQUIRED)
file(GLOB
  SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include/multimedia/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include/multimedia/common/*.hpp"
)

if(ENABLE_TEST)
  add_subdirectory(tests)
endif()

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
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES} "src/main.cpp")
target_include_directories(${PROJECT_NAME}
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
  "${FFMPEG_INCLUDE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include"
)
target_link_directories(${PROJECT_NAME}
  PUBLIC
  ${FFMPEG_LIBRARIES_DIR}
)
target_link_libraries(${PROJECT_NAME}
  PUBLIC
  ${PROJECT_LIBS}
)
target_compile_features(${PROJECT_NAME}
  PUBLIC
  cxx_std_17
  c_std_11
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PROJECT_LIB_NAME "${PROJECT_NAME}d")
else()
  set(PROJECT_LIB_NAME "${PROJECT_NAME}")
endif()

if(ENABLE_STATIC)
  add_library(${PROJECT_LIB_NAME} STATIC ${SRC_FILES})
  target_include_directories(${PROJECT_LIB_NAME}
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${FFMPEG_INCLUDE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include"
  )
  target_link_directories(${PROJECT_LIB_NAME}
    PUBLIC
    ${FFMPEG_LIBRARIES_DIR}
  )
  target_link_libraries(${PROJECT_LIB_NAME}
    PUBLIC
    ${PROJECT_LIBS}
  )
  target_compile_features(${PROJECT_LIB_NAME}
    PUBLIC
    cxx_std_17
    c_std_11
  )
elseif(ENABLE_SHARED)
  add_library(${PROJECT_LIB_NAME} SHARED ${SRC_FILES})
  target_include_directories(${PROJECT_LIB_NAME}
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${FFMPEG_INCLUDE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include"
  )
  target_link_directories(${PROJECT_LIB_NAME}
    PUBLIC
    ${FFMPEG_LIBRARIES_DIR}
  )
  target_link_libraries(${PROJECT_LIB_NAME}
    PUBLIC
    ${PROJECT_LIBS}
  )
  target_compile_features(${PROJECT_LIB_NAME}
    PUBLIC
    cxx_std_17
    c_std_11
  )
endif()
