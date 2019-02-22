#ifndef GET_HEADER_H
#define GET_HEADER_H

#include <stdlib.h>
#include <stdint.h>

#define MAX_HEADER_LEVEL 6

typedef uint32_t header_count_t;
typedef uint8_t header_level_t;

const char * get_header (const char * line_start,
                         size_t line_len,
                         size_t * header_len,
                         header_level_t * header_level);

#endif