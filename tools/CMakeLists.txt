add_executable(isview WIN32 isview/main.cpp)
target_link_libraries(isview PRIVATE isformats gdi32 user32 winmm)
if(TARGET isassets)
  target_link_libraries(isview PRIVATE isassets)
endif()
target_link_options(isview PRIVATE -municode)

# Proof-of-concept sandbox; its movement and rules are placeholders (see its header).
add_executable(poc_wave WIN32 poc_wave/main.cpp)
target_link_libraries(poc_wave PRIVATE isformats gdi32 user32 shell32 msimg32 winmm)
target_link_options(poc_wave PRIVATE -municode)
