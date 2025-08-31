#include "text_state.h"

#include "geom/mat3.h"

TextState text_state_default() {
    return (TextState
    ) {.character_spacing = 0.0, .word_spacing = 0.0, .horizontal_spacing = 1.0
    };
}

TextObjectState text_object_state_default() {
    return (TextObjectState
    ) {.text_matrix = geom_mat3_identity(),
       .text_line_matrix = geom_mat3_identity()};
}
