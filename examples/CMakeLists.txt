foreach(example white_noise print_devices sine_wave melody level_meter)
  add_executable("${example}" "${example}.cpp")
  target_link_libraries("${example}" PRIVATE std::audio)
endforeach()
