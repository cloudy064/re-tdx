#include "tdx_date.h"

int tdx_date_valid(uint32_t date) {
    static const unsigned lengths[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned year = date / 10000, month = date / 100 % 100, day = date % 100;
    unsigned maximum;
    if (year < 1 || year > 9999 || month < 1 || month > 12 || day < 1)
        return 0;
    maximum = lengths[month - 1];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        maximum++;
    return day <= maximum;
}

int tdx_date_parse(const char *text, size_t length, int allow_dashes, uint32_t *out) {
    uint32_t date = 0;
    size_t index;
    if (!text || !out || (length != 8 && !(allow_dashes && length == 10)))
        return 0;
    for (index = 0; index < length; ++index) {
        if (length == 10 && (index == 4 || index == 7)) {
            if (text[index] != '-') return 0;
        } else {
            if (text[index] < '0' || text[index] > '9') return 0;
            date = date * 10 + (unsigned)(text[index] - '0');
        }
    }
    if (!tdx_date_valid(date)) return 0;
    *out = date;
    return 1;
}

int tdx_date_days(uint32_t date, long long *out) {
    int year, era;
    unsigned month, day, yoe, mp, doy, doe;
    if (!out || !tdx_date_valid(date)) return 0;
    year = (int)(date / 10000);
    month = date / 100 % 100;
    day = date % 100;
    year -= month <= 2;
    era = year / 400;
    yoe = (unsigned)(year - era * 400);
    mp = month > 2 ? month - 3 : month + 9;
    doy = (153 * mp + 2) / 5 + day - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    *out = (long long)era * 146097 + doe - 719468;
    return 1;
}
