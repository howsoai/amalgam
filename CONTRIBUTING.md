# Contributing

This Howso&trade; opensource project only accepts code contributions from individuals and organizations that have signed a contributor license agreement. For more information on contributing and for links to the individual and corporate CLAs, please visit: https://www.howso.com/cla

# General Guidelines
In general, there are a few general principles to keep in mind for contributing.

1. The interpreter should attempt to return as valid result as possible, and should never error out or crash. If the parameters are nonsensical, returning a null is the preferred approach. Optionally, a warning or error may be emitted to stderr, and making warnings for likely improper use is planned for future development. The interpreter should ideally never crash.
1. When adding functionality, consider whether it could be included in an existing operator / opcode via an additional parameter or expansion of parameter types. Effective comparing, merging, and generating of code works best when the branching factor of opcode types is not too large.
1. Efficiency and multithreading are important. Please do not include something that will slow it down unless there is a significant value gain. In particular, the query engine has been highly optimized, and adding a single branch in some of the inner loops can reduce performance on some applications by significant amounts.
1. Look around and ask the community before undertaking a big effort. Perhaps someone else is already working on it, or perhaps there's a reason why something has not been done yet.
1. Cross-platform support is important. Any contribution should in theory compile on all major compilers for all major operating systems and all major hardware supported. It's not expected that every contributor has every environment handy, but automated testing will hopefully reveal gaps.
1. If submitting a bug, please try to find a minimally reproducing test first.
1. Follow the coding style conventions listed in this document.
1. Prior to using **any** new 3rd party library, the library must be reviewed for utility, license considerations, and security. A library that adds considerable functionality, only requires cross-platform standard C++11, only requires including a couple of files, with good documentation and readable code, and has a vibrant and active international community is highly likely to be accepted. An unknown library with one anonymous contributor, restrictive license, with poor documentation and code quality, which is difficult to use, requiring complex additions to the build process is highly likely to be rejected no matter the potential functionality. Between these extremes is room for discussion.
1. There is no "my code", "your code", and "Bob's code". This means:
    -   No @author or other specific attribution in source code
    -   Anyone can work on any part of the system, as long as they have the
        necessary knowledge and skill and it makes sense for them to do so
    -   Do say:
        -   "There's a bug in X that manifests when the Y method is called with Z."
    -   Don't say:
        -   "There's a bug in Bob's code where it interacts with the module
            Alice contributed last week."

# Coding Style

1.  Properly name your variables. We're not competing for "least amount
    of code written to get it working". Long variable names are
    acceptable when they aid understanding. Use your best judgement. Be consistent and
    sufficiently descriptive.

1.  Do not add unnecessary comments when the function of the code is
    immediately obvious.

        int num_widgets = 0; //number of widgets

1.  Add comments to any code that would not be immediately obvious to
    another developer that has never seen that block of code.
    Complex algorithms should have outlines in comments as to how they work.

1.  Someone who has never seen your code should be able to read it,
    understand it, and find functional areas quickly. Whitespace should
    be used as "phrasing" (as in music) to group code into chunks that a
    person can easily understand, with comments to make it easier to
    navigate. If a person needs to read 10 lines of code to understand
    what it does, it should have a one-line comment so the person
    doesn't need to read it. Put yourself in the shoes of someone who
    has never seen your code before but must quickly fix an important
    bug correctly.

1.  Generally, comments should explain **why** and **how**, not **what**.

1. Comments should usually start above the code and use //. On rare occasions,
   comments can make sense to be on the same line, but especially for large and complex if/else blocks.

1.  Be consistent with the rest of the code base when working on
    existing projects. This usually means you should follow the
    standards for whatever language you're coding.

1.  Sufficient whitespace should be used between numbers, operators,
    variables, functions, etc. so that people with dyslexia and other reading concerns
    can efficiently read the code.

        //bad
        for(size_t i=0;i<GetValue(m)/2;i++)
            SetOtherValue(i/2-m+1);

        //good
        for(size_t i = 0; i < GetValue(m) / 2; i++)
            SetOtherValue(i / 2 - m + 1);

1.  Do not put a space between flow control statements and parentheses.
    As this has become common in some IDEs, you may need to change from the default options.

        //no space between "while" and the open paren
        while(true)
        {
            doStuff();
        }

1. Any TODO comment in main should have an associated ticket.

        //do this:
        //TODO 26837: get value from parent entity
        std::string value = "good default";

        //this sort of comment is discouraged but acceptable in some situations:
        //double precision may be overkill because we're returning a 32 bit result
        double density = static_cast<double>(mass) / volume;

        //don't do this:
        //TODO: set appropriate error code. Or maybe just forget about this unless it becomes a bug.
        int error_code = -1;

1. Use tabs for indentation, one tab per level. Use spaces after the initial tab length.

1. Preprocessor macros should be one indentation fewer than the code to which it applies.

1. Curly braces go on their own lines except for do-while loops.

1. CamelCase should be used for classes, functions, and methods,
lowerCamelCase for all attributes, and lower\_snake\_case for all
variables on the stack or global (parameters, local variables, etc.).

1. Header include order is the following, and within each header group,
   headers should be sorted alphabetically.
    1. Project (local to current project)
    1. 3rd party (local to current project)
    1. System/compiler
    1. Forward declarations

1. Explicit and specific types are preferrable in declarations, but auto can be allowed. E.g., int32_t is preferable to int.

1. Grammar and writing style should aspirationally follow the [Chicago Manual of Style](https://en.wikipedia.org/wiki/The_Chicago_Manual_of_Style).
