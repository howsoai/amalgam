//project headers:
#include "StringManipulation.h"

#include "FastMath.h"

//3rd party headers:
#include "swiftdtoa/SwiftDtoa.h"

//system headers:
#include <algorithm>
#include <sstream>
#include <string>

std::string StringManipulation::NumberToString(double value)
{
	//first check for unusual values
	if(FastIsNaN(value))
		return ".nan";
	if(value == std::numeric_limits<double>::infinity())
		return ".infinity";
	if(value == -std::numeric_limits<double>::infinity())
		return "-.infinity";

	char char_buffer[128];
	size_t num_chars_written = swift_dtoa_optimal_double(value, &char_buffer[0], sizeof(char_buffer));
	return std::string(&char_buffer[0], num_chars_written);
}

std::string StringManipulation::NumberToString(size_t value)
{
	//do this our own way because regular string manipulation libraries are slow and measurably impact performance
	constexpr size_t max_num_digits = std::numeric_limits<size_t>::digits / 3; //max of binary digits per character
	constexpr size_t buffer_size = max_num_digits + 2;
	char buffer[buffer_size];
	char *p = &buffer[0];

	if(value == 0) //check for zero because it's a very common case for integers
		*p++ = '0';
	else //convert each character
	{
		//peel off digits and put them in the next position for the string (reverse when done)
		char *buffer_start = &buffer[0];
		while(value != 0)
		{
			//pull off the least significant digit and convert it to a number character
			*p++ = ('0' + (value % 10));
			value /= 10;
		}

		//put back in original order
		std::reverse(buffer_start, p);
	}
	*p = '\0';	//terminate string
	return std::string(&buffer[0]);
}

std::string StringManipulation::RemoveFirstWord(std::string &str, bool strip_word, char char_to_strip)
{
	std::string first_token;

	if(str.size() == 0)
		return first_token;


	//if str is wrapped in char_to_strip's, remove chars between char_to_strip's (typically double quotes)
	if(strip_word && (str[0] == char_to_strip))
	{
		size_t end_char_to_strip_idx;
		end_char_to_strip_idx = str.find(char_to_strip, 1);
		//if no end char_to_strip return rest of str past initial char_to_strp
		if(end_char_to_strip_idx == std::string::npos)
		{
			first_token = str.substr(1);
			str = "";
			return first_token;
		}

		//the ending char_to_strip must not be escaped
		if(end_char_to_strip_idx != 0)
		{
			while(str[end_char_to_strip_idx - 1] == '\\')
			{
				str.erase(end_char_to_strip_idx - 1, 1); //remove the escape chars
				end_char_to_strip_idx = str.find(char_to_strip, end_char_to_strip_idx + 1);
			}
		}

		//chars between first and last char_to_strips make up the token
		first_token = str.substr(1, end_char_to_strip_idx - 1);

		//update str and remove preceding whitespace
		str = str.substr(end_char_to_strip_idx + 1);
		while(str.size() > 0 && str[0] == ' ') {
			str = str.substr(1); //Is there a faster way than this?
		}
		return first_token;
	}

	//otherwise, split based on whitespace
	size_t spacepos = str.find(' ');
	if(spacepos == std::string::npos)
	{
		first_token = str;
		str = "";
	}
	else
	{
		first_token = str.substr(0, spacepos);
		str = str.substr(spacepos + 1);
	}
	return first_token;
}

std::vector<std::string> StringManipulation::Split(std::string &s, char delim)
{
	std::vector<std::string> ret;
	std::stringstream ss { s };
	std::string item;

	while(std::getline(ss, item, delim))
		ret.push_back(item);

	return ret;
}

std::string StringManipulation::BinaryStringToBase16(std::string &binary_string)
{
	std::string base16_string;
	base16_string.resize(2 * binary_string.size());
	for(size_t i = 0; i < binary_string.size(); i++)
	{
		uint8_t value = binary_string[i];
		base16_string[2 * i] = base16Chars[value >> 4];
		base16_string[2 * i + 1] = base16Chars[value & 15];
	}

	return base16_string;
}

std::string StringManipulation::Base16ToBinaryString(std::string &base16_string)
{
	std::string binary_string;
	binary_string.resize(base16_string.size() / 2);
	for(size_t i = 0; i < base16_string.size(); i += 2)
	{
		uint8_t value = (Base16CharToVal(base16_string[i]) << 4);
		value += Base16CharToVal(base16_string[i + 1]);
		binary_string[i / 2] = value;
	}

	return binary_string;
}

std::string StringManipulation::BinaryStringToBase64(std::string &binary_string)
{
	size_t binary_len = binary_string.size();
	size_t full_triples = binary_len / 3;

	std::string base64_string;
	//resize triples to quads
	base64_string.reserve((full_triples + 2) * 4);

	//encode all groups of 3
	for(size_t i = 0; i + 3 <= binary_len; i += 3)
	{
		auto encoded_quad = Base64ThreeBytesToFourChars(binary_string[i],
								binary_string[i + 1], binary_string[i + 2]);
		base64_string.append(begin(encoded_quad), end(encoded_quad));
	}

	//clean up any characters that aren't divisible by 3,
	// zero fill the remaining bytes, and pad with '=' characters per standard
	size_t chars_beyond_triplets = binary_len - full_triples * 3;
	if(chars_beyond_triplets == 2)
	{
		auto encoded_quad = Base64ThreeBytesToFourChars(binary_string[binary_len - 2],
														binary_string[binary_len - 1], 0);

		base64_string.push_back(encoded_quad[0]);
		base64_string.push_back(encoded_quad[1]);
		base64_string.push_back(encoded_quad[2]);
		base64_string.push_back('=');
	}
	else if(chars_beyond_triplets == 1)
	{
		auto encoded_quad = Base64ThreeBytesToFourChars(binary_string[binary_len - 1], 0, 0);

		base64_string.push_back(encoded_quad[0]);
		base64_string.push_back(encoded_quad[1]);
		base64_string.push_back('=');
		base64_string.push_back('=');
	}

	return base64_string;
}

std::string StringManipulation::Base64ToBinaryString(std::string &base64_string)
{
	size_t base64_len = base64_string.size();

	if(base64_len == 0)
		return std::string();

	//if the length isn't divisible by 4, then resize down
	if((base64_len % 4) != 0)
	{
		base64_len = (base64_len * 4) / 4;
		base64_string.resize(base64_len);
	}

	//exclude last quad, because don't know if it is full
	// in case it has any padding via '=' character and will need special logic
	size_t known_full_quads = (base64_len / 4) - 1;

	std::string binary_string;
	//resize quads to triples
	binary_string.reserve( ((known_full_quads + 2) * 3) / 4);

	//iterate over quads, but don't use <= because don't want to include last quad,
	// same reasoning as known_full_quads
	for(size_t i = 0; i + 4 < base64_len; i += 4)
	{
		auto triplet = Base64FourCharsToThreeBytes(base64_string[i],
						base64_string[i + 1], base64_string[i + 2], base64_string[i + 3]);
		binary_string.append(begin(triplet), end(triplet));
	}

	size_t last_quad_start = known_full_quads * 4;

	if(base64_string[last_quad_start + 2] == '=')
	{
		auto triplet = Base64FourCharsToThreeBytes(base64_string[last_quad_start],
						base64_string[last_quad_start + 1], 'A', 'A');
		binary_string.push_back(triplet[0]);
	}
	else if(base64_string[last_quad_start + 3] == '=')
	{
		auto triplet = Base64FourCharsToThreeBytes(base64_string[last_quad_start],
			base64_string[last_quad_start + 1], base64_string[last_quad_start + 2], 'A');
		binary_string.push_back(triplet[0]);
		binary_string.push_back(triplet[1]);
	}
	else //last quad is full
	{
		auto triplet = Base64FourCharsToThreeBytes(base64_string[last_quad_start],
			base64_string[last_quad_start + 1], base64_string[last_quad_start + 2],
			base64_string[last_quad_start + 3]);
		binary_string.append(begin(triplet), end(triplet));
	}

	return binary_string;
}

int StringManipulation::CompareNumberInStringRightJustified(const std::string &a, const std::string &b, size_t &a_index, size_t &b_index)
{
	//comparison result of first non-matching digit
	int compare_val_if_same_length = 0;

	while(1)
	{
		unsigned char a_value;
		unsigned char b_value;

		//treat as if zero terminated strings
		if(a_index < a.size())
			a_value = a[a_index];
		else
			a_value = '\0';

		if(b_index < b.size())
			b_value = b[b_index];
		else
			b_value = '\0';

		if(!StringManipulation::IsUtf8ArabicNumerals(a_value)
				&& !StringManipulation::IsUtf8ArabicNumerals(b_value))
			return compare_val_if_same_length;
		if(!StringManipulation::IsUtf8ArabicNumerals(a_value))
			return -1;
		if(!StringManipulation::IsUtf8ArabicNumerals(b_value))
			return +1;

		//see if found first nonmatching digit
		if(a_value < b_value)
		{
			if(compare_val_if_same_length == 0)
				compare_val_if_same_length = -1;
		}
		else if(a_value > b_value)
		{
			if(compare_val_if_same_length == 0)
				compare_val_if_same_length = +1;
		}

		a_index++;
		b_index++;
	}

	//can't make it here
	return 0;
}

int StringManipulation::CompareNumberInStringLeftJustified(const std::string &a, const std::string &b, size_t &a_index, size_t &b_index)
{
	while(1)
	{
		unsigned char a_value;
		unsigned char b_value;

		//treat as if zero terminated strings
		if(a_index < a.size())
			a_value = a[a_index];
		else
			a_value = '\0';

		if(b_index < b.size())
			b_value = b[b_index];
		else
			b_value = '\0';

		//if out of digits, then they're equal
		if(!StringManipulation::IsUtf8ArabicNumerals(a_value)
				&& !StringManipulation::IsUtf8ArabicNumerals(b_value))
			return 0;

		//if one ran out of digits, then it's less
		if(!StringManipulation::IsUtf8ArabicNumerals(a_value))
			return -1;
		if(!StringManipulation::IsUtf8ArabicNumerals(b_value))
			return +1;

		//compare values
		if(a_value < b_value)
			return -1;
		if(a_value > b_value)
			return +1;

		a_index++;
		b_index++;
	}

	//can't get here
	return 0;
}

int StringManipulation::StringNaturalCompare(const std::string &a, const std::string &b)
{
	size_t a_index = 0, b_index = 0;

	while(1)
	{
		unsigned char a_value;
		unsigned char b_value;

		//skip over spaces
		while(a_index < a.size() && std::isspace(static_cast<unsigned char>(a[a_index])))
			a_index++;
		//treat as if zero terminated string
		if(a_index < a.size())
			a_value = a[a_index];
		else
			a_value = '\0';

		//skip over spaces
		while(b_index < b.size() && std::isspace(static_cast<unsigned char>(b[b_index])))
			b_index++;
		if(b_index < b.size())
			b_value = b[b_index];
		else
			b_value = '\0';

		//check for group of digits
		if(StringManipulation::IsUtf8ArabicNumerals(a_value)
			&& StringManipulation::IsUtf8ArabicNumerals(static_cast<unsigned char>(b_value)))
		{
			int result;
			//if starts with leading zeros, then do a comparison from the left, otherwise from the right
			if(a_value == '0' || b_value == '0')
				result = CompareNumberInStringLeftJustified(a, b, a_index, b_index);
			else
				result = CompareNumberInStringRightJustified(a, b, a_index, b_index);

			if(result != 0)
				return result;

			//if made it here, then the numbers were equal; move on to the next character
			continue;
		}

		//if strings are identical from a natural sorting perspective, then use regular compare to make sure order consistency is preserved
		if(a_value == '\0' && b_value == '\0')
			return a.compare(b);

		if(a_value < b_value)
			return -1;

		if(a_value > b_value)
			return +1;

		a_index++;
		b_index++;
	}

	return 0;
}
