# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("libstreaming")

add_definitions(-DSOCKLEN_T=socklen_t)
add_definitions(-DNO_SSTREAM=1)
add_definitions(-DNEWLOCALE_NOT_USED)
add_definitions(-DBSD=1)

include_directories(include)
include_directories(live/UsageEnvironment/include)
include_directories(live/BasicUsageEnvironment/include)
include_directories(live/groupsock/include)
include_directories(live/liveMedia/include)

file(GLOB BasicUsageEnvironmentFiles
        live/BasicUsageEnvironment/*.cpp)
add_library(BasicUsageEnvironment
        ${BasicUsageEnvironmentFiles})

file(GLOB groupsockfiles
        live/groupsock/*.cpp live/groupsock/*.c)
add_library(groupsock ${groupsockfiles})

file(GLOB liveMediaFiles
        live/liveMedia/*.cpp live/liveMedia/*.c)
add_library(liveMedia ${liveMediaFiles})

file(GLOB UsageEnvironmentFiles
        live/UsageEnvironment/*.cpp)
add_library(UsageEnvironment ${UsageEnvironmentFiles})

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
        AndroidFramedSource.cpp
        libstreaming.cpp)

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        liveMedia
        groupsock
        BasicUsageEnvironment
        UsageEnvironment)