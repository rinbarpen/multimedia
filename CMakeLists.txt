cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

project("multimedia" VERSION 0.4 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build ${PROJECT_NAME} with ${CMAKE_BUILD_TYPE}")

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
# add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw")
# include_directories("3rdparty/glfw/include")
# ######################################## Requirments End   ########################################

# ######################################## Library Begin ############################################
if (WIN32)
  set(OPENSSL_ROOT_DIR "D:/vcpkg/vcpkg/installed/x64-windows-static")
endif()

find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
if(NOT OPENSSL_FOUND)
  message(FATAL_ERROR "OpenSSL not found")
endif()

set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ffmpeg")
if(WIN32)
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib/win")
else()
  set(FFMPEG_LIBRARIES_DIR "${FFMPEG_DIR}/lib/linux")
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
  set(SDL_LIBRARIES_DIR "${SDL_DIR}/lib/win/x86")
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
  "${CMAKE_CURRENT_SOURCE_DIR}/src/multimedia/recorder/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include/multimedia/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include/multimedia/recorder/*.hpp"
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

set(PROJECT_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yaml-cpp/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/fmt/include"
  # "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/glfw/include"
  "${SDL_INCLUDE}"
  "${FFMPEG_INCLUDE}"
)

add_executable(${PROJECT_NAME} ${SRC_FILES} "src/main.cpp")
target_include_directories(${PROJECT_NAME}
PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
  ${PROJECT_INCLUDES}
)
target_link_directories(${PROJECT_NAME}
PRIVATE
  ${FFMPEG_LIBRARIES_DIR}
  ${SDL_LIBRARIES_DIR}
)
target_link_libraries(${PROJECT_NAME}
PRIVATE
  ${PROJECT_LIBS}
)
target_compile_features(${PROJECT_NAME}
PRIVATE
  cxx_std_17
)
