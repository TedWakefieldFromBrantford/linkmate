#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <stddef.h>

#define MAX_URL_LENGTH 1024
#define STARTING_CAPACITY 16
#define STOP_BYTE '\0'

typedef struct {
    char url[MAX_URL_LENGTH];
    int line_number;
    int start_column;
    int end_column;
} Link;

typedef struct {
    Link *links;
    size_t count;
    size_t capacity;
} LinkList;

/* Get URLs from text */
int get_urls_from_text(const char *source_text, LinkList *url_container);

/* Free URL list */
void free_url_list(LinkList *url_container);

#endif /* URL_PARSER_H */
