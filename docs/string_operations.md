### Opcode: `explode`
#### Parameters
`string str [number stride]`
#### Description
Explodes `str` into the pieces that make it up.  If `stride` is zero or unspecified, then it explodes `str` by character per UTF-8 parsing.  If `stride` is specified, then it breaks it into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(explode "abcdefghi")
```
Output:
```amalgam
[
	"a"
	"b"
	"c"
	"d"
	"e"
	"f"
	"g"
	"h"
	"i"
]
```
Example:
```amalgam
(explode "abcdefghi" 1)
```
Output:
```amalgam
[
	"a"
	"b"
	"c"
	"d"
	"e"
	"f"
	"g"
	"h"
	"i"
]
```
Example:
```amalgam
(explode "abcdefghi" 2)
```
Output:
```amalgam
["ab" "cd" "ef" "gh" "i"]
```
Example:
```amalgam
(explode "abcdefghi" 3)
```
Output:
```amalgam
["abc" "def" "ghi"]
```
Example:
```amalgam
(explode "abcdefghi" 4)
```
Output:
```amalgam
["abcd" "efgh" "i"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `split`
#### Parameters
`string str [string split_string] [number max_split_count] [number stride]`
#### Description
Splits `str` into a list of strings based on `split_string`, which is handled as a regular expression.  Any data matching `split_string` will not be included in any of the resulting strings.  If `max_split_count` is provided and greater than zero, it will only split up to that many times.  If `stride` is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If `stride` is specified and a value other than zero, then it does not use `split_string` as a regular expression but rather a string, and it breaks the result into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(split "hello world")
```
Output:
```amalgam
["hello world"]
```
Example:
```amalgam
(split "hello world" " ")
```
Output:
```amalgam
["hello" "world"]
```
Example:
```amalgam
(split "hello\r\nworld\r\n!" "\r\n")
```
Output:
```amalgam
["hello" "world" "!"]
```
Example:
```amalgam
(split "hello world !" "\\s" 1)
```
Output:
```amalgam
["hello" "world !"]
```
Example:
```amalgam
(split "hello to the world" "to" (null) 2)
```
Output:
```amalgam
["hello " " the world"]
```
Example:
```amalgam
(split "abcdefgij")
```
Output:
```amalgam
["abcdefgij"]
```
Example:
```amalgam
(split "abc de fghij" " ")
```
Output:
```amalgam
["abc" "de" "fghij"]
```
Example:
```amalgam
(split "abc\r\nde\r\nfghij" "\r\n")
```
Output:
```amalgam
["abc" "de" "fghij"]
```
Example:
```amalgam
(split "abc de fghij" " " 1)
```
Output:
```amalgam
["abc" "de fghij"]
```
Example:
```amalgam
(split "abc de fghij" " de " (null) 4)
```
Output:
```amalgam
["abc de fghij"]
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `substr`
#### Parameters
`string str [number|string location] [number|string param] [string replacement] [number stride]`
#### Description
Finds a substring `str`.  If `location` is a number, then evaluates to a new string representing the substring starting at the offset specified by `location`.  If `location` is a string, then it will treat `location` as a regular expression.  If `param` is specified, then it may change the interpretation of `location`.  If `param` is specified and `location` is a number it will go until that length beyond the offset specified by `location`.  If `param` is specified and `location` is a regular expression, `param` will represent one of the following: if null or "first", then it will return the first match of the regular expression; or if `param` is a number or the string "all", then substr will evaluate to a list of up to param matches (which may be infinite yielding the same result as "all").  If `param` is a negative number or the string "submatches", then it will return a list of list of strings, for each match up to the count of the negative number or all matches.  If `param` is "submatches", each inner list will represent the full regular expression match followed by each submatch as captured by parenthesis in the regular expression, ordered from an outer to inner, left-to-right manner.  If `location` is a negative number, then it will measure from the end of the string rather than the beginning.  If `replacement` is specified and not null, it will return the original string rather than the substring, but the substring will be replaced by replacement regardless of what `location` is.  And if replacement is specified, then it will override some of the logic for the `param` type and always return just a string and not a list.  If `stride` is zero or unspecified, then it explodes the string by character per UTF-8 parsing.  If `stride` is specified, then it breaks it into chunks of that many bytes.  For example, a `stride` of 1 would break it into bytes, whereas a `stride` of 4 would break it into 32-bit chunks.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(substr "hello world")
```
Output:
```amalgam
"hello world"
```
Example:
```amalgam
(substr "hello world" 1)
```
Output:
```amalgam
"ello world"
```
Example:
```amalgam
(substr "hello world" 1 8)
```
Output:
```amalgam
"ello wo"
```
Example:
```amalgam
(substr "hello world" 1 100)
```
Output:
```amalgam
"ello world"
```
Example:
```amalgam
(substr "hello world" 1 -1)
```
Output:
```amalgam
"ello worl"
```
Example:
```amalgam
(substr "hello world" -4 -1)
```
Output:
```amalgam
"orl"
```
Example:
```amalgam
(substr "hello world" -4 -1 (null) 1)
```
Output:
```amalgam
"orl"
```
Example:
```amalgam
(substr "hello world" 1 3 "x")
```
Output:
```amalgam
"hxlo world"
```
Example:
```amalgam
(substr "hello world" "(e|o)")
```
Output:
```amalgam
"e"
```
Example:
```amalgam
(substr "hello world" "[h|w](e|o)")
```
Output:
```amalgam
"he"
```
Example:
```amalgam
(substr "hello world" "[h|w](e|o)" 1)
```
Output:
```amalgam
["he"]
```
Example:
```amalgam
(substr "hello world" "[h|w](e|o)" "all")
```
Output:
```amalgam
["he" "wo"]
```
Example:
```amalgam
(substr "hello world" "(([h|w])(e|o))" "all")
```
Output:
```amalgam
["he" "wo"]
```
Example:
```amalgam
(substr "hello world" "[h|w](e|o)" -1)
```
Output:
```amalgam
[
	["he" "e"]
]
```
Example:
```amalgam
(substr "hello world" "[h|w](e|o)" "submatches")
```
Output:
```amalgam
[
	["he" "e"]
	["wo" "o"]
]
```
Example:
```amalgam
(substr "hello world" "(([h|w])(e|o))" "submatches")
```
Output:
```amalgam
[
	["he" "he" "h" "e"]
	["wo" "wo" "w" "o"]
]
```
Example:
```amalgam
(substr "hello world" "(?:([h|w])(?:e|o))" "submatches")
```
Output:
```amalgam
[
	["he" "h"]
	["wo" "w"]
]
```
Example:
```amalgam
;invalid syntax test
(substr "hello world" "(?([h|w])(?:e|o))" "submatches")
```
Output:
```amalgam
[]
```
Example:
```amalgam
(substr "hello world" "(e|o)" (null) "[$&]")
```
Output:
```amalgam
"h[e]ll[o] w[o]rld"
```
Example:
```amalgam
(substr "hello world" "(e|o)" 2 "[$&]")
```
Output:
```amalgam
"h[e]ll[o] world"
```
Example:
```amalgam
(substr "abcdefgijk")
```
Output:
```amalgam
"abcdefgijk"
```
Example:
```amalgam
(substr "abcdefgijk" 1)
```
Output:
```amalgam
"bcdefgijk"
```
Example:
```amalgam
(substr "abcdefgijk" 1 8)
```
Output:
```amalgam
"bcdefgi"
```
Example:
```amalgam
(substr "abcdefgijk" 1 100)
```
Output:
```amalgam
"bcdefgijk"
```
Example:
```amalgam
(substr "abcdefgijk" 1 -1)
```
Output:
```amalgam
"bcdefgij"
```
Example:
```amalgam
(substr "abcdefgijk" -4 -1)
```
Output:
```amalgam
"gij"
```
Example:
```amalgam
(substr "abcdefgijk" -4 -1 (null) 1)
```
Output:
```amalgam
"gij"
```
Example:
```amalgam
(substr "abcdefgijk" 1 3 "x")
```
Output:
```amalgam
"axdefgijk"
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `concat`
#### Parameters
`[string str1] [string str2] ... [string strN]`
#### Description
Concatenates all strings and evaluates to the single resulting string.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(concat "hello" " " "world")
```
Output:
```amalgam
"hello world"
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `parse`
#### Parameters
`string str [bool transactional] [bool return_warnings]`
#### Description
String `str` is parsed into code, and the result is returned.  If `transactional` is false, the default, it will attempt to parse the whole string and will return the closest code possible if there are any parse issues.  If `transactional` is true, it will parse the string transactionally, meaning that any node that has a parse error or is incomplete will be omitted along with all child nodes except for the top node.  If any performance constraints are given or `return_warnings` is true, the result will be a tuple of the form [value, warnings, performance_constraint_violation], where warnings is an assoc mapping all warnings to their number of occurrences, and perf_constraint violation is a string denoting the constraint exceeded (or (null) if none)), unless `return_warnings` is false, in which case just the value will be returned.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(parse "(seq (+ 1 2))" .true))
```
Output:
```amalgam
(seq
	(+ 1 2)
)
```
Example:
```amalgam
(parse "(seq (+ 1 2) (+ " .true))
```
Output:
```amalgam
(seq
	(+ 1 2)
)
```
Example:
```amalgam
(parse "(seq (+ 1 2) (+ " .false .true))"
```
Output:
```amalgam
[
	(seq
		(+ 1 2)
		(+)
	)
	["Warning: 2 missing closing parenthesis at line 1, column 17"]
]
```
Example:
```amalgam
(parse "(seq (+ 1 2) (+ " .true .true))"
```
Output:
```amalgam
[
	(seq
		(+ 1 2)
	)
	["Warning: 1 missing closing parenthesis at line 1, column 17"]
]
```
Example:
```amalgam
(parse "(seq (+ 1 2) (+ (a ) 3) " .true .true))"
```
Output:
```amalgam
[
	(seq
		(+ 1 2)
	)
	["Warning: Invalid opcode \"a\"; transforming to apply opcode using the invalid opcode type at line 1, column 19"]
]
```
Example:
```amalgam
(parse "(6)"))
```
Output:
```amalgam
(apply "6")
```
Example:
```amalgam
(parse "(not_an_opcode)"))
```
Output:
```amalgam
(apply "not_an_opcode")
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

### Opcode: `unparse`
#### Parameters
`code c [bool pretty_print] [bool sort_keys] [bool include_attributes]`
#### Description
Code is unparsed and the representative string is returned. If `pretty_print` is true, the output will be in pretty-print format, otherwise by default it will be inlined.  If `sort_keys` is true, the default, then it will print assoc structures and anything that could come in different orders in a natural sorted order by key, otherwise it will default to whatever order it is stored in memory.  If `include_attributes` is true, it will print out attributes like comments, but by default it will not.
#### Details
 - Permissions required:  none
 - Allows concurrency: false
 - Requires entity: false
 - Creates new scope: false
 - Creates new target scope: false
 - Value newness (whether references existing node): new
#### Examples
Example:
```amalgam
(unparse (parse "(print \"hello\")"))
```
Output:
```amalgam
"(print \"hello\")"
```
Example:
```amalgam
(parse (unparse (list (sqrt -1) (null) .infinity -.infinity)))
```
Output:
```amalgam
[(null) (null) .infinity -.infinity]
```
Example:
```amalgam
(unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma")))
```
Output:
```amalgam
"{a 1 b 2 c [\"alpha\" \"beta\" \"gamma\"]}"
```
Example:
```amalgam
(unparse (associate "a" 1 "b" 2 "c" (list "alpha" "beta" "gamma")) .true)
```
Output:
```amalgam
"{\r\n\ta 1\r\n\tb 2\r\n\tc [\"alpha\" \"beta\" \"gamma\"]\r\n}\r\n"
```

[Amalgam Opcodes](./amalgam_overview.md#amalgam-opcodes)

