#ifndef GET_HEADER_H
#define GET_HEADER_H

#include <stdlib.h>
#include <stdint.h>

#include "utils/string_slice.h"

#define MAX_HEADER_LEVEL 6

typedef uint32_t header_count_t;

typedef uint8_t header_level_t;

typedef void (*header_line_callback_t) (str_slice_t potential_header_line,
                                        void * data);

void for_each_possible_header_line (str_slice_t slice,
                                    header_line_callback_t callback,
                                    void * data);

str_slice_t get_header (str_slice_t line,
                        header_level_t * header_level);

#endif