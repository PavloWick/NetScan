#include "oui.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t prefix;
    int bits;
    char *vendor;
} OuiEntry;

static OuiEntry *g_table = NULL;
static size_t g_count = 0;
static size_t g_capacity = 0;

static uint64_t prefix_mask(int bits) {
    if(bits == 48) {
        return UINT64_C(0xffffffffffff);
    }
    return (UINT64_C(0xffffffffffff) << (48 - bits)) & UINT64_C(0xffffffffffff);
}

static int compare_oui_entries(const void *a, const void *b) {
    const OuiEntry *left = a;
    const OuiEntry *right = b;

    if(left->prefix < right->prefix) return -1;
    if(left->prefix > right->prefix) return 1;
    if(left->bits < right->bits) return -1;
    if(left->bits > right->bits) return 1;
    return 0;
}

static char *dup_string(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if(copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static int hex_value(int c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_prefix(const char *text, uint64_t *value, int *bits) {
    uint64_t parsed = 0;
    int nibbles = 0;
    int parsed_bits = 24;

    for(const char *p = text; *p; p++) {
        if(*p == '/') {
            parsed_bits = atoi(p + 1);
            break;
        }

        int v = hex_value((unsigned char)*p);
        if(v < 0) continue;
        if(nibbles == 12) return -1;

        parsed = (parsed << 4) | (uint64_t)v;
        nibbles++;
    }

    if(nibbles == 0 || parsed_bits <= 0 || parsed_bits > 48) {
        return -1;
    }

    parsed <<= 4 * (12 - nibbles);
    *value = parsed;
    *bits = parsed_bits;
    return 0;
}

static char *trim(char *s) {
    while(isspace((unsigned char)*s)) s++;

    char *end = s + strlen(s);
    while(end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

static int add_entry(uint64_t prefix, int bits, const char *vendor) {
    if(g_count == g_capacity) {
        size_t new_capacity = g_capacity == 0 ? 1024 : g_capacity * 2;
        OuiEntry *new_table = realloc(g_table, sizeof(*g_table) * new_capacity);
        if(!new_table) {
            return -1;
        }
        g_table = new_table;
        g_capacity = new_capacity;
    }

    g_table[g_count].prefix = prefix;
    g_table[g_count].bits = bits;
    g_table[g_count].vendor = dup_string(vendor);
    if(!g_table[g_count].vendor) {
        return -1;
    }
    g_count++;
    return 0;
}

int oui_load(const char *path) {
    FILE *file = fopen(path, "r");
    if(!file) {
        return -1;
    }

    oui_free();

    char line[512];
    while(fgets(line, sizeof(line), file)) {
        char *cursor = trim(line);
        if(*cursor == '\0' || *cursor == '#') continue;

        char prefix_text[64];
        char short_name[128];
        char full_name[256];
        full_name[0] = '\0';

        int matched = sscanf(cursor, "%63s %127s %255[^\n]",
                             prefix_text, short_name, full_name);
        if(matched < 2) continue;

        uint64_t prefix;
        int bits;
        if(parse_prefix(prefix_text, &prefix, &bits) != 0) continue;

        char *vendor = matched == 3 ? trim(full_name) : short_name;
        if(add_entry(prefix, bits, vendor) != 0) {
            fclose(file);
            oui_free();
            return -1;
        }
    }

    fclose(file);
    qsort(g_table, g_count, sizeof(*g_table), compare_oui_entries);
    return 0;
}

const char *oui_lookup(const char *mac) {
    uint64_t value;
    int bits;
    if(parse_prefix(mac, &value, &bits) != 0) {
        return NULL;
    }

    for(int prefix_bits = 48; prefix_bits > 0; prefix_bits--) {
        OuiEntry key = {
            .prefix = value & prefix_mask(prefix_bits),
            .bits = prefix_bits,
            .vendor = NULL
        };
        OuiEntry *match = bsearch(&key, g_table, g_count, sizeof(*g_table),
                                  compare_oui_entries);
        if(match) {
            return match->vendor;
        }
    }

    return NULL;
}

void oui_free(void) {
    for(size_t i = 0; i < g_count; i++) {
        free(g_table[i].vendor);
    }
    free(g_table);
    g_table = NULL;
    g_count = 0;
    g_capacity = 0;
}
