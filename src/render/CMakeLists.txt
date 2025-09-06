add_library(render
    render.c
    graphics_state.c
    text_state.c)
target_include_directories(render PUBLIC "${pdf_SOURCE_DIR}/include")
target_link_libraries(render PUBLIC arena canvas pdf)
target_link_libraries(render PRIVATE geom pdf-logger sfnt)
target_compile_features(render PUBLIC c_std_11)
if (NOT MSVC)
    target_compile_options(render PRIVATE -fsanitize=address,undefined)
    target_link_options(render PRIVATE -fsanitize=address,undefined)
endif()
