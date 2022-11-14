cmake_minimum_required (VERSION 3.12)
 
project(test_decoder)

include_directories("D:/Program Files/ffmpeg-4.2.3/include")
link_directories("D:/Program Files/ffmpeg-4.2.3/lib")

add_executable (test_decoder  "test_decoder_queue.c")
target_link_libraries(test_decoder avformat avcodec avutil swscale)

set(OpenCV_DIR "D:/Program Files/opencv/build")
find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
	target_include_directories(test_decoder PUBLIC ${OpenCV_INCLUDE_DIR})
	target_link_libraries(test_decoder ${OpenCV_LIBS})
endif()