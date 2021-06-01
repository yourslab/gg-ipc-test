cmake_minimum_required(VERSION 3.10)
project(gg-ipc-uat)

add_subdirectory("../aws-iot-device-sdk-cpp-v2" "build")

add_executable(
    gg-ipc-uat
        "test.cpp"
)

set_target_properties(gg-ipc-uat PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(
    gg-ipc-uat
    PUBLIC
        GreengrassIpc-cpp    
)
