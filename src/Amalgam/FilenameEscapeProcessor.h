#pragma once

//project headers:
#include "PlatformSpecific.h"

//system headers:
#include <string>

class FilenameEscapeProcessor
{
public:
	static const char escape_char = '_';
	static const size_t num_bytes_per_char = 1;
	static const size_t num_hex_values_per_char = 2 * num_bytes_per_char;
	static_assert(num_hex_values_per_char == 2, "hex string escaping only supports 2 hex per char for now (requires some generalization of member functions beyond 2)");

	//returns true if the char c is safe to leave unescaped in a filename string for amalgam
	//safe characters: [0-9][a-z][A-Z]
	//Devnote: character ranges given as numeric as the function only works if '0' < '9', '9' < 'a', etc.
	//The static_asserts give no performance impact, but basically assert that the above is true AND the desired characters are the actual values specified
	static bool IsUnescapedCharSafe(const char c)
	{
		// lower bound limit //
		static_assert('0' == 48, "inconsistent character values.");
		if(c < 48) //'0'
			return false;

		// ranges //
		//0-9
		static_assert('9' == 57, "inconsistent character values.");
		if(c <= 57) //'9'
			return true;

		//reject between '9' and 'A'
		static_assert('A' == 65, "inconsistent character values.");
		if(c < 65) //'A'
			return false;

		//A-Z
		static_assert('Z' == 90, "inconsistent character values.");
		if(c <= 90) //'Z'
			return true;

		//reject between 'Z' and 'a'
		static_assert('a' == 97, "inconsistent character values.");
		if(c < 97) //'a'
			return false;

		//a-z
		static_assert('z' == 122, "inconsistent character values.");
		if(c <= 122) //'z'
			return true;

		//anything beyond 122 'z' is rejected
		return false;
	}

	//converts a [0-15] (4-bit) value to a single char of its associated hexidecimal character
	static char DecimalToHex(const uint8_t c)
	{
		assert(c < 16); //value must be 4-bits only

		if(c >= 10)
			return c - 10 + 'a';

		return c + '0';
	}

	static constexpr uint8_t HexToDecimal(const char c)
	{
		if(c >= '0')
		{
			if(c <= '9')
				return c - '0';

			if(c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			if(c >= 'A' && c <= 'F')
				return c - 'A' + 10;
		}

		//invalid and possibly unsafe char is not a hex value, return 0 as having no value
		return 0;
	}

	//generates the 2 escape hex characters for a given an 8-bit character
	static void GetEscapeHexFromCharValue(char c, char &high_out, char &low_out)
	{
		low_out = DecimalToHex(15 & c);
		high_out = DecimalToHex(15 & (c >> 4));
	}

	//Get the 8-bit value represented by 2 4-bit hex characters
	static constexpr char GetCharValueFromEscapeHex(const char high, const char low)
	{
		return HexToDecimal(low) + ((HexToDecimal(high) << 4) & 240);
	}

	//returns a copy of string where all potentially unsafe characters are escaped
	//see IsUnescapedCharSafe() for list of safe characters
	static std::string SafeEscapeFilename(const std::string &string)
	{
		std::string out;

		if(string.length() == 0)
			return out;

		char escape_buffer[1 + num_hex_values_per_char]; //store 1 escape char '_' + 2 hex digit chars per byte
		escape_buffer[0] = escape_char;

		for(const auto a : string)
		{
			if(IsUnescapedCharSafe(a))
				out += a;
			else
			{
				GetEscapeHexFromCharValue(a, escape_buffer[1], escape_buffer[2]);
				out.append(&escape_buffer[0], sizeof(escape_buffer));
			}
		}

		return out;
	}

	//returns a copy of string where escaped characters are converted back to their 8-bit values
	//any character sequence _xx is converted to a single 8-bit character using xx as the hex code
	static std::string SafeUnescapeFilename(const std::string &string)
	{
		std::string out;

		if(string.length() == 0)
			return out;

		uint8_t escape_index = 0;
		char escape_hex[num_hex_values_per_char] = { 0, 0 };
		for(const auto a : string)
		{
			if(a == escape_char)
			{
				assert(escape_index == 0); //must complete a previous escape before starting a new one
				escape_index = num_hex_values_per_char;
			}
			else
			{
				if(escape_index > 0)
				{
					escape_index--;
					escape_hex[escape_index] = a; //filled backwards for speed (see below)

					if(escape_index == 0)
					{
						out += GetCharValueFromEscapeHex(escape_hex[1], escape_hex[0]); //escape hex is filled backwards so swap digits
						escape_hex[0] = 0;
						escape_hex[1] = 0;
					}
				}
				else
				{
					out += a;
				}
			}
		}

		return out;
	}
};
