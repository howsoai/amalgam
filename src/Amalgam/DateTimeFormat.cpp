//project headers:
#include "Concurrency.h"
#include "DateTimeFormat.h"

#include "PlatformSpecific.h"

//3rd party headers:
#include "date/date.h"
#include "date/tz.h"

//thread local locales and string streams to prevent std::ostringstream or std::locale from reducing concurrency
//by their global locks
class CachedLocale
{
public:

	void UpdateLocaleIfNeeded(std::string new_locale_name)
	{
		if(localeName != new_locale_name)
		{
			localeName = new_locale_name;
			locale = std::locale(new_locale_name);
		}
		stringStream.imbue(locale);
	}

	//clears flag and contents of stringStream
	void ResetStringStream(std::string new_value = std::string())
	{
		stringStream.clear();
		stringStream.str(new_value);

		//only want to initialize this once
		stringStream.imbue(defaultLocale);
	}

	std::stringstream stringStream;
	std::locale locale;

protected:
	std::string localeName;

	//a global default locale used to reset stringStream
	const static std::locale defaultLocale;
};

const std::locale CachedLocale::defaultLocale;

std::string SetTimeZoneDatabasePath(std::string path)
{
	//only do this on Windows -- other operating systems use the built-in timezone database
#ifdef OS_WINDOWS
	if(path.empty())
	{
		std::string error;
		if(Platform_IsResourcePathAccessible("./tzdata", true, error))
		{
			path = "./tzdata";
		}
		else if(Platform_IsResourcePathAccessible(Platform_GetHomeDirectory() + "/.amalgam/tzdata", true, error))
		{
			path = Platform_GetHomeDirectory() + "/.amalgam/tzdata";
		}
		else if(Platform_IsResourcePathAccessible(Platform_GetHomeDirectory() + "/.howso/tzdata", true, error))
		{
			path = Platform_GetHomeDirectory() + "/.howso/tzdata";
		}
		else
		{
			std::cerr << "Warning: Could not find time zone database (tzdata directory) in: current working directory, ~/.amalgam, or ~/.howso.\n";
			std::cerr << "Instead, all dates will use UTC.\n";
			std::cerr << "The tzdata directory should contain the files from https://www.iana.org/time-zones and https://github.com/unicode-org/cldr/blob/master/common/supplemental/windowsZones.xml .\n";
		}
	}

	if(!path.empty())
		date::set_install(path);
#endif

	return path;
}

// return true if format is year-month or month-year, where year is %Y and month is one of %m, %b, %B or %h, with any single character separator
inline bool IsFormatMonthAndYearOnly(const std::string &s)
{
	//expected formats should be length of 5, e.g., "%m-%Y"
	if(s.size() != 5) 
		return false;

	if(s[1] == 'Y' && (s[4] == 'm' || s[4] == 'b' || s[4] == 'B' || s[4] == 'h'))
		return true;

	if(s[4] == 'Y' && (s[1] == 'm' || s[1] == 'b' || s[1] == 'B' || s[1] == 'h'))
		return true;
	
	return false;
}

//edits s in place to make sure that it has only valid format specifiers (%'s as specified by the strftime standards)
// if a format specifier is invalid, it will eliminate it and preserve the  underlying characters
// returns true if the format string contains a %z offset
inline bool ConstrainDateTimeStringToValidFormat(std::string &s)
{
	bool has_time_offset = false;

	for(size_t index = 0; index < s.size(); index++)
	{
		//skip over non-specifier characters
		if(s[index] != '%')
			continue;

		//specifiers need to have at least one character afterward
		if(index + 1 >= s.size())
		{
			//get rid of the % character and finish
			s.pop_back();
			break;
		}

		switch(s[index + 1])
		{
		//valid single-character format specifiers
		case 'z':
		{
			index++;
			has_time_offset = true;
			continue;
		}

		case '%':	case 'a':	case 'A':	case 'b':	case 'B':	case 'C':	case 'c':	case 'd':	case 'D':	case 'e':	case 'F':	case 'G':
		case 'g':	case 'h':	case 'H':	case 'I':	case 'j':	case 'm':	case 'M':	case 'n':	case 'p':	case 'r':	case 'R':	case 'S':
		case 't':	case 'T':	case 'U':	case 'u':	case 'V':	case 'W':	case 'w':	case 'x':	case 'X':	case 'Y':	case 'y':	case 'Z':
		{
			index++;
			continue;
		}

		//valid double-character format specifiers
		case 'E':
		{
			if(index + 2 >= s.size())
			{
				//remove all remaining including the format specifier
				s.erase(begin(s) + index, end(s));
				continue;
			}

			switch(s[index + 2])
			{
			case 'C':	case 'c':	case 'x':	case 'X':	case 'Y':	case 'y':
			{
				index += 2;
				continue;
			}
			default:
				break;
			}

			break;
		}

		case 'O':
		{
			if(index + 2 >= s.size())
			{
				//remove all remaining including the format specifier
				s.erase(begin(s) + index, end(s));
				continue;
			}

			switch(s[index + 2])
			{
			case 'd':	case 'e':	case 'H':	case 'I':	case 'm':	case 'M':	case 'S':
			case 'U':	case 'u':	case 'V':	case 'W':	case 'w':	case 'y':
			{
				index += 2;
				continue;
			}
			default:
				break;
			}

			break;
		}

		//invalid format specifier, replace % with a space
		default:
			s[index] = ' ';
			break;
		}

	}

	return has_time_offset;
}


//finds a vector of time zones corresponding to the abbreviation
// from https://github.com/HowardHinnant/date/wiki/Examples-and-Recipes#convert_by_timezone_abbreviation
template<class Duration>
std::vector<date::zoned_time<std::common_type_t<Duration, std::chrono::seconds>>> FindZonesByAbbrev(date::sys_time<Duration> tp, const std::string &abbrev)
{
	using namespace std::chrono;
	using namespace date;
	std::vector<zoned_time<std::common_type_t<Duration, std::chrono::seconds>>> results;
	auto &db = get_tzdb();
	for(auto &z : db.zones)
	{
		if(z.get_info(tp).abbrev == abbrev)
			results.emplace_back(&z, tp);
	}
	return results;
}

//returns the time_zone corresponding with the string timezone
// if timezone is an abbreviation, it will only select a timezone if it is a unique timezone corresponding to the abbreviation
const date::time_zone *GetTimeZoneFromString(const std::string &timezone)
{
	// if timezone wasn't specified, return local timezone
	if(timezone.empty())
		return date::current_zone();

	const date::time_zone *tz;
	try
	{
		tz = date::locate_zone(timezone);
	}
	catch(...)
	{
		// search DB of timezones to find one that may match this abbreviation - note the first time this is run it may take several seconds
		// pass in 'now' as the timestamp, we only need the list of possible matching timezones, timestamp is not relevant
		auto now = std::chrono::system_clock::now();
		auto results = FindZonesByAbbrev(now, timezone);

		//only use the time zone if there is a unique corresponding timezone
		if(results.size() == 1)
			tz = results[0].get_time_zone();
		else //if can't find anything or ambiguous, so use current timezone
			tz = date::current_zone();
	}

	return tz;
}


//don't pass locale by reference so can default it
double GetNumSecondsSinceEpochFromDateTimeString(const std::string &datetime_str, std::string format, std::string locale, std::string timezone)
{
	bool has_time_offset = ConstrainDateTimeStringToValidFormat(format);

	std::chrono::system_clock::time_point dt;
	std::string in_date_timezone = "";

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		static CachedLocale cached_locale;

	cached_locale.ResetStringStream(datetime_str);

	if(!locale.empty())
	{
		//make sure it's utf-8
		locale += ".utf-8";
		//if the locale is valid, use it
		try
		{
			cached_locale.UpdateLocaleIfNeeded(locale);
		}
		catch(...)
		{
		}
	}

	try
	{
		if(IsFormatMonthAndYearOnly(format))
		{
			//month and year only dates must be parsed specifically into year_month 
			date::year_month ym;
			cached_locale.stringStream >> date::parse(format, ym, in_date_timezone);
			//convert to time_point by specifying the day to be 1 for the parsed year month
			dt = date::sys_days{ ym / 1 };
		}
		else
		{
			//parse string into dt and if there was a timezone in the string, stores that into in_date_timezone
			cached_locale.stringStream >> date::parse(format, dt, in_date_timezone);
		}
	}
	catch(...)
	{
	}

	//overwrite the passed-in timezone if one was parsed out of the datetime string
	if(!in_date_timezone.empty())
		timezone = in_date_timezone;
	//if there is no timezone defined, but the format has a time offset provided via %z, assume the offset is UTC
	else if(has_time_offset)
		timezone = "UTC";

	const date::time_zone* t_z = GetTimeZoneFromString(timezone);

	// convert parsed date to the specified timezone
	auto zoned_datetime = date::make_zoned(t_z, dt);

	// calculate the difference in seconds between UTC and the specified time zone
	int64_t diff = std::chrono::duration_cast<std::chrono::seconds>(zoned_datetime.get_sys_time().time_since_epoch()).count() -
		std::chrono::duration_cast<std::chrono::seconds>(zoned_datetime.get_local_time().time_since_epoch()).count();

	//add the difference to the parsed date time, resulting in in a datetime that's the specified time at UTC
	//eg: if 10:00:00 was passed in without a time zone, and if local zone is EST, it's actually +5 hours, so 15:00:00 UTC.
	dt += std::chrono::seconds(diff);

	//output seconds while keeping original precision by casting the value to microseconds and then dividing by 1000000.0
	return std::chrono::duration_cast<std::chrono::microseconds>(dt.time_since_epoch()).count() / 1000000.0;
}


//converts a datetime time point into a string specified by format, locale, and time zone t_z
// templated so it will properly cast the TimepointType and round to the appropriate number of digits
// locale is not specified as constant or passed reference because this function may modify the string
//  and needs to call the copy constructor
template<typename TimepointType>
std::string ConvertZonedDateTimeToString(TimepointType datetime, const std::string &format, std::string locale, const date::time_zone *tz)
{
	auto zoned_dt = date::make_zoned(tz, datetime);

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		static CachedLocale cached_locale;

	cached_locale.ResetStringStream();
	if(locale.empty())
	{
		try
		{
			cached_locale.stringStream << date::format(format, zoned_dt);
		}
		catch(...)
		{
			//can't emit anything
		}
	}
	else
	{
		//make sure it's utf-8
		locale += ".utf-8";
		//if the locale is valid, use it
		try
		{
			cached_locale.UpdateLocaleIfNeeded(locale);
			cached_locale.stringStream << date::format(cached_locale.locale, format, zoned_dt);
		}
		catch(...)
		{
			try
			{
				cached_locale.stringStream << date::format(format, zoned_dt);
			}
			catch(...)
			{
				//can't emit anything
			}
		}
	}

	return cached_locale.stringStream.str();
}

//format and locale are not passed by reference because both need a copy
std::string GetDateTimeStringFromNumSecondsSinceEpoch(double seconds_since_epoch, std::string format, const std::string &locale, const std::string &timezone)
{
	if(seconds_since_epoch != seconds_since_epoch
			|| seconds_since_epoch == std::numeric_limits<double>::infinity()
			|| seconds_since_epoch == -std::numeric_limits<double>::infinity())
		seconds_since_epoch = 0.0;

	bool has_time_offset = ConstrainDateTimeStringToValidFormat(format);

	bool has_fractional_seconds = (seconds_since_epoch != static_cast<int64_t>(seconds_since_epoch));

	std::chrono::system_clock::time_point datetime;
	datetime = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(static_cast<double>(seconds_since_epoch))));

	//if there is no timezone defined, but the format has a time offset provided via %z assume the offset is UTC
	const date::time_zone *tz = nullptr;
	if(timezone.empty() && has_time_offset)
		tz = GetTimeZoneFromString("UTC");
	else
		tz = GetTimeZoneFromString(timezone);

	//round to the appropriate precision for seconds
	if(has_fractional_seconds)
		return ConvertZonedDateTimeToString<std::chrono::system_clock::time_point>(datetime, format, locale, tz);
	else
	{
		auto rounded_timepoint = std::chrono::floor<std::chrono::seconds>(datetime);
		return ConvertZonedDateTimeToString<decltype(rounded_timepoint)>(rounded_timepoint, format, locale, tz);
	}
}

//parses time_str based on format and locale and returns the number of seconds since midnight
double GetNumSecondsSinceMidnight(const std::string &time_str, std::string format, std::string locale)
{
	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		static CachedLocale cached_locale;

	cached_locale.ResetStringStream(time_str);

	if(!locale.empty())
	{
		//make sure it's utf-8
		locale += ".utf-8";
		//if the locale is valid, use it
		try
		{
			cached_locale.UpdateLocaleIfNeeded(locale);
		}
		catch(...)
		{
			//can't emit anything
		}
	}

	try
	{
		std::chrono::nanoseconds tp;
		cached_locale.stringStream >> date::parse(format, tp);

		if(cached_locale.stringStream.fail())
			return 0.0;

		double seconds_since_midnight = std::chrono::duration_cast<std::chrono::microseconds>(tp).count() / 1000000.0;
		seconds_since_midnight = std::fmod(seconds_since_midnight, 86400);
		if(seconds_since_midnight < 0)
			seconds_since_midnight += 86400;
		return seconds_since_midnight;
	}
	catch(...)
	{
		//can't emit anything
	}

	return 0.0;
}

//transfroms num_secs_from_midnight into a string representing the time of day
std::string GetTimeStringFromNumSecondsSinceMidnight(double seconds_since_midnight, std::string format, std::string locale)
{
	if(seconds_since_midnight != seconds_since_midnight
			|| seconds_since_midnight == std::numeric_limits<double>::infinity()
			|| seconds_since_midnight == -std::numeric_limits<double>::infinity())
		seconds_since_midnight = 0.0;

	seconds_since_midnight = std::fmod(seconds_since_midnight, 86400);
	if(seconds_since_midnight < 0)
		seconds_since_midnight += 86400;
	bool has_fractional_seconds = (seconds_since_midnight != static_cast<int64_t>(seconds_since_midnight));

	auto tp = std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(seconds_since_midnight));

	#if defined(MULTITHREAD_SUPPORT) || defined(MULTITHREAD_INTERFACE)
	thread_local
	#endif
		static CachedLocale cached_locale;

	cached_locale.ResetStringStream();
	if(!locale.empty())
	{
		//make sure it's utf-8
		locale += ".utf-8";
		//if the locale is valid, use it
		try
		{
			cached_locale.UpdateLocaleIfNeeded(locale);
		}
		catch(...)
		{
			//can't emit anything
		}
	}

	try
	{
		if(has_fractional_seconds)
		{
			cached_locale.stringStream << date::format(format, tp);
		}
		else
		{
			auto rounded_timepoint = std::chrono::floor<std::chrono::seconds>(tp);
			cached_locale.stringStream << date::format(format, rounded_timepoint);
		}

		return cached_locale.stringStream.str();
	}
	catch(...)
	{
		//can't emit anything
	}

	return "";
}

std::string _time_zone_database_path = SetTimeZoneDatabasePath();
