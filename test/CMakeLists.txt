add_executable(overlaymenu_test overlaymenu_test.cpp ../src/overlaymenu.cpp)
target_include_directories(overlaymenu_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_test(NAME overlaymenu_test COMMAND overlaymenu_test)
