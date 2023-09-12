# Amalgam Beginner Guide

This is a colloquial guide for beginners to get started programming with Amalgam.  It assumes some familiarity with programming.  For detailed documentation on the language, see the [Amalgam Language Reference](https://howsoai.github.io/amalgam).

## Amalgam : code-is-data-is-code

Amalgam uses [S-expressions](https://en.wikipedia.org/wiki/S-expression) as its operators, which are a pair of parenthesis surrounding an opcode and its parameters.
One way to think about this is: every operator *is* a function. So when you see `(+ 2 1)`, you can read that
as "add two and one", or "call the function named '+' with
parameters of 2 and 1".
Since the scope of all operators and operations is explicit, there is no ambiguity with regard to order of operations.

Examples:

`(+ 2 3)`
&gt; 5
`(- 7 1)`
&gt; 6
`(- 1 7)`
&gt; -6
`(* 4 3)`
&gt; 12
`(/ 16 2)`
&gt; 8
`(+ 1 2 3 4 5)`
&gt; 15
`(* 2 5 (+ 3 4))`
&gt; 70

>     #Python
>     print("hello world")
>
>     ;Amalgam
>     (print "hello world")

For a list of all operations, see the [Amalgam Language Reference](https://howsoai.github.io/amalgam).


# Scripting
Amalgam is an interpreted (scripting) language.  To run Amalgam files, typically with a *.amlg* extension,
you can simply use the Amalgam binary to run a script, for example if your script is named *my_script.amlg*:

`/path/to/bin/amalgam my_script.amlg`

Due to the syntax of `#!` being a private labeled variable, the traditional unix [shebang](https://en.wikipedia.org/wiki/Shebang_(Unix)) with the Amalgam interpreter works as expected.

The first outermost operation will be executed in the script, thus most standalone script code should be in
a `(seq` block, since that is a single function that executes everything in it sequentially.
Example contexts of a script file:

```
(seq
    (print "hello ")
    (* 2 2)
    (print "there " )
)

that code above prints "hello there " but anything else can go here since it is ignored and
only that first function is executed.
The 2*2 is evaluated but is not displayed because it is not inside a (print) statement
(print "general Kenobi\n") ; is ignored

```

Thus for the purposes of this user guide, assume that all below examples are being executed inside a `(seq` block.
Since all code is a list whose contents are being evaluated, the code
could just as easily put all your code inside a **(list** block as
well.  **(list** and **(seq** are identical, except that
**(seq** will return whatever the last item inside it evaluates to,
whereas **(list** does not, making **seq** more efficient for the purposes of evaluating code sequentially.

    (print (list 1 2 3)) ; prints the list itself, i.e., (list 1 2 3)

    (print (seq 1 2 3)) ; prints 3, since that's the last item in the list to be evaluated

Since both are lists, if you were to **(get** an item from a
specific index from either of them, you'd get the same result:

    (print
        (get (list 1 2 3) 1) ; get the item at index 1, which is a 2

        (get (seq 1 2 3) 1) ;same
    )


Data Structures:
---------------

The data types of amalgam consist of immediate values, which are string and number, lists, which are ordered sets of elements and have an opcode associated with it (which may be list), and assoc, which is an associative array of key-value pairs.  Code is just a list with a different opcode.

Indices of lists are 0-based, and keys of an assoc are referred to as
the indices of an assoc.  This concept unifies assocs and lists so that
you may think of lists as assocs where the index is the 'key' for each
value in a list.  The order of items in an assoc is never guaranteed.

    (declare (assoc

        indices_of_my_list (indices (list 10 20 30 40))         ;returns (list 0 1 2 3)
        indices_of_my_assoc (indices (assoc "x" 2 "y" 3 "z" 4)) ;returns just the 'keys', (list "x" "z" "y"), NOTE: order of indices in an assoc is not guaranteed
        values_of_my_list (values (list 10 20 30 40))           ;return the exact same list (list 10 20 30 40) since the values of a list are the list itself
        values_of_my_assoc (values (assoc "x" 2 "y" 3 "z" 4))   ;returns just the values (list 4 3 2)  NOTE: order of values in an assoc is not guaranteed

    ))

Also note that `(assoc foo 3 bar 5)` is same as `(assoc "foo" 3 "bar" 5)`, the quotes around
the indices (keys) are *optional*. This means that **(assoc** uses the literal string value of the
keys as provided to it. Thus if you have a variable named 'foo' and you intended the key to be the value
of that variable instead of it being "foo", you need to use the opcode **(associate** instead.
**(associate** evaluates the *keys* of an assoc and should be used anytime the *keys* are
either variables or output of some code. The values of an assoc are always evaluated.

    (declare (assoc foo "my_key" bar 5)) ; create a 'variable' named "foo" that has the value of "my_key"
    (print (assoc foo bar))          ;prints (assoc foo 5) - the key was not evaluated
    (print (associate foo bar)       ;prints (assoc my_key 5) - the key was evaluated

    ;use (associate if the key is the result of output of some method:
    (print (associate (call GenerateUUID) "hello"))  ;prints (assoc "UUID_KEY_GOES_HERE" "hello")


Also note that once you declare a variable and it exists in the context
you cannot use **(declare** to overwrite it. Since variables are actually
just keys of an assoc that are on the stack, and those key already exist,
if you want to change their values, you need to use the **(assign** opcode:

    (assign (assoc x 4 y 8)) ; overwrites the values of x and y accordingly


For example, after you create a variable called **my\_list** that has a
list of letters:

    (declare (assoc my_list (list "a" "b" "c")))

if you want to now edit **my\_list** and append the letter "d" to it,
if you simply do this:

    (append my_list "d")

nothing will happen because even though the code is evaluated, we didn't
'do' anything to the evaluated outcome, we didn't store it into anything!  To update what value **my\_list** stores we have to do this:

    (assign (assoc my_list (append my_list "d")))


To make this easier, Amalgam has the **(accum** opcode that can be used as such:

    (accum (assoc my_list d"))

More examples of basic list operations:

    (seq

        ;declare a couple of lists of letters
        (declare (assoc
            kitty (list "A" "B" "C" "D" "E")
            bunny (list "x" "y" "z")
        ))

        ;different types of list operations
        (declare (assoc
            first_in_kitty (first kitty) ; result is "A"

            last_in_kitty (last kitty) ; result is "E"

            ;(trunc removes items from the end of a list
            truncate_1_item_in_kitty (trunc kitty) ; result is (list "A" "B" "C" "D")

            truncate_all_items_in_kitty_leaving_2 (trunc kitty 2) ; result (list "A" "B" )

            truncate_2_items_in_kitty (trunc kitty -2) ; result (list "A" "B" "C" )


            ;(tail removes items from the front of a list
            remove_1_item_from_front_of_kitty (tail kitty) ; result is (list "B" "C" "D" "E")

            remove_all_from_front_of_kitty_leaving_2 (tail kitty 2) ; result is (list "D" "E")

            remove_2_items_from_front_of_kitty (tail kitty -2) ; result is (list "C" "D" "E")

            ;(append is straight forward
            kitty_and_bunny (append kitty bunny) ;result is (list "A" "B" "C" "D" "E" "x" "y" "z")

            size_of_kitty (size kitty) ;result is 5

            reverse_of_kitty (reverse kitty) ;result is (list "E" "D" "C" "B" "A")
        ))

    )

# Variables and Scope

There are several of ways to declare variables in Amalgam. One way is to use `(declare (assoc`.

`(declare` = creates the variable in the current context (current scope), but only if it does not already exist.

>     ;Amalgam
>     (declare (assoc h "hamster"))
>     (print h)
>     ;;outputs: hamster
>
>     # Python equivalent:
>     h = "hamster"
>     print(h)

Another way is `(let`, which makes the variable available *inside* the operator (local scope).

>     ;Amalgam
>     (let (assoc y "yamster"))
>     (print y)
>     ;;outputs: null
>
>     //Java equivalent:
>     if(true) {
>         String y = "yamster"
>     }
>     System.out.print(y);
>
>
>     ;Amalgam
>     (let
>         (assoc y "yamster")
>         (print y)
>     )
>     ;;outputs: yamster
>
>     //Java equivalent:
>     if(true) {
>         String y = "yamster"
>         System.out.print(y);
>     }

**Important distinction**
`(let (assoc` and `(declare (assoc` create and initialize the variables,
they do **not** overwrite an already existing variables.
Use `(assign` to set previously declared variables.

>     //javascript
>     var x = 5; //declare and set variable x to 5
>     x = ["a","b","c"]; //sets variable x to a list of letters instead
>
>
>     ;Amalgam
>     (declare (assoc x 5)) ;declare and set variable x to 5
>     (declare (assoc x (list "a" "b" "c"))) ;does nothing because x has already been declared
>     (assign (assoc x (list "a" "b" "c"))) ;sets variable x to a list of letters instead

More examples with descriptions:


    (seq
     ;a (declare (assoc will create an assoc of key -> value pairs where the values can be code itself.
     ;note: the declaration can be treated as though it's done in parallel, so you CANNOT use values in the same declare to
     ; calculate subsequent values like so:
       (declare (assoc x 3 y 2 foo (* x y)))
       (print foo "\n") ;outputs 0 because foo has already been evaluated, and when it was, x and y were nulls
    )

    (seq
     ;if you want to use declared values to make new values, you have to chain the declare statements like so:
       (declare (assoc x 3 y 2))
       (declare (assoc foo (* x y))) ;the multiplication is evaluated right here so the result is stored in foo
       (print foo) ;thus we get the expected result of 6 here
    )


    ;if we want foo to be a function, we need to make sure the code isn't evaluated right away, to do that we wrap it in a 'lambda'
    (seq
       (declare (assoc x 3 y 2))
       (declare (assoc foo (lambda (* x y)))) ;the multiplication is stored as the code itself, WITHOUT being evaluated

       (print "foo: " foo "\n") ;thus this returns the unevaluated code for the multiplication that's stored into foo

     ;now that the code in foo is not evaluated, to evaluate it we can 'call' it:
       (print "calling foo: "
         (call foo) ; this calls (evaluates) foo, which uses the values of x and y in the scope and thus returns a 6
         "\n"
       )

     ;since the code in foo is not evaluated until we actually call it, we can pass in parameters for x and y and evaluate it with
     ;those parameters
       (print "calling foo w/ params: "
         (call foo (assoc x 4 y 8))  ;and now we have what most developers would consider a 'function' or 'method'
         "\n"
       )
    )

# Conditionals

Amalgam's `(if` operator takes a series of condition / action pairs,
followed by an *optional* default action.

>     //Groovy
>     x = 4
>     if(x < 3){
>       print "x is tiny"
>     } else if(x == 3){
>       print "x is exactly 3"
>     } else {
>       print "x is huuge"
>     }
>
>     ;Amalgam
>     (let
>         (assoc x 4)
>         (if (< x 3)
>             (print "x is tiny")
>             (= x 3) ;else if
>             (print "x is exactly 3")
>             ;else
>             (print "x is huuge")
>         )
>     )

# Loops and Maps

The amalgam syntax is a little different than some other languages,
but the overall idea is the same.  However, functional programming is strongly recommended,
as while loops containing an accum can be considerably slower and consume notably more memory.

>     //Java
>     // print values 1-10
>     for(int i = 0; i < 10; i++){
>         System.out.print(i);
>     }
>
>     ;Amalgam
>     ;This is a classic while loop, but not recommended as it has an accum within it
>     ; Loops:
>     (let
>         (assoc i 0)
>         (while (< i 10)
>             (print i) ; do stuff here
>             (accum (assoc i 1)) ;increment i by 1
>         )
>     )
>
>
>     ;Maps:
>     ;Maps serve the same purpose as loops, but in a more functional way.  Maps are also an easy and efficient way to iterate over a series of values that may not be a sequence
>     ;and are not guaranteed to execute in order (i.e., map may utilize parallel processing, especially if preceeded by ||)
>     ; Print values from 1 to 10
>     (map
>         (lambda
>             (print (target_value)) ;run this on each item from the list
>         )
>         (range 0 9) ;generate a list from 0 through 9
>     )


# Functions

Functions in Amalgam *should* be implemented using labels to follow
coding standards, but can be unlabeled variables as well.

>     //Javascript
>     function mul(x, y){
>         return x * y;
>     }
>     console.log( mul(4,2) );
>     //outputs: 8
>
>     ;Amalgam
>     #mul (* x y)
>     (print (call mul (assoc x 4 y 2)))
>     ;outputs: 8
>
>     ;unlabeled function definition:
>     (declare (assoc mul (lambda (* x y))))

Notes:

`(lambda` means we're going to define a function (or any code) but we
are not going to evaluate it. In this example, it stores the
function/code itself into the variable.
We pass in parameters to a function as an `(assoc` with the variables as
the keys of that assoc.
`(call` evaluates/executes/runs the code inside the lambda.

Functions with default parameters:

>     //Javascript
>     //function to multiple all values in the list by each other and then to multiply them result by the specified factor
>     //default my_list to be a list of 1 if it's not specified
>     function multiplyValuesInList(my_list = [1], factor = 2) {
>         return my_list.reduce((a, b) => a * b * factor);
>     }
>
>     console.log(multiplyValuesInList());
>     //outputs: 2
>     console.log(multiplyValuesInList([3,2],3));
>     //outputs: 18
>
>     ;Amalgam
>     #multiplyValuesInList
>         (declare
>             (assoc
>                 my_list (list 1)  ;specify the parameters and what the default values are
>                 factor 2
>             )
>             (* (apply "*" my_list) factor)
>         )
>
>     (call multiplyValuesInList)
>     ;;outputs: 2
>     (call multiplyValuesInList (assoc my_list (list 3 2) factor 3))
>     ;;outputs: 18

# Entities (Objects)

Objects in Amalgam are called **entities**.
The base script being executed is an entity itself.

To create 'child' contained entities, use `(create_entities`, to load
existing code as a contained entity use `(load_entity` instead.
Once there are contained (aka "child") entities, you can call their
functions via `(call_entity` or retrieve their data
via `(retrieve_from_entity`.

The 'parent' container entity has full access to its full hierarchy of
contained entities (all its children entities and grand children, etc.)
Child entities, however only have access to their parent container
entity's labels that are marked with a **\#^**.

>     //Java
>     public class Car {
>         private String color = "white";
>         public void setColor(String c) { this.color = c; }
>         public String getColor() { return this.color; }
>         public void drive(int speed) {
>             if(speed < 35) {
>                 System.out.println("slow " + color);
>             }
>             else {
>                 System.out.println("vroom " + color);
>             }
>         }
>     }
>
>     Car myCar = new Car();  //instantiate a car
>     myCar.setColor("blue"); //set color
>     myCar.drive(67);        //drive fast
>
>
>     ;;Amalgam
>     ;creating a named entity will "instantiate" so that you can refer to it by name, in this example we name it "car"
>     (create_entities "car"
>         (lambda (null
>             #color "white"
>
>             #drive
>                 (declare
>                     (assoc speed 0) ;parameter, default to 0
>                     (if (< speed 35)
>                         (print "slow " color "\n")
>                         ;else
>                         (print "vroom " color "\n")
>                     )
>                 )
>         ))
>     )
>     (assign_to_entities "car" (assoc color "blue")) ;set color
>     (call_entity "car" "drive" (assoc speed 67))  ;drive fast
>
>
>     ;...alternatively you could create an entity and assign it to a variable and then refer to it using the variable:
>     (declare (assoc
>         myCar
>             (create_entities
>                 (null
>                     ;todo: copy-pasta code from the above (null
>                 )
>             )
>     ))
>     (assign_to_entities myCar (assoc color "blue")) ;set color
>     (call_entity myCar "drive" (assoc speed 67))  ;drive fast

# Labels

Entity attributes are denoted by "labels". To label an operator, just place a
label in front or above the referenced opcode. Labels are petty much
annotations and references to code and data; in object oriented programming,
they are for denoting creating methods and attributes, though an operator can have multiple labels.

    #foo
    #bar
        5

This will attach the labels **foo** and **bar** to the immediate number 5 as
instantiated in the current location of the current entity,
allowing you to use either of them in code as variables, checked after all other lexical scopes:

    (print foo " toes and " bar " fingers\n")

or you can label more complex code:

    (seq
        (null
            #code_block
            (list "a" "b" #third_value "c")
        )

        (print (get code_block 1)) ;gets the value at index=1, prints b

        (print third_value) ;prints c since that's what this label is referencing

)

The explanation for this code above is that **\#code\_block** is in
front of the list, therefore it references the entire list, whereas
**\#third\_value** is in front of the literal string "c" so it
references just that value.
According to the style guide you should put labels on their own lines above the code you want to attach them to.
In the above example, the label **\#third\_value** is not on its own line, but it still references whatever code is immediately after it.
Additionally, **\#code\_block** is placed in a null so that it won't be executed by the seq.


There are 4 types of labels in Amalgam:
  - `#regular_label` labels accessible by this entity and all 'parent'
container entities, but not 'child' contained entities
  - `#^public_label` labels accessible by everyone, contained and container
entities
  - `#!private_label` labels accessible only by this entity, not contained
and not container entities
  - `##inaccessible_label` multiple-\# means the label, whether its regular,
public or private, are innaccessible by anyone until they are evaluated


# More Advanced Operations

### Apply

The **(apply** opcode takes whatever code block you pass it, and changes the opcode to whatever the new opcode you specified, and then evaluates that block of code:

usage: *(apply lambda(&lt;your\_new\_opcode&gt;)
&lt;your\_code\_you\_want\_the\_new\_opcode\_applied\_to&gt;)*

    (seq
        (declare (assoc hamsters (list 2 1 3 4 5)))

        ;result is 15 - this will apply the function (+ to the list of items in hamsters
        (declare (assoc summed_up (apply (lambda (+)) hamsters) ))
     )

Essentially what happens is the code **(list** 2 1 3 4 5) *becomes*
**(+** 2 1 3 4 5), i.e., the **(list** opcode is swapped out for a
**(+** and the code block is then evaluated.

You may use shorthand for lambda inside **(apply** by using double
quotes, like so: **(apply "+" hamsters),** though double quotes must be
used if using apply as a 'cast' operation, to cast code to a type that
doesn't have an opcode, (i.e. **string,** **number** or **symbol**).

### Zip and Unzip

Zip behaves like a zipper.  It takes two lists and converts them into an assoc.  Unzip is the same but in reverse.  It returns a list of values that correspond to the specified list of keys.

usage: *(zip &lt;list\_of\_keys&gt; &lt;list\_of\_values&gt;)*
usage: *(unzip &lt;assoc&gt; &lt;list\_of\_keys&gt;)*

    (seq
        (declare (assoc
            my_keys (list "a" "b" "c")
            my_vals (list 2 4 8)
        ))

        (declare (assoc

            ;results in: (assoc "a" 2  "b" 4 "c" 8)
            my_map (zip my_keys my_vals)

            ;if values list is not provided, Amalgam defaults it to nulls, the result is: (assoc "a" (null) "b" (null) "c" (null))
            just_keys_no_values_map (zip my_keys)
        ))

        ;now that we created an assoc, we can try to unzip it:
        (declare (assoc

            ;result is (list 4 8)  because those were the values for keys "b" and "c", which is what we passed into the unzip
            couple_of_values (unzip my_map (list "b" "c"))
        ))
    )

You can use unzip on lists as well, for lists the 'key' are their
indices: you can think of assocs as key→value pairs with custom defined
keys, and lists as key→value pars where the keys are indices.

Therefore if we want values from a list at indices 1 and 2, we can pass
in those indices into the unzip:

    (declare (assoc

       ;result is (list "b" "c") because those are the values corresponding to the indices in that list
        couple_of_list_values (unzip my_keys (list 1 2))

    ))

### Get

Get retrieves a value from a list or an assoc.

usage: *(get &lt;code&gt;&lt;index&gt;)*

    Getting an individual value from a list or an assoc is basic - you just specify the index of the item you want:
    (seq
        (declare (assoc
            numbers (list 10 20 30 40 50)
            numbers_map (assoc "a" 10 "b" 20 "c" 30)
        ))

        (declare (assoc

            ;returns 30 since that's the third value (at index 2)
            third_value (get numbers 2)

            ;returns 20 since that's the value for key "b"
            value_for_key_b (get numbers_map "b")
        ))
    )


Getting a value that is inside a nested structure requires you to
specify the indices of each level of the nesting you want in order,
using a list as the parameter:

    (seq
        (declare (assoc
            numbers (list 10 20 (list "a" "b") 40 50)
            numbers_map (assoc "a" 10 "b" 20 "c" (assoc "A" 1 "B" (list 2 4 8)))
        ))

        (declare (assoc
            ;returns "a" - we specified that we want to look at item that's at index 2, and inside that item we want to look at what's at index 0
            first_value_from_third_value (get numbers (list 2 0))

            ;return 4 - look at value for key "c", inside that look at item for key "B", inside that look at item at index 1
            my_nested_value (get numbers_map (list "c" "B" 1)
        ))
    )



### Set

Set is the same as **(get** above, except you pass in one more parameter specifying what to set the value to, instead of returning it.

usage: *(set &lt;code&gt; &lt;index&gt; &lt;new\_code&gt;)*

    (seq
        (declare (assoc
            numbers (list 10 20 30 40 50)
            numbers_map (assoc "a" 10 "b" 20 "c" 30)
        ))

        (declare (assoc
            ;returns (list 10 20 33 40 50) since we've set the value that's at at index 2
            changed_third_value (set numbers 2 33)

           ;returns (assoc "a" 10 "b" 22 "c" 30) since we've set the value for key "b"
            changed_value_for_key_b_map (set numbers_map "b" 22)
        ))
    )

Setting values for nested structures also works the same way, you
specify a list of how to 'walk' into the nested structure.


### Sort:

Allows users to write their own comparators by using **(target_value 1)** and **(target_value)** (often referred to as 'a' and 'b' in other languages) as it processes the list. Details on the **(target_value)** opcode and its scope offset parameter can be found farther below.

    (seq
        (declare (assoc hamsters (list 2 1 3 4 5)))
        (declare (assoc
            sorted (sort hamsters) ) ; result is (list 1 2 3 4 5)

            ;as the items in the list are being iterated over, they are set to the built-in opcodes for
            ;current value: (target_value) and previous value (target_value 1)
            ;and if you specify the comparison method, it'll use the output of that comparison to select the order
            reverse_sorted (sort (lambda (< (target_value) (target_value 1))) hamsters)  ; result is (list 5 4 3 2 1)

    )

###  Reduce

Collapses all items into one using a custom operation.

usage: *(reduce (lambda &lt;your\_custom\_operation&gt;) &lt;data&gt;)*

During your custom operation, you can use the two built-in opcodes
**(target_value)** and **(target_value 1). (target_value 1)** is the reduced result during the iteration, while
**(target_value)** is the current item being iterated on.

For example if you have a list of numbers that you want to add up, **(target_value 1)**
will keep a running total, while **(target_value)** will be set to the value of the
next item to be added:

    (reduce (lambda (+ (target_value 1) (target_value))) (list 1 2 3 4))

The internal iteration would be as follows:

```
(target_value 1)=1 (target_value)=2

(target_value 1)=3 (target_value)=3

(target_value 1)=6 (target_value)=4

result: 10
```

### Filter and Map

Filter returns elements that evaluate to true via a custom filtering function, whereas map transforms each element via the custom function.

usage: *(map &lt;custom\_function&gt; &lt;data&gt;)*
usage: *(filter &lt;custom\_filter\_function&gt; &lt;data&gt;)*

**(target_value)** is set to the value of the current item, while **(target_index)** is set
to the index "key" of the current item.

    (seq
        (declare (assoc
            numbers (list 10 20 30 40 50)
            numbers_map (assoc "a" 10 "b" 20 "c" 30)
        ))

        (declare (assoc
            ;iterate over the numbers list and add the index of the number to the number, result: (list 10 21 32 43 54)
            modified_numbers
                (map
                    (lambda
                        (+ (target_value) (target_index))
                    )
                    numbers
                )

            ;iterate over numbers_map, and for each value, convert it into a list of numbers from 1 to that value
            ;result will be:  (assoc "a" (list 1 2 3 4 5 6 7 8 9 10) "b" (list 1 2 ...etc... 19 20) "c" (list 1 2 ...etc... 29 30))
            modified_numbers_map
                (map
                    (lambda
                       ;output a list of numbers from 1 to whatever value is stored in (target_value)
                       (range 1 (target_value))
                    )
                    numbers_map
                )

            ;filter leaves items on the list that match the specified condition and removes all others, results in (list 30 40 50)
            numbers_over_25
                 (filter (lambda (> (target_value) 25)) numbers)

            ;leave only those values whose (target_index) % 2 == 1, results in (list 10 30 50)
            odd_indices_only
                 (filter (lambda (= (mod (target_index) 2) 1)) numbers)

        ))
    )

Regarding **(target_value)** and **(target_index),** If you have
nested map statements, they will refer to the items being
iterated by the immediately scoped **(map** statement they are in.

    (seq
        (declare (assoc matrix (list (list 1 2 3) (list 10 20 30)) ))

        ;iterate over each row in the matrix and print out the values in each row, separating each row with a new line
        ;expected output:
        ;1 2 3
        ;10 20 30
        (map
            (lambda (seq
                ;iterate over each value in the row of data
                (map
                    (lambda
                         ;here (target_value) refers to each value in the row, print it with a space afterward
                        (print (target_value) " ")
                    )

                    ;here (target_value) refers to each item, a 'row' in matrix, which itself is a a list of values
                    (target_value)
                )

                ;print a new line after all the values in the row have been printed
                (print "\n")
            ))
            matrix
       )
    )


**(target_value <num>)** and **(target_index <num>)** are used to access the currently iterated value farther up the stack,
where <num> is how many 'layers' to go up. For example:

    (map
        (lambda
            (map
                (lambda
                   (print
                        (target_value) " " ; prints inner-most values of 10, 20, and 30
                        (target_index) " " ; prints inner-most indices of 0, 1, and 2
                        (target_value 1) " " ;prints the target_value from 1 level up the stack, a, b and c
                        (target_index 1) "\n' ; prints the target_index from 1 level up the stack  A, B, and C
                   )
                )
                (list "10" "20" "30")
            )
        )
        (assoc "A" "a" "B" "b" "C" "c") ; note: assocs aren't necessarily iterated in order since they are unordered hashmaps
    )

```
outputs:
10 0 b B
20 1 b B
30 2 b B
10 0 c C
20 1 c C
30 2 c C
10 0 a A
20 1 a A
30 2 a A
```

Important note regarding **(target_value)** and **(target_index)** and how their offset parameter works:
The following opcodes each have their own scope stack (in the Amalgam reference document, you'll see that these all have a '_Creates one or more new entries on target stack_' under their description):

    assoc
    filter
    list
    map
    reduce
    replace
    rewrite
    sort
    weave
    zip

    ; Example
    (map
       (lambda (let
           (assoc
               ; store target_value into x_val, must provide stack-offset of 1 because this is inside a (assoc) that has its own scope
               x_val (target_value 1)

               ; store wrapped in a list, must provide stack-offset of 2 because it's inside a (list) that has its own scope
               ; and it's inside the (assoc), therefore the value is 2 because it's nested two levels deep
               x_list (list (target_value 2))
           )

           (print
               x_val " = " (target_value) "\n" ; in original scope of the (map) statement, not inside a (assoc)
               x_list " = " (list (target_value 1)) "\n" ; wrapped in a list, must privode stack-offset of 1 because (list) has its own scope
           )
       ))
       (list 1 2 3)
    )


### Types

The following are different variations of null or nonexistant:
**(null)** - missing value (or code)
**.nan** - missing number value (Not A Number)
**.nas** - missing string value (Not A String)

To numerify something you can use `(+` operator:
`(+ "5")` converts the string 5 to the number 5

numerifying a null in any way or dividing 0 by 0 results in a `.nan`:
`(+ (null)` = `.nan`
`(* 5 (null))` = `.nan`
`(/ 0 0)` = `.nan`

To stringify something you can use the `(concat` operator:
`(concat 5)` converts the number 5 into the string 5

stringifying a null in any way results in a `.nas`:
`(concat (null))` = `.nas`

`(zip (list "a" (null)) (list 1 2))) ` = `(assoc "a" 1 .nas 2)`

To check the type of something you can use either **(get_type** to return the type itself
or **(get_type_string** to return the readable string version of the type:

`(get_type "hello")` = `""`
`(get_type 5)` = `0`
`(get_type (list 1 2 3))` = `(list)`

`(get_type_string "hello")` = `"string"`
`(get_type_string 5)` = `"number"`
`(get_type_string (list 1 2 3))` = `"list"`

### Entities
"Objects" in Amalgam are called **entities**.
The base script being executed is an entity itself.
Entities are named instances of Amalgam code.
The 'root' of an entity is its top-most function, thus a script calling `(retrieve_entity_root)` on itself will print its own code out.

Entities can contain other entities, aka *child* entities. The 'parent' container entity has full access to its full hierarchy of
contained entities (all its children entities and grand children, etc.) Child entities, however only have access to their parent container
entity's labels that are marked with a `#^`

To create contained entities use `(create_entities`, optionally specifying a name for each one. If you don't specify one, the system will
create one for youin the format of 10 digit number preceded by a _, e.g., "_2364810274".


```
(create_entities (lambda (null)) )
(create_entities "bob" (lambda (null)) )
```

The above code creates one empty entity with a system-generated name and one named *bob*.
To see the full list of contained entities use `(contained_entities)`.

You can create entities contained by other entities by specifying the hierarchy traversal path, for example, to create an entity contained by *bob* named *food* we would do this:
`(create_entities (list "bob" "food") (lambda (null)) )`
And to see all contained entities in *bob* we would do this:
`(contained_entities "bob")` or `(contained_entities (list "bob"))`

Since methods/functions are referenced via labels (see User Guide for details), we can call functions on child entities like so:

```
;create an entity named 'foo' with a method and a static value
(create_entities "foo" (lambda
    (null
        ##add_five (+ x 5) ;create a method that adds 5 to x
        ##static_value "STATIC"
    )
))
(print
    ;execute the method 'add_five' on entity 'foo' passing in 10 as the variable x
    (call_entity "foo" "add_five" (assoc x 10))
    " "
    ;retrieve the value stored in entity foo's label 'static_value'
    (retrieve_from_entity "foo" "static_value")
)
```
The above code should print out "15 STATIC".

### Loading Amalgam Files

You can load an existing file one of two ways:
1) as a contained entity and then access the entity
2) directly into your current file

As entity:
`(load_entity "filename.amlg" "blah")`
You now have a contained entity named *blah* that you can access via regular entity operations.

Directly into your file:

```
#load_label (null)
(direct_assign_to_entities (assoc load_label (load "filename.amlg")) )
```

The entire contents of your *filename.amlg* will be loaded into *load_label* and accessible directly in the rest of the script.
If the file you are loading is a standalone script, you may execute it via `(call load_label)`.

### Small Examples

```
;convert nulls to 0, while leaving non-nulls as-is:
(or my_value)

;convert values to numbers:
(+ my_value)

;convert values to strings:
(concat my_value)

;get rid of dupes in a list:
(indices (zip list_with_dupes))

;collapse a list of lists into a unique list:
(indices (zip (reduce (lambda (append (target_value) (target_value 1))) list_of_lists )))

;count instances of a 'value' in a list:
(size (filter (lambda (= (target_value) value) your_list)))

;return values at specific indices for a list (slow):
(filter (lambda (contains_value indices_list (target_index))) your_list)

;return values at specific indices for a list (fast):
(unzip your_list indices_list)

;check if a list is all of one value, e.g., (null):
(apply "=" (append your_list (list (null))))

;set a nested key 'third' in myassoc that's three levels down:
;python equivalent of myassoc['first']['second']['third'] = value_to_set
(assign "myassoc" (list "first" "second" "third") value_to_set)


;sort keys in an assoc by their corresponding values
(sort
    (lambda (> (get your_assoc (target_value)) (get your_assoc (target_value 1))))
    (indices your_assoc)
)

;convert from a list of assocs to a flat assoc
;i.e. (list (assoc ...) (assoc ...)) into (assoc ...)
(reduce (lambda (append (target_value) (target_value 1)) list_of_assocs)
or
(apply "append" list_of_assocs)

;check if small_list is a subset of big_list
;by converting small_list into an assoc and removing all big_list's keys from it
;there should be no keys remaining
(= 0 (size (remove (zip small_list) big_list)) )

;get all possible indices of a specific your_value in a list
(filter
	(lambda (= your_value (get your_list (target_value))))
	(indices your_list)
)

;convert a list of lists into a flat list
(apply "append" list)


;dynamically set parallelism (concurrency) based on some logic
(null
    #mymap (map (lambda (print (target_value) "\n")) (range 1 30))
)
(if run_mt
    (call (set_concurrency mymap (true)))
    (call mymap)
)
```

# Style Guide

**General rules:**

1.  **tabs**, not spaces
2.  snake case (e.g., this\_is\_a\_long\_variable\_name ) for local variables, camelCaseVariables for global variables
3.  spaces between operators and parenthesis,
good:`(/ (+ 3 4) 7)`
    bad:`(/(+ 3 4)7)`
4.  no more than **2** opening parenthesis per line *(see exception rule \#6)*
5.  no more than **2** closing parenthesis per line *(see exception rule \#6)*
6.  if the statement is simple and short enough to fit entirely on one line, go ahead, but use your judgement for readability
7.  closing parens should match opening parens in terms of indentation for any first operator on a line
8.  one tab indent for new lines continuing a statement
9.  no double indents to adhere to rule #7
10. since labels are treated as functions and attributes, a label that is a `(declare (assoc` statement treats that first `(assoc` like input parameters, try to avoid declaring variables in that initial statement that are not parameters to the function
11. all comments should go on their own lines, **above** the code they are commenting
12. append `_map` (or `_set`) to variable names that will only ever store assocs

>     ;bad: >2 opening parens and closing statement paren doesn't line up with opening paren - breaks rules #4 and #7
>     (declare (assoc test (/
>         (+ 3 4) 7)
>
>
>     ;while this indentation is fine, it's better to avoid single line indents like this since it's unnecessary
>     (declare
>         (assoc test (/ (+ 3 4) 7)) ;this is a poorly placed comment, it should be on its own line per rule #11
>     )
>
>     ;good example, all one line since the statement is simple per rule #6
>     (declare (assoc test (/ (+ 3 4) 7) ))
>
>
>     ;good example of single indent because both pairs of parens open and close
>     ;at the same indentation amount, per rules #4,#5 and #8
>     (declare (assoc
>         test (/ (+ 3 4) 7)
>         things (list "of" "stuff")
>     ))
>
>     ;old styling, acceptable but should be changed per rule #9.
>     ;avoid double indents. (assoc should be on its own line below, see next example.
>     (declare (assoc
>             test (/ (+ 3 4) 7)
>             things (list "of" "stuff")
>             stuff_map (assoc "key" "value")
>         )
>     )
>
>     ;good example of how the open parenthesis for declare and assoc match indentations per rule #7
>     (declare
>         (assoc
>             test (/ (+ 3 4) 7)
>             test2
>                 ;this comment is above the code it's for, and it's fine to one-line simple calls that only take a few parameters
>                 (call some_function (assoc param1 val1))
>             test3
>                 (call another_function (assoc
>                     param1 val1
>                     param2 val2
>                 ))
>         )
>     )
>
