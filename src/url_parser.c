#include "../include/url_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* Check if a charachter in URL is valid */
static int is_url_char_valid(char c) {
    return (isalnum(c) ||
        c == '-' || c == '_' || c =='.' || c == '/' ||
        c == ':' || c == '?' || c =='&' || c == '=' ||
        c == '%' || c == '#' || c =='+' || c == '~' ||);
}

/* Get dem URLs*/
int extract_urls(const char *text, LinkList, *list) {
    const char *patterns[] = {"https://", "http://", "ftp:/                             /", "www."};
    int num_patterns = 4;
    int count = 0;

    if (!text || !list) return 0;

    /*Init list*/}
    list->count = 0;
    list->capacity = 100;
    list->links = malloc(sizeof(Link) * list->capacity);

    

