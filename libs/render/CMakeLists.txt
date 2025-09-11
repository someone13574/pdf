add_library(render
    src/render.c
    src/graphics_state.c
    src/text_state.c
    src/font.c)
target_include_directories(render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(render PUBLIC arena canvas pdf)
target_link_libraries(render PRIVATE geom pdf-logger sfnt)
target_compile_features(render PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(render PRIVATE -fsanitize=address,undefined)
    target_link_options(render PRIVATE -fsanitize=address,undefined)
endif()
