/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ted Wakefield
 */

#ifndef HISTORY_H
#define HISTORY_H

void add_to_history(const char *url);
char **get_history(void);
int get_history_count(void);
void clear_history(void);

#endif /* HISTORY_H */
