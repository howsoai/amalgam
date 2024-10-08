#pragma once

//system headers:
#include <string>

//sets the path of the timezone database (from http://www.iana.org/time-zones )
// used for time zones.  If no path is specified, it will look in default locations
//returns the path used
std::string SetTimeZoneDatabasePath(std::string path = "");

//parses datetime based on format and locale and returns the number of seconds from "Epoch" (January 1, 1970)
double GetNumSecondsSinceEpochFromDateTimeString(const std::string &datetime_str, std::string format, std::string locale, std::string timezone);

//transforms seconds_since_epoch into the datetime string specified by format and locale based on "Epoch" (January 1, 1970)
// positive and negative values of num_secs_from_epoc are allowed
std::string GetDateTimeStringFromNumSecondsSinceEpoch(double seconds_since_epoch, std::string format, const std::string &locale, const std::string &timezone);

//parses time_str based on format and locale and returns the number of seconds since midnight
double GetNumSecondsSinceMidnight(const std::string &time_str, std::string format, std::string locale);

//transfroms seconds_since_midnight into a string representing the time of day
std::string GetTimeStringFromNumSecondsSinceMidnight(double seconds_since_midnight, std::string format, std::string locale);
