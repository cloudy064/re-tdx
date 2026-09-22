/* tdx_limit.c - the price-limit rules the terminal keeps in hqrule.dat. */
#include "tdx_limit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bias that turns a truncation into round-half-up, and the threshold below which a
 * price counts as absent.  The reference's own constants, reproduced as floats. */
static const double tdx_limit_bias = 0.503000020980835;
static const double tdx_limit_present = 0.00009999999747378752;

/* Storing through a float is not cosmetic: the value a caller compares against a quote has
 * to be the float the terminal would hold. */
static double store_float(double value) {
    return (double)(float)value;
}

double tdx_limit_upper(double previous_close, double rate) {
    float previous = (float)previous_close;
    float native_rate = (float)rate;
    float increment = (float)((double)(int)((double)native_rate * (double)previous * 100.0 +
                                           tdx_limit_bias) /
                              100.0);
    return store_float((double)(int)(((double)previous + (double)increment) * 100.0 +
                                    tdx_limit_bias) /
                       100.0);
}

double tdx_limit_lower(double previous_close, double rate) {
    float previous = (float)previous_close;
    float native_rate = (float)rate;
    return store_float((double)(int)((1.0 - (double)native_rate) * (double)previous * 100.0 +
                                    tdx_limit_bias) /
                       100.0);
}

double tdx_limit_beijing(double previous_close, double rate, int upper) {
    float previous = (float)previous_close;
    float native_rate = (float)rate;
    double factor = upper ? 1.0 + (double)native_rate : 1.0 - (double)native_rate;
    /* Truncation, not round-half-up: a different function for this board. */
    double bias = upper ? 0.003 : 0.997;
    return store_float((double)(int)(factor * (double)previous * 100.0 + bias) / 100.0);
}

static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

int tdx_limit_security_class(int market_id, const char *code) {
    if (!code || strlen(code) != 6)
        return -1;
    if (market_id == 0) {
        if (starts_with(code, "000") || starts_with(code, "001"))
            return 0;
        if (starts_with(code, "002") || starts_with(code, "003") || starts_with(code, "004"))
            return 8;
        if (starts_with(code, "300") || starts_with(code, "301"))
            return 9;
    } else if (market_id == 1) {
        if (starts_with(code, "600") || starts_with(code, "601") ||
            starts_with(code, "603") || starts_with(code, "605"))
            return 11;
        if (starts_with(code, "688") || starts_with(code, "689"))
            return 19;
    } else if (market_id == 2 && starts_with(code, "920")) {
        return 21;
    }
    return -1;
}

int tdx_limit_name_is_st(const char *name) {
    if (!name || !*name)
        return 0;
    /* The reference's rule: the two letters anywhere, or one of several prefixes that
     * signal a corporate action rather than special treatment. */
    if (strstr(name, "ST") != NULL)
        return 1;
    return name[0] == 'S' || starts_with(name, "XDS") || starts_with(name, "XRS") ||
           starts_with(name, "DRS");
}

void tdx_limit_rules_default(tdx_limit_rules *rules) {
    if (!rules)
        return;
    memset(rules, 0, sizeof(*rules));
    rules->chinext_rate = 0.20;
    rules->star_rate = 0.20;
    rules->beijing_rate = 0.30;
    snprintf(rules->source, sizeof(rules->source), "TdxW-defaults");
}

/* Reads one "key=value" line.  Returns 0 for a "[section]" header, a blank line, a
 * comment, or anything else that is not a rule.
 *
 * The file is a plain INI.  An earlier dump of it wrapped each line in brackets because of
 * the formatting chosen while printing, and this parser was first written for
 * "[key=value]" as a result - which found nothing at all in a file that plainly had the
 * keys.  Reading the bytes, not the display, is what settled it. */
static int parse_rule_line(const char *line, size_t length, char *key, size_t key_capacity,
                           char *value, size_t value_capacity) {
    size_t start = 0;
    size_t stop = length;
    size_t equals;
    size_t index;

    while (start < stop && (line[start] == ' ' || line[start] == '\t' ||
                            line[start] == '\r'))
        start++;
    while (stop > start && (line[stop - 1] == ' ' || line[stop - 1] == '\t' ||
                            line[stop - 1] == '\r'))
        stop--;
    if (stop <= start)
        return 0;
    if (line[start] == ';' || line[start] == '#')
        return 0;
    if (line[start] == '[' && line[stop - 1] == ']')
        return 0; /* a section header */
    equals = start;
    while (equals < stop && line[equals] != '=')
        equals++;
    if (equals >= stop)
        return 0; /* a section header such as [RULE], not a rule */
    if (equals - start == 0 || equals - start >= key_capacity)
        return 0;
    for (index = start; index < equals; ++index)
        key[index - start] = line[index];
    key[equals - start] = '\0';
    if (stop - equals - 1 >= value_capacity)
        return 0;
    for (index = equals + 1; index < stop; ++index)
        value[index - equals - 1] = line[index];
    value[stop - equals - 1] = '\0';
    return 1;
}

int tdx_limit_rules_parse(const char *text, size_t length, tdx_limit_rules *rules,
                          tdx_error *err) {
    size_t position = 0;
    size_t applied = 0;

    if (!text || !rules) {
        tdx_error_set(err, "parsing the limit rules needs text and an output");
        return TDX_ERR;
    }
    while (position < length) {
        size_t end = position;
        char key[64];
        char value[64];
        while (end < length && text[end] != '\n')
            end++;
        if (parse_rule_line(text + position, end - position, key, sizeof(key), value,
                            sizeof(value))) {
            if (strcmp(key, "SZST10Date") == 0) {
                rules->sz_st_10_date = atoi(value);
                applied++;
            } else if (strcmp(key, "SHST10Date") == 0) {
                rules->sh_st_10_date = atoi(value);
                applied++;
            } else if (strcmp(key, "CYBZDRatio") == 0) {
                rules->chinext_rate = atof(value);
                applied++;
            } else if (strcmp(key, "KCBZDRatio") == 0) {
                rules->star_rate = atof(value);
                applied++;
            } else if (strcmp(key, "BJBZDRatio") == 0) {
                rules->beijing_rate = atof(value);
                applied++;
            }
        }
        position = end + 1;
    }
    if (applied == 0) {
        tdx_error_set(err, "the rule file holds none of the keys this reader knows");
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_limit_rules_load(const char *root, tdx_limit_rules *rules, tdx_error *err) {
    char path[TDX_LIMIT_RULE_PATH_MAX];
    FILE *stream;
    char text[4096];
    size_t length;
    int applied;

    if (!rules) {
        tdx_error_set(err, "loading the limit rules needs an output");
        return TDX_ERR;
    }
    tdx_limit_rules_default(rules);
    if (snprintf(path, sizeof(path), "%s/T0002/hq_cache/hqrule.dat", root ? root : "") >=
        (int)sizeof(path)) {
        tdx_error_set(err, "the rule path does not fit in %zu bytes", sizeof(path));
        return TDX_ERR;
    }
    stream = fopen(path, "rb");
    if (!stream) {
        /* A terminal without the file still has rules, so this is not a failure. */
        return TDX_OK;
    }
    length = fread(text, 1, sizeof(text) - 1, stream);
    fclose(stream);
    text[length] = '\0';
    /* A file with none of the keys is left as the defaults rather than refused: the
     * terminal falls back the same way. */
    {
        tdx_limit_rules loaded;
        tdx_error ignored;
        tdx_limit_rules_default(&loaded);
        ignored.message[0] = '\0';
        applied = tdx_limit_rules_parse(text, length, &loaded, &ignored);
        if (applied == TDX_OK) {
            snprintf(loaded.source, sizeof(loaded.source), "%s", path);
            *rules = loaded;
        }
    }
    return TDX_OK;
}

int tdx_limit_calculate(int market_id, const char *code, const char *name,
                        double previous_close, int as_of_yyyymmdd,
                        const tdx_limit_rules *rules, tdx_limit_prices *out) {
    tdx_limit_rules fallback;
    double rate = 0.10;
    int security_class;
    int sz_st_class;
    int sh_st_class;

    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!rules) {
        tdx_limit_rules_default(&fallback);
        rules = &fallback;
    }
    security_class = tdx_limit_security_class(market_id, code);
    out->security_class = security_class;
    if (!isfinite(previous_close) || previous_close <= tdx_limit_present ||
        security_class < 0 || (name && name[0] == 'N')) {
        snprintf(out->source, sizeof(out->source), "not-price-limited");
        return 0;
    }
    /* The switch date is what keeps special treatment at 10 per cent while the configured
     * date is in the future: with 99991231 the 5 per cent branch never fires. */
    sz_st_class = security_class == 0 || security_class == 7 || security_class == 8;
    sh_st_class = security_class == 11 || security_class == 18;
    if (((sz_st_class && rules->sz_st_10_date < as_of_yyyymmdd) ||
         (sh_st_class && rules->sh_st_10_date < as_of_yyyymmdd)) &&
        tdx_limit_name_is_st(name)) {
        rate = 0.05;
    } else if (security_class == 9) {
        rate = rules->chinext_rate;
    } else if (security_class == 19) {
        rate = rules->star_rate;
    } else if (security_class == 21) {
        rate = rules->beijing_rate;
    }
    if (!isfinite(rate) || rate <= tdx_limit_present) {
        snprintf(out->source, sizeof(out->source), "disabled-rate");
        return 0;
    }
    out->available = 1;
    out->rate = rate;
    if (security_class == 21) {
        out->upper = tdx_limit_beijing(previous_close, rate, 1);
        out->lower = tdx_limit_beijing(previous_close, rate, 0);
    } else {
        out->upper = tdx_limit_upper(previous_close, rate);
        out->lower = tdx_limit_lower(previous_close, rate);
    }
    snprintf(out->source, sizeof(out->source), "%s",
             rules->source[0] ? rules->source : "local-hqrule");
    return 1;
}
