#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char browser[64];
    char theme[32];
    int history_enabled;
    char history_file[256];
} Config;

Config load_config(void);

#endif /* CONFIG_H */
