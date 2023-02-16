cmake_minimum_required(VERSION 3.16)
project(SR_WEBCAM LANGUAGES C CXX OBJC)

if(APPLE)
    # separate library created just to build the Objective-C code
    add_library(sr_webcam_mac
        include/sr_webcam.h
        src/sr_webcam_mac.m
        )

    target_include_directories(sr_webcam_mac
        PUBLIC
        include
        )
endif()

add_library(sr_webcam
    include/sr_webcam.h
    src/sr_webcam_internal.h
    src/sr_webcam.c
)

if(APPLE)
    # when building on macOS, also link the Objective-C++ lib.
    target_link_libraries(sr_webcam 
        "-framework Cocoa"
        "-framework AppKit"
        "-framework CoreData"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework Accelerate")

    target_link_libraries(sr_webcam sr_webcam_mac)
endif()