add_executable(pdf-cli main.c)
target_link_libraries(pdf-cli PRIVATE pdf)
target_compile_definitions(pdf-cli PRIVATE DEBUG TEST)
