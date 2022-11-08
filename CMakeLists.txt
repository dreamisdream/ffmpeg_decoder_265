cmake_minimum_required (VERSION 3.12)
 
project(test_decoder)

include_directories("D:/Program Files/ffmpeg-4.2.3/include")
link_directories("D:/Program Files/ffmpeg-4.2.3/lib")

add_executable (test_decoder "test_decoder.cpp")

target_link_libraries(test_decoder avformat avcodec avutil swscale)
