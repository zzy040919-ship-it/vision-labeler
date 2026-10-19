cmake_minimum_required(VERSION 3.21)
project(VisionLabeler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc videoio)

add_executable(vision-labeler main.cpp)
target_link_libraries(vision-labeler PRIVATE Qt6::Widgets ${OpenCV_LIBS})
target_include_directories(vision-labeler PRIVATE ${OpenCV_INCLUDE_DIRS})
