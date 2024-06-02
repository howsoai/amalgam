#pragma once

//system headers:
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace StringManipulation
{
	//converts a number into a string quickly and accurately (moreso than built-in C++ libraries)
	std::string NumberToString(double value);
	std::string NumberToString(size_t value);

	//removes the first token from str and return the removed token
	std::string RemoveFirstToken(std::string &str);

	//splits a string by given delimiter
	std::vector<std::string> Split(std::string &s, char delim = ' ');

	//separates the argument string and returns an appropriate vector of strings
	//if greedy is true, the returned vector contains the full list of arguments and arg_string is unmodified
	//if greedy is false, the returned vector only contains a single string, and arg_string is modified
	//to only contain the portion of the string after the removed section
	std::vector<std::string> SplitArgString(std::string &arg_string, bool greedy = true);

	//returns the number of bytes wide the character in position of string s is if it is whitespace,
	// 0 if it is not a newline
	inline size_t IsUtf8Whitespace(const std::string &s, size_t position)
	{
		auto cur_char = s[position];
		if(cur_char == '\t' || cur_char == '\n' || cur_char == '\v' || cur_char == '\f'
				|| cur_char == '\r' || cur_char == ' ')
			return 1;

		//need to additionally check the following multicharacter utf-8 code points:
		//name							hex			dec		bytes
		// no - break space				U + 00A0	160		0xC2 0xA0
		// ogham space mark				U + 1680	5760	0xE1 0x9A 0x80
		// en quad						U + 2000	8192	0xE2 0x80 0x80
		// em quad						U + 2001	8193	0xE2 0x80 0x81
		// en space						U + 2002	8194	0xE2 0x80 0x82
		// em space						U + 2003	8195	0xE2 0x80 0x83
		// three - per - em space		U + 2004	8196	0xE2 0x80 0x84
		// four - per - em space		U + 2005	8197	0xE2 0x80 0x85
		// six - per - em space			U + 2006	8198	0xE2 0x80 0x86
		// figure space					U + 2007	8199	0xE2 0x80 0x87
		// punctuation space			U + 2008	8200	0xE2 0x80 0x88
		// thin space					U + 2009	8201	0xE2 0x80 0x89
		// hair space					U + 200A	8202	0xE2 0x80 0x8A
		// line separator				U + 2028	8232	0xE2 0x80 0xA8
		// paragraph separator			U + 2029	8233	0xE2 0x80 0xA9
		// narrow no - break space		U + 202F	8239	0xE2 0x80 0xAF
		// medium mathematical space	U + 205F	8287	0xE2 0x81 0x9F
		// ideographic space			U + 3000	12288	0xE3 0x80 0x80

		//need at least 2 characters for the remaining whitespace possibilities
		if(position + 2 >= s.size())
			return 0;

		if(static_cast<uint8_t>(cur_char) == 0xC2 && static_cast<uint8_t>(s[position + 1]) == 0xA0)
			return 2;

		//need 3 characters for the remaining whitespace possibilities
		if(position + 3 >= s.size())
			return 0;

		if(static_cast<uint8_t>(cur_char) == 0xE1
				&& static_cast<uint8_t>(s[position + 1]) == 0x9A
				&& static_cast<uint8_t>(s[position + 2]) == 0x80)
			return 3;

		if(static_cast<uint8_t>(cur_char) == 0xE2)
		{
			if(static_cast<uint8_t>(s[position + 1]) == 0x80)
			{
				uint8_t third_char = s[position + 2];
				if(third_char >= 0x80 && third_char <= 0xAF)
					return 3;
			}
			else if(static_cast<uint8_t>(s[position + 1]) == 0x81 && static_cast<uint8_t>(s[position + 2]) == 0x9F)
			{
				return 3;
			}
		}

		if(static_cast<uint8_t>(cur_char) == 0xE3
				&& static_cast<uint8_t>(s[position + 1]) == 0x80
				&& static_cast<uint8_t>(s[position + 2]) == 0x80)
			return 3;

		return 0;
	}

	//returns true if c is a numeric digit
	inline bool IsUtf8ArabicNumerals(uint8_t c)
	{
		return (c >= '0' && c <= '9');
	}

	//returns the number of bytes wide the character in position of string s is if it is a newline,
	// 0 if it is not a newline
	inline size_t IsUtf8Newline(std::string &s, size_t position)
	{
		auto cur_char = s[position];
		//don't count carriage returns (\r) as new lines, since it just moves the cursor
		if(cur_char == '\n' || cur_char == '\v' || cur_char == '\f')
			return 1;

		if(position + 3 < s.size())
		{
			if(static_cast<uint8_t>(cur_char) == 0xE2)
			{
				//line separator
				if(static_cast<uint8_t>(s[position + 1]) == 0x80 && static_cast<uint8_t>(s[position + 2]) == 0xA8)
					return 3;
				//paragraph separator
				else if(static_cast<uint8_t>(s[position + 1]) == 0x80 && static_cast<uint8_t>(s[position + 2]) == 0xA9)
					return 3;
			}
		}

		return 0;
	}

	//returns the length of the UTF-8 character in s starting at the specified offset
	inline size_t GetUTF8CharacterLength(std::string_view s, size_t offset = 0)
	{
		if(offset >= s.size())
			return 0;

		//there's at least one byte left
		size_t remaining_length = s.size() - offset;

		uint8_t first_byte = s[offset];

		//0xxxxxxx means 1 byte in UTF-8 standard
		if((first_byte & 0x80) == 0x00)
			return 1;

		//110xxxxx means 2 bytes in UTF-8 standard
		if((first_byte & 0xE0) == 0xC0)
			return std::min<size_t>(2, remaining_length);

		//1110xxxx means 3 bytes in UTF-8 standard
		if((first_byte & 0xF0) == 0xE0)
			return std::min<size_t>(3, remaining_length);

		//11110xxx means 4 bytes in UTF-8 standard
		if((first_byte & 0xF8) == 0xF0)
			return std::min<size_t>(4, remaining_length);

		//else invalid UTF-8, just return one byte
		return 1;
	}

	//returns the number of UTF8 characters in the string
	inline size_t GetNumUTF8Characters(std::string_view s)
	{
		size_t offset = 0;
		size_t next_offset = 0;
		size_t num_chars = 0;
		do
		{
			next_offset = GetUTF8CharacterLength(s, offset);
			offset += next_offset;
			if(next_offset != 0)
				num_chars++;
		} while(next_offset != 0);

		return num_chars;
	}

	//for s, finds the offset of the last utf8 character and its length
	inline std::pair<size_t, size_t> GetLastUTF8CharacterOffsetAndLength(std::string_view s)
	{
		//walk along the utf8 string until find the last character
		size_t offset = 0;
		size_t end_offset = 0 + GetUTF8CharacterLength(s, 0);
		while(end_offset < s.size())
		{
			size_t next_length = GetUTF8CharacterLength(s, end_offset);
			if(next_length == 0)
				break;

			offset = end_offset;
			end_offset = offset + next_length;
		}

		size_t length = end_offset - offset;
		return std::make_pair(offset, length);
	}

	//returns the offset of the nth utf8 character in the specified string
	// if the string does not have that many characters, then it will return the size of the string
	inline size_t GetNthUTF8CharacterOffset(std::string_view s, size_t nth)
	{
		size_t offset = 0;
		for(size_t i = 0; i < nth; i++)
		{
			size_t len = GetUTF8CharacterLength(s, offset);
			if(len == 0)
				break;

			offset += len;
		}

		return offset;
	}

	//returns the offset of the nth last utf8 character in the specified string
	// if the string does not have that many characters, then it will return the size of the string
	inline size_t GetNthLastUTF8CharacterOffset(std::string_view s, size_t nth)
	{
		size_t num_utf8_chars = GetNumUTF8Characters(s);

		//if past the end, just return the end
		if(nth >= num_utf8_chars)
			return s.size();

		//reflect from the end
		nth = num_utf8_chars - nth;

		return GetNthUTF8CharacterOffset(s, nth);
	}

	//expands the utf8 string s into each character in exploded
	inline void ExplodeUTF8Characters(std::string_view s, std::vector<uint32_t> &exploded)
	{
		exploded.clear();

		size_t utf8_char_start_offset = 0;
		while(utf8_char_start_offset < s.size())
		{
			size_t utf8_char_length = StringManipulation::GetUTF8CharacterLength(s, utf8_char_start_offset);
			//done if no more characters
			if(utf8_char_length == 0)
				break;

			//there's at least one character, but copy out each character in the string
			uint32_t value = s[utf8_char_start_offset];
			for(size_t i = 1; i < utf8_char_length; i++)
			{
				value <<= 8;
				value |= s[utf8_char_start_offset + i];
			}
			exploded.push_back(value);

			utf8_char_start_offset += utf8_char_length;
		}
	}

	//concatenates utf8 characters into utf8 string, opposite of ExplodeUTF8Characters
	inline std::string ConcatUTF8Characters(std::vector<uint32_t> &chars)
	{
		std::string result;
		result.reserve(chars.size());

		//for each character, concatenate any parts that fit
		for(auto c : chars)
		{
			if(c > 0xFFFFFF)
			{
				result.push_back(c >> 24);
				c &= 0xFFFFFF;
			}

			if(c > 0xFFFF)
			{
				result.push_back(c >> 16);
				c &= 0xFFFF;
			}

			if(c > 0xFF)
			{
				result.push_back(c >> 8);
				c &= 0xFF;
			}

			result.push_back(c);
		}

		return result;
	}

	template<typename SourceType>
	inline std::string To1ByteString(SourceType value)
	{
		std::string string_value(1, '\0');
		string_value[0] = reinterpret_cast<uint8_t &>(value);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To2ByteStringLittleEndian(SourceType value)
	{
		std::string string_value(2, '\0');
		uint16_t to_write = reinterpret_cast<uint16_t &>(value);
		string_value[0] = static_cast<uint8_t>(to_write & 255);
		string_value[1] = static_cast<uint8_t>((to_write >> 8) & 255);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To2ByteStringBigEndian(SourceType value)
	{
		std::string string_value(2, '\0');
		uint16_t to_write = reinterpret_cast<uint16_t &>(value);
		string_value[1] = static_cast<uint8_t>(to_write & 255);
		string_value[0] = static_cast<uint8_t>((to_write >> 8) & 255);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To4ByteStringLittleEndian(SourceType value)
	{
		std::string string_value(4, '\0');
		uint32_t to_write = reinterpret_cast<uint32_t &>(value);
		string_value[0] = static_cast<uint8_t>(to_write & 255);
		string_value[1] = static_cast<uint8_t>((to_write >> 8) & 255);
		string_value[2] = static_cast<uint8_t>((to_write >> 16) & 255);
		string_value[3] = static_cast<uint8_t>((to_write >> 24) & 255);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To4ByteStringBigEndian(SourceType value)
	{
		std::string string_value(4, '\0');
		uint32_t to_write = reinterpret_cast<uint32_t &>(value);
		string_value[3] = static_cast<uint8_t>(to_write & 255);
		string_value[2] = static_cast<uint8_t>((to_write >> 8) & 255);
		string_value[1] = static_cast<uint8_t>((to_write >> 16) & 255);
		string_value[0] = static_cast<uint8_t>((to_write >> 24) & 255);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To8ByteStringLittleEndian(SourceType value)
	{
		std::string string_value(8, '\0');
		uint64_t to_write = reinterpret_cast<uint64_t &>(value);
		string_value[0] = static_cast<uint8_t>(to_write & 255);
		string_value[1] = static_cast<uint8_t>((to_write >> 8) & 255);
		string_value[2] = static_cast<uint8_t>((to_write >> 16) & 255);
		string_value[3] = static_cast<uint8_t>((to_write >> 24) & 255);
		string_value[4] = static_cast<uint8_t>((to_write >> 32) & 255);
		string_value[5] = static_cast<uint8_t>((to_write >> 40) & 255);
		string_value[6] = static_cast<uint8_t>((to_write >> 48) & 255);
		string_value[7] = static_cast<uint8_t>((to_write >> 56) & 255);
		return string_value;
	}

	template<typename SourceType>
	inline std::string To8ByteStringBigEndian(SourceType value)
	{
		std::string string_value(8, '\0');
		uint64_t to_write = reinterpret_cast<uint64_t &>(value);
		string_value[7] = static_cast<uint8_t>(to_write & 255);
		string_value[6] = static_cast<uint8_t>((to_write >> 8) & 255);
		string_value[5] = static_cast<uint8_t>((to_write >> 16) & 255);
		string_value[4] = static_cast<uint8_t>((to_write >> 24) & 255);
		string_value[3] = static_cast<uint8_t>((to_write >> 32) & 255);
		string_value[2] = static_cast<uint8_t>((to_write >> 40) & 255);
		string_value[1] = static_cast<uint8_t>((to_write >> 48) & 255);
		string_value[0] = static_cast<uint8_t>((to_write >> 56) & 255);
		return string_value;
	}

	//converts a single Base16 character into a binary nibble value
	constexpr uint8_t Base16CharToVal(char c)
	{
		if(c >= '0' && c <= '9')
			return c - '0';
		if(c >= 'a' && c <= 'f')
			return 10 + c - 'a';
		if(c >= 'A' && c <= 'F')
			return 10 + c - 'A';

		return 0;
	}

	//encodes the binary_string with Base16 and returns the new string
	std::string BinaryStringToBase16(std::string &binary_string);

	//decodes the Base16 string and returns the binary string
	std::string Base16ToBinaryString(std::string &base16_string);

	//converts a single Base64 character into a binary 6-bit value
	constexpr uint8_t Base64CharToVal(char c)
	{
		if(c >= 'A' && c <= 'Z')
			return c - 'A';
		if(c >= 'a' && c <= 'z')
			return 26 + c - 'a';
		if(c >= '0' && c <= '9')
			return 52 + c - '0';
		if(c == '+')
			return 62;
		if(c == '/')
			return 63;

		return 0;
	}

	//encodes the binary_string with Base64 and returns the new string
	std::string BinaryStringToBase64(std::string &binary_string);

	//decodes the Base64 string and returns the binary string
	std::string Base64ToBinaryString(std::string &base64_string);

	static const std::string base16Chars = "0123456789abcdef";
	static const std::string base64Chars
		= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	//converts 3 binary bytes into 4 chars for Base64 encoding
	inline std::array<char, 4> Base64ThreeBytesToFourChars(uint8_t a, uint8_t b, uint8_t c)
	{
		uint32_t value_of_triple = ((a << 16) | (b << 8) | c);

		//extract each group of 6 bits
		char char1 = base64Chars[(value_of_triple >> 18) & 63];
		char char2 = base64Chars[(value_of_triple >> 12) & 63];
		char char3 = base64Chars[(value_of_triple >> 6) & 63];
		char char4 = base64Chars[value_of_triple & 63];
		return { char1, char2, char3, char4 };
	}

	//converts 4 chars into 3 binary bytes for Base64 encoding
	inline std::array<uint8_t, 3> Base64FourCharsToThreeBytes(char a, char b, char c, char d)
	{
		std::uint32_t value_of_quad = ( (Base64CharToVal(a) << 18)
										| (Base64CharToVal(b) << 12)
										| (Base64CharToVal(c) << 6)
										| Base64CharToVal(d) );

		uint8_t value1 = (value_of_quad >> 16) & 255;
		uint8_t value2 = (value_of_quad >> 8) & 255;
		uint8_t value3 = value_of_quad & 255;
		return { value1, value2, value3 };
	}

	//compares right-aligned numbers in a string.  searches for first digit that isn't equal,
	// figures out which one is greater, and remembers it.  then it sees which number string is longer
	// if the number strings are the same length, then go with whichever was remembered to be bigger
	// both indices will be updated along the way
	int CompareNumberInStringRightJustified(const std::string &a, const std::string &b, size_t &a_index, size_t &b_index);

	//compares left-aligned numbers in a string until a difference is found, then uses that for comparison
	// starts at the specified indicies
	// both indices will be updated along the way
	int CompareNumberInStringLeftJustified(const std::string &a, const std::string &b, size_t &a_index, size_t &b_index);

	//compares two strings "naturally" as applicable, ignoring spaces and treating numbers how a person would
	// however, if the strings are "identical" via natural comparison, then it falls back to regular string comparison to ensure
	// that strings are always ordered the same way
	int StringNaturalCompare(const std::string &a, const std::string &b);

	//variant of StringNaturalCompare for sorting
	inline bool StringNaturalCompareSort(const std::string &a, const std::string &b)
	{
		int comp = StringNaturalCompare(a, b);
		return comp < 0;
	}

	//variant of StringNaturalCompare for reverse sorting
	inline bool StringNaturalCompareSortReverse(const std::string &a, const std::string &b)
	{
		int comp = StringNaturalCompare(a, b);
		return comp > 0;
	}

};
