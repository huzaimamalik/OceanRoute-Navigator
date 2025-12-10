#pragma once

#include <string>

using namespace std;

struct Date {
 int day;
 int month;
 int year;
};

struct Time {
 int hour;
 int minute;
};

Date parseDate(const string &s);

Time parseTime(const string &s);

int compareDate(const Date &a, const Date &b);

int compareTime(const Time &a, const Time &b);
