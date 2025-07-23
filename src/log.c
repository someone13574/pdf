#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char* level_colors[] = {
    "\x1b[2m", // TRACE
    "\x1b[34m", // DEBUG
    "\x1b[32m", // INFO
    "\x1b[33m", // WARN
    "\x1b[31m", // ERROR
};

static const char* level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};

struct LevelRule {
    bool prefix_matching;
    char* pattern;
    LogLevel level;
};

bool rule_matches(struct LevelRule* rule, const char* group) {
    if (rule->prefix_matching) {
        return strncmp(rule->pattern, group, strlen(rule->pattern)) == 0;
    } else {
        return strcmp(rule->pattern, group) == 0;
    }
}

int parse_level(char* level, LogLevel* out) {
    if (strcasecmp(level, "trace") == 0) {
        *out = LOG_LEVEL_TRACE;
    } else if (strcasecmp(level, "debug") == 0) {
        *out = LOG_LEVEL_DEBUG;
    } else if (strcasecmp(level, "info") == 0) {
        *out = LOG_LEVEL_INFO;
    } else if (strcasecmp(level, "warn") == 0) {
        *out = LOG_LEVEL_WARN;
    } else if (strcasecmp(level, "error") == 0) {
        *out = LOG_LEVEL_ERROR;
    } else {
        return -1;
    }

    return 0;
}

int parse_rules(const char* text, struct LevelRule* rules, int num_rules) {
    int parsed_rules = 0;

    char* copy = strdup(text);
    char* save_ptr;
    char* rule = strtok_r(copy, ",", &save_ptr);

    while (rule) {
        if (rules) {
            assert(parsed_rules < num_rules);

            char level_str[6];
            char pattern_str[64];

            int x = sscanf(rule, "%63[^=]=%5s", pattern_str, level_str);

            if (x != 2) {
                free(copy);
                return -1;
            }

            unsigned long pattern_len = strlen(pattern_str);
            if (pattern_len == 0) {
                free(copy);
                return -1;
            }

            rules[parsed_rules].prefix_matching =
                pattern_str[pattern_len - 1] == '*';
            if (rules[parsed_rules].prefix_matching) {
                pattern_str[pattern_len - 1] = '\0';
            }

            rules[parsed_rules].pattern = strdup(pattern_str);
            if (!rules[parsed_rules].pattern) {
                free(copy);
                return -1;
            }

            if (parse_level(level_str, &rules[parsed_rules].level) != 0) {
                free(copy);
                return -1;
            }
        }

        parsed_rules += 1;
        rule = strtok_r(NULL, ",", &save_ptr);
    }

    free(copy);

    if (!rules || parsed_rules == num_rules) {
        return parsed_rules;
    }

    return -1;
}

static int num_logger_rules = 0;
static struct LevelRule* logger_rules = NULL;

void logger_init(void) {
    if (logger_rules) {
        return;
    }

    const char* env = getenv("PDF_LOG_LEVEL");

    if (!env) {
        env = "*=debug,vec=info,array=info,arena=info";
    }

    num_logger_rules = parse_rules(env, NULL, 0);
    if (num_logger_rules < 0) {
        printf("Invalid logging rule set.\n");
        exit(EXIT_FAILURE);
    } else if (num_logger_rules == 0) {
        printf("Must have at least one logging rule.\n");
        exit(EXIT_FAILURE);
    }

    logger_rules =
        calloc((unsigned long)num_logger_rules, sizeof(struct LevelRule));
    if (parse_rules(env, logger_rules, num_logger_rules) < 0) {
        printf("Invalid logging rule set.\n");
        exit(EXIT_FAILURE);
    }

    logger_log(
        0,
        "",
        LOG_LEVEL_INFO,
        RELATIVE_FILE_PATH,
        __LINE__,
        "Logging rules: `%s`",
        env
    );
}

void logger_log(
    int check_level,
    const char* group,
    LogLevel level,
    const char* file,
    unsigned long line,
    const char* fmt,
    ...
) {
    logger_init();

    if (check_level) {
        LogLevel filter_level = LOG_LEVEL_WARN;
        for (int rule_idx = num_logger_rules - 1; rule_idx >= 0; rule_idx--) {
            if (rule_matches(&logger_rules[rule_idx], group)) {
                filter_level = logger_rules[rule_idx].level;
                break;
            }
        }

        if (level < filter_level) {
            return;
        }
    }

    bool display_group = strlen(group) != 0;

    int level_pad = 5 - (int)strlen(level_names[level]);
    int file_and_group_len =
        (int)strlen(file) + (int)strlen(group) + (display_group ? 3 : 0);

    if (line < 10) {
        file_and_group_len += 1;
    } else if (line < 100) {
        file_and_group_len += 2;
    } else if (line < 1000) {
        file_and_group_len += 3;
    } else if (line < 10000) {
        file_and_group_len += 4;
    }

    int file_pad_available = 48;
    int file_pad = 0;

    do {
        file_pad = file_pad_available - file_and_group_len;
        file_pad_available += 16;
    } while (file_pad < 0);

    if (display_group) {
        printf(
            "\x1b[4m%s:%lu\x1b[0m \x1b[2m(%s)\x1b[0m %*s %s[%s]\x1b[0m%*s ",
            file,
            line,
            group,
            file_pad,
            "",
            level_colors[level],
            level_names[level],
            level_pad,
            ""
        );
    } else {
        printf(
            "\x1b[4m%s:%lu\x1b[0m %*s %s[%s]\x1b[0m%*s ",
            file,
            line,
            file_pad,
            "",
            level_colors[level],
            level_names[level],
            level_pad,
            ""
        );
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
