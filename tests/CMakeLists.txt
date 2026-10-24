add_executable(is_tests
  test_main.cpp
  test_bytes.cpp
  test_ne.cpp
  test_res16.cpp
  test_dib.cpp
  test_golden.cpp)
target_link_libraries(is_tests PRIVATE isformats)
target_include_directories(is_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest ${CMAKE_CURRENT_SOURCE_DIR})

# Golden tests read the original files and the isre outputs. Under Wine, drive Z: is /.
if(CMAKE_CROSSCOMPILING)
  set(_z "Z:")
endif()
add_test(NAME is_tests COMMAND is_tests)
set_tests_properties(is_tests PROPERTIES ENVIRONMENT
  "IS_ORIGINAL_DIR=${_z}${IS_ORIGINAL_DIR};IS_ASSETS_DIR=${_z}${IS_ASSETS_DIR}")
