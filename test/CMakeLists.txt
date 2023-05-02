add_executable(test
        test_main.cpp
        audio_buffer_test.cpp
        audio_device_test.cpp)
target_link_libraries(test PRIVATE std::audio)
