cmake_minimum_required (VERSION 3.12)
 
project(test_decoder)

include_directories("D:/Program Files/ffmpeg-4.2.3/include")
link_directories("D:/Program Files/ffmpeg-4.2.3/lib")

# add_executable (test_decoder  "wasmvideoplayer.cpp")
# target_link_libraries(test_decoder avformat avcodec avutil swscale)



file(GLOB_RECURSE mains RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(mainfile IN LISTS mains)
    # Get file name without directory
    get_filename_component(mainname ${mainfile} NAME_WE)
    add_executable(${mainname} ${mainfile} )
    target_link_libraries(${mainname} avformat avcodec avutil swscale)
endforeach()