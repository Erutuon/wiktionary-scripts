#ifndef GET_HEADER_H
#define GET_HEADER_H

#include <stdlib.h>
#include <stdint.h>

#include "utils/string_slice.h"

#define MAX_HEADER_LEVEL 6

typedef uint32_t header_count_t;
typedef uint8_t header_level_t;

str_slice_t get_header (str_slice_t line,
                         header_level_t * header_level);

#endif