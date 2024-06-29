cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

project("RbpMultimedia" VERSION 1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build ${PROJECT_NAME} with ${CMAKE_BUILD_TYPE}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")

if (MSVC)
  add_compile_options()
elseif(NOT ENABLE_STATIC)
  add_compile_options(-fpic)
endif(MSVC)

# ######################################## Requirments Begin ########################################
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/googletest")
# ######################################## Requirments End   ########################################

# ######################################## Library Begin ############################################
if (WIN32)
  set(OPENSSL_ROOT_DIR "D:/vcpkg/vcpkg/installed/x64-windows-static")
endif()

find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
if(NOT OPENSSL_FOUND)
  message(FATAL_ERROR "OpenSSL not found")
endif()

if(WIN32)
  set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg/win")
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib/x64")
else()
  set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg/linux")
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib")
#  find_package(FFmpeg COMPONENTS SDL2 REQUIRED)
endif()
set(FFMPEG_LIBRARIES
  avcodec
  avformat
  avutil
  avdevice
  avfilter
  postproc
  swresample
  swscale)

set(FFMPEG_INCLUDE "${FFMPEG_DIR}/include")
message(STATUS "Load FFmpeg")

set(SDL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SDL2")
if(WIN32)
  set(SDL_LIBRARIES_DIR "${SDL_DIR}/lib/win/x64")
else()
  set(SDL_LIBRARIES_DIR "${SDL_DIR}/lib/linux")
  find_package(SDL2 COMPONENTS SDL2 REQUIRED)
endif()
set(SDL_LIBRARIES
  SDL2 SDL2main
)
set(SDL_INCLUDE "${SDL_DIR}/include")
message(STATUS "Load SDL")

# ######################################## Library End   ############################################

# find_package(Qt5 REQUIRED COMPONENTS Widgets Gui Core Sql REQUIRED)
file(GLOB
  SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/player/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/recorder/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/device/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/filter/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/io/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/common/*.cpp"
)

add_subdirectory(tests)

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
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include"
  # "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/include"
  "${SDL_INCLUDE}"
  "${FFMPEG_INCLUDE}"
)


if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(RBPLIB_NAME ${PROJECT_NAME}d)
else() 
  set(RBPLIB_NAME ${PROJECT_NAME})
endif()

if (ENABLE_STATIC)
  add_library(${RBPLIB_NAME} STATIC ${SRC_FILES})
else()
  add_library(${RBPLIB_NAME} SHARED ${SRC_FILES})
endif()
target_include_directories(${RBPLIB_NAME}
PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
  ${PROJECT_INCLUDES}
)
target_link_directories(${RBPLIB_NAME}
PRIVATE
  ${FFMPEG_LIBRARIES_DIR}
  ${SDL_LIBRARIES_DIR}
)
target_link_libraries(${RBPLIB_NAME}
PRIVATE
  ${PROJECT_LIBS}
)
target_compile_features(${RBPLIB_NAME}
PRIVATE
  cxx_std_17
)
