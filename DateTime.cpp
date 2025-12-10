

#include <iostream>
#include "DateTime.h"
#include <cstdlib>

using namespace std;

static int toInt(const string &s, size_t start, size_t len) {
 return atoi(s.substr(start, len).c_str());
}

Date parseDate(const string &s) {
 Date d{0,0,0};

 size_t first = s.find('/');
 size_t second = s.find('/', first +1);
 if (first == string::npos || second == string::npos) return d;
 d.day = atoi(s.substr(0, first).c_str());
 d.month = atoi(s.substr(first +1, second - first -1).c_str());
 d.year = atoi(s.substr(second +1).c_str());
 return d;
}

Time parseTime(const string &s) {
 Time t{0,0};

 size_t colon = s.find(':');
 if (colon == string::npos) return t;
 t.hour = atoi(s.substr(0, colon).c_str());
 t.minute = atoi(s.substr(colon +1).c_str());
 return t;
}

int compareDate(const Date &a, const Date &b) {
 if (a.year != b.year) return a.year < b.year ? -1 : 1;
 if (a.month != b.month) return a.month < b.month ? -1 : 1;
 if (a.day != b.day) return a.day < b.day ? -1 : 1;
 return 0;
}

int compareTime(const Time &a, const Time &b) {
 if (a.hour != b.hour) return a.hour < b.hour ? -1 : 1;
 if (a.minute != b.minute) return a.minute < b.minute ? -1 : 1;
 return 0;
}
