add_executable(isview WIN32 isview/main.cpp)
target_link_libraries(isview PRIVATE isformats gdi32 user32)
target_link_options(isview PRIVATE -municode)
