Noct Syntax
===========

## Assignments

Variables in Noct are dynamically typed and don't require explicit
declaration. The assignment operator (`=`) is used to create and
assign values to variables.

As shown in the example below, Noct supports various data types
including integers, floating-point numbers, and strings. Variables can
be reassigned to different types at any time during execution.

```
func main() {
    var a = 123;
    print(a);

    var b = 1.0;
    print(b);

    var c = "string";
    print(c);
}
```

## Global Variables

Global variables can be defined in functions, and cannot be defined
outside functions.

```
func main() {
    globalVariable = 123;
    print(globalVariable);
}
```

## Local Variables

Using the `var` keyword allows you to declare a variable as
local. Without `var` declaration, assigning to a variable may create a
global variable.

```
func main() {
    var a = 123;
    print(a);
}
```

## Type Annotations

Parameters and local declarations may carry optimization-oriented type
annotations.  In an ordinary function, annotated parameters are checked at
function entry at optimization level 1 and above.  An ordinary local annotated
as `int`, `long`, `float`, or `double` is also checked at its initializer and
every reassignment: a proven mismatch is a compile error and a value whose type
is not known until runtime is checked at that assignment.  Optimization level
0 retains the ordinary function's dynamic behavior and treats these
annotations as hints only.  The mandatory exact contracts of `__fast func` are
described separately below.

The checked primitive contract permits `int` where `long` is requested and
`float` where `double` is requested; the value is canonicalized to the wider
runtime type.  Function return annotations remain exact and are checked at
optimization level 2.

```
func copy_words(dst: rpackeduint32, src: rpackeduint32) {
    // The r prefix declares the caller's non-aliasing guarantee.
}
```

Packed annotations include `packedint8`, `packeduint8`, `packedint16`,
`packeduint16`, `packedint32`, `packeduint32`, `packedint64`,
`packeduint64`, `packedfloat` (float32 elements), and `packeddouble`
(float64 elements).  Prefixing one of these names with `r` produces its
restricted form, for example `rpackeduint8` or `rpackedfloat`.  The existing
plain `packed` name accepts any packed element type but does not provide an
element-type or non-aliasing optimization fact.

A restricted annotation is a contract supplied by the programmer.  Backing
storage accessed through a restricted parameter must not overlap storage
accessed through a different parameter while the call is active.  The runtime
is not required to compare their objects or backing ranges, and the optimizer
may use the declaration as a non-aliasing fact without proving it.  Results are
not specified when the caller violates this contract.

## Array

Arrays are ordered collections of values, accessed by index. Arrays
support iteration through the `for` loop construct, allowing you to
iterate through each value directly.

```
func main() {
    var array = [0, 1, 2];
    for (value in array) {
        print(value);
    }
}
```

Arrays can hold values of different types simultaneously, reflecting
the dynamic typing system.

```
func main() {
    var array = [123, "string"];
}
```

The language provides a built-in function `push()` to add elements to
the end of an array.  Also, `pop()` removes the final element.

```
func main() {
    var array = []
    Array.push(array, 0);
    Array.push(array, 1)
    Array.push(array, 2);

    var last = Array.pop(array);
}
```

## Dictionary

Dictionaries store key-value pairs, similar to hash maps or objects in
other languages. They are defined using curly braces with key-value
pairs separated by colons. Dictionaries support iteration where both
the key and value can be accessed simultaneously.

```
func main() {
    var dict = {key1: "value1", key2: "value2"};
    for (key, value in dict) {
        print("key = " + key);
        print("value = " + value);
    }

}
```

Dictionaries may be constructed in a single step way. An assignment
can be an array style which uses `[]`, or an object style which uses
`.`.

```
func main() {
    var dict = {};
    dict["key1"] = "value1";
    dict.key2 = "value2";
}
```

The built-in function `remove()` allows for the deletion of entries by
key.

```
func main() {
    var dict = {key1: "value1", key2: "value2"};
    remove(dict, "key1");
}
```

## For-loop

The for-loop construct provides a concise syntax for iterating through
sequences such as ranges, arrays, and dictionaries.

The range syntax (using the `..` operator) creates an iterator that
generates values from the start to one less than the end value.

```
func main() {
    for (i in 0..10) {
        print(i);
    }
}
```

For-loops can also iterate directly over arrays and other collection
types.

Arrays can be iterated by the for-value syntax.

```
func main() {
    array = [0, 1, 2];
    for (value in array) {
        print(value);
    }
}
```

Dictionaries can be iterated by the for-key-value syntax.

```
func main() {
    var dict = {key1: "value1", key2: "value2"};
    for (key, value in dict) {
        print(key + "=" + value);
    }
}
```

## While Loops

The while-loop provides a traditional iteration mechanism that
continues execution as long as a specified condition remains
true. Unlike for-loops which are designed for iterating over
collections, while-loops are more flexible and can be used for
implementing various algorithms where the number of iterations isn't
known in advance. The example shows a basic counter implementation
incrementing from 0 to 9.

```
func main() {
    var i = 0;
    while (i < 10) {
        print(i);
        i = i + 1;
    }
}
```

## If and Else Blocks

Control flows allow for conditional executions based on evaluated
expressions. The if-else construct follows a familiar syntax where
conditions are evaluated in sequence.

```
func main() {
    var a = readint();
    if (a == 0) {
        print("0");
    } else if (a == 1) {
        print("1");
    } else {
        print("other");
    }
}
```

## Lambda Functions

Functions are first-class objects in the language. Anonymous
functions, also known as `lambda` expressions, allow you to create
functions without names.

```
func main() {
    var f = (a, b) => { return a + b; }
    print(f(1, 2));
}
```

Lambda functions are simply translated to named functions in the
compilation process. Therefore, they can't capture variables declared
in outer functions.

## Increment/Decrement (+=, -=, ++, --)

```
func main() {
    var a = 123;
    a += 321;
    a++;

    var b = 123;
    b -= 321;
    b--;
}
```

`++` and `--` are supported only as standalone statements (`a++;`, `b--;`).
Using them inside expressions is disallowed to avoid complex side-effects.

## OOP in Noct

The object-oriented model in Noct is a lightweight variation of prototype-based OOP.

- Classes are simply dictionary templates
- Inheritance and instantiation are realized by dictionary merging
- There is no prototype chain, and modifying a class does not affect existing instances

This design treats dictionaries as first-class objects, and the author refers to it as Dictionary-based OOP (D-OOP).

```
func main() {
    // The base class definition. (A class is just a dictionary.)
    Animal = class {
        name: "Animal",
        cry: (this) => {
        }
    };

    // The subclass definition. (Just a dictionary merging.)
    Cat = extend Animal {
        name: "Cat",
        voice: "meow",
        cry: (this) => {
            print(this.name + " cries like " + this.voice);
        }
    };

    // Instantiation. (Just a dictionary merging.)
    var myCat = new Cat {
        voice: "neee"
    };

    // This-call uses -> () syntax. (Equal to myCat.cry(myCat))
    myCat->cry();
}
```

## Intrinsics

### Int.from(val)

Converts a value to an int value.

```
var a = Int.from(1.2);
print(a); // => 1
```

### Long.from(val)

Converts a value to a long value.

```
var a = Long.from(1.2);
print(a); // => 1
```

### Float.from(val)

Converts a value to a float value.

```
var a = Float.from("1.2");
print(a); // => 1.2
```

### Double.from(val)

Converts a value to a double value.

```
var a = Double.from("1.2");
print(a); // => 1.2
```

### String.from(val)

Converts a value to a string value.

```
var a = String.from(1.2);
print(a); // => "1.2"
```

### String.charCount(s)

Returns the number of the Unicode characters in a string value.

```
var count1 = String.charCount("ABC");
print(count1); // => 3

var count2 = String.charCount("文ABC");
print(count1); // => 4
```

### String.charAt(s, index)

Returns the character at the index in a string.
The character is returned as a string.

```
var c = String.charAt("ABC", 1);
print(c); // => "B"
```

### String.substring(s, start, len)

Returns a substring.

```
var s1 = String.substring("ABC", 0, 1);
print(s1); // => "A"

var s2 = String.substring("ABC", 1, 2);
print(s2); // => "BC"
```

### String.indexOf(s1, s2)

Searches for a substring and returns the **character index** of the
first match, or -1 if there is none. The index is in characters, the
same unit `String.charAt()` and `String.substring()` use, so the three
combine correctly on multibyte text.

```
var index1 = String.indexOf("ABCDEF", "CD");
print(index1); // => 2

var index2 = String.indexOf("ABCDEF", "DC");
print(index2); // => -1

var s = "あいu";
print(String.indexOf(s, "u"));              // => 2, not the byte offset 6
print(String.substring(s, String.indexOf(s, "い"), 1)); // => "い"
```

### Array.make(size)

Make a new array with an initial size.

```
var a = Array.make(128);
print(Array.size(a)); // => 128
```

### Array.size(arr)

Returns the size of an array.

```
var a = Array.make(128);
print(Array.size(a)); // => 128
```

### Array.push(arr, val)

Adds an element to the tail of an array.

```
var arr = [1, 2, 3];
Array.push(arr, 4);
print(arr); // => [1, 2, 3, 4]
```

### Array.pop(arr)

Removes the tail element from an array.

```
var arr = [1, 2, 3];
var v = Array.pop(arr);
print(arr); // => [1, 2]
print(v); // => 3
```

### Array.resize(arr, size)

Makes a resized array.

```
var arr1 = [1, 2, 3, 4, 5];
var arr2 = Array.resize(arr1, 3);
print(arr2); // => [1, 2, 3]
```

### Array.copy(arr)

Makes a copy of an array.

```
var arr1 = [1, 2, 3, 4, 5];
var arr2 = Array.copy(arr1);
print(arr2); // => [1, 2, 3, 4, 5]
```

### Dict.make()

Makes a new dictionary.

```
var d = Dict.make();
```

### Dict.merge(src1, src2)

Merges two dictionaries into a new dictionary.

```
var d1 = {foo: "FOO"};
var d2 = {bar: "BAR"};
var d3 = Dict.merge(src1, src2);
print(d3); // => {bar: "BAR", foo: "FOO"}
```

### Dict.size(dict)

Returns the size of a dictionary.

```
var d = {foo: "FOO", bar: "BAR"};
print(Dict.size(d)); // => 2
```

### Dict.hasKey(dict, key)

Checks whether a key exists in a dictionary.

``
var d = {foo: "FOO", bar: "BAR"};
if (Dict.hasKey(d, "foo"))
    print("foo exists")
```

### Dict.remove(dict, key)

Removes a key from a dictionary.

```
var d = {foo: "FOO", bar: "BAR"};
Dict.remove(d, "foo");
print(d); // => {bar: "BAR"}
```
```

### Dict.copy(dict)

Makes a shallow copy of a dictionary.

```
var d1 = {foo: "FOO", bar: "BAR"};
var d2 = Dict.copy(d1);
```

### Packed.int8(size)

Makes an int8 packed array.

```
var pi8 = Packed.int8(128);
pi8[0] = 0;
```

### Packed.int16(size)

Makes an int16 packed array.

```
var pi16 = Packed.int16(128);
pi16[0] = 0;
```

### Packed.int32(size)

Makes an int32 packed array.

```
var pi32 = Packed.int32(128);
pi32[0] = 0;
```

### Packed.int64(size)

Makes an int64 packed array.

```
var pi64 = Packed.int64(128);
pi64[0] = 0;
```

### Packed.uint8(size)

Makes an int8 packed array.

```
var pu8 = Packed.uint8(128);
pu8[0] = 0;
```

### Packed.uint16(size)

Makes an uint16 packed array.

```
var pu16 = Packed.uint16(128);
pu16[0] = 0;
```

### Packed.uint32(size)

Makes an uint32 packed array.

```
var pu32 = Packed.uint32(128);
pu32[0] = 0;
```

### Packed.uint64(size)

Makes an uint64 packed array.

```
var pu64 = Packed.uint64(128);
pu64[0] = 0;
```

### Packed.float32(size)

Makes a float32 packed array.

```
var pf32 = Packed.float32(128);
pf32[0] = 0;
```

### Packed.float64(size)

Makes a float64 packed array.

```
var pf64 = Packed.float64(128);
pf64[0] = 0;
```

### Packed.size(packed)

Returns the element count of a packed array.

```
var pi8 = Packed.int8(128);
print(Packed.size(pi8)); // => 128
```

### Packed.type(packed)

Returns the element type of a packed array.

```
var pi8 = Packed.int8(128);
print(Packed.type(pi8)); // => "int8"
```

### Packed.copy(dst, dstIndex, src, srcIndex, count)

Copies `count` elements from `src` to `dst` and returns `count`.
Indices and the count are in elements, the same unit `Packed.size()`
and the `[]` notation use.

Both arrays must hold the same element type. The two regions may
overlap, so this also serves to move a block inside one array, as a
gap buffer does.

```
var src = Packed.uint8(8);
var dst = Packed.uint8(8);
Packed.copy(dst, 2, src, 0, 4);

// Slide a block down by two, over itself.
Packed.copy(src, 0, src, 2, 6);
```

### Packed.fill(dst, index, count, value)

Sets `count` elements of `dst` to `value`, starting at `index`, and
returns `count`. The value is converted to the array's element type.

```
var buf = Packed.uint8(1024);
Packed.fill(buf, 0, 1024, 0);
```

### Math.abs(x)

Gets an absolute value.

```
var a = abs(-1);
print(a); // => 1
```

### Math.random()

Gets a float random value. (0.0 to 1.0)

```
var r = random(); // 0 .. 1.0
```

### Math.sin()

Gets a sin(x) value.

```
var y = sin(x);
```

### Math.cos()

Gets a cos(x) value.

```
var y = cos(x);
```

### Math.tan()

Gets a tan(x) value.

```
var y = tan(x);
```

### Type.of(value)

Gets the type of a value as a string. The result is one of `"int"`,
`"long"`, `"float"`, `"double"`, `"string"`, `"array"`, `"dict"`,
`"packed"` or `"func"`.

```
print(Type.of(1));         // int
print(Type.of("s"));       // string
print(Type.of([1, 2]));    // array
print(Type.of({a: 1}));    // dict
```

### Global.hasVariable(name)

Checks whether a global variable exists.

```
globalVar1 = 1;
if (Global.hasVariable("globalVar1")
    print("globalVar1 exists");
```

### GC.youngGC()

Executes a young GC.

```
GC.youngGC();
```

### GC.oldGC()

Executes an old GC.

```
GC.youngGC();
```

### GC.compactGC()

Executes a compact GC.

```
GC.compactGC();
```

## `__accel func`

`__accel` is an optional function-optimization hint.  It does not change the
meaning of the function: a compiler may ignore the hint and execute the normal,
checked CPU body.  Parameters, return values, and control flow follow the same
rules as an ordinary function.  This is also the behavior at optimization
level 0, in a build without the optimizer, without `--gpu`, or when the GPU
optimizer declines the function.

The accepted forms are:

```noct
__accel func transform(value: int): int {
    return value * 2;
}

static __accel func helper(value) {
    return value + 1;
}
```

`__accel var`, `__accel let`, `__gpu func`, accelerator-specific parameter
suffixes, and source access to the internal `__Accel` package are not language
features.  There is no public `Accel.*` package: an accelerator function is
called by its ordinary function name.

The initial GPU optimization subset is deliberately narrow.  Its primary
buffer parameters are exact `rpackedint32`, `rpackeduint32`, and
`rpackedfloat` annotations.  It accepts typed `int` and `float` scalar
parameters and suitable top-level one-dimensional ranged loops whose
iterations are independent.  Straight-line arithmetic, comparisons, and
Packed loads and stores within such loops may be moved to the selected GPU
backend.  Multiple adjacent eligible loops may share one synchronous GPU
session.  CPU statements divide eligible loops into separate regions: the
first session finishes before the CPU statements execute, and a later session
begins for the next eligible region.  “Multiple GPU regions” therefore means
multiple disjoint synchronous offload regions in one function; it does not
mean that the function uses multiple physical GPUs.

Code outside that subset remains valid CPU code; unsupported control flow,
calls, types, or dependence patterns do not make the source invalid.  A loop
classified as a DOSUM reduction may be optimized when its accumulator is an
`int` (signed 32-bit) or `u32` (unsigned 32-bit), starts at additive zero, and
is updated by addition.  Floating point reductions and minimum, maximum, and
product reductions are currently declined and continue through the CPU body.

Local buffers use ordinary declarations and constructors such as
`let temporary: packedfloat = Packed.float32(count)`.  Ordinarily, a typed
local remains a CPU-owned Packed object and the optimizer adds whole-buffer
transfers around the GPU session.  It may omit that host Packed object only
for a conservative device-only local optimization.  A declaration using the
matching exact `Packed.*` constructor must immediately precede a single GPU
region; the local must not be used by CPU code, returned, escaped, reassigned,
or retained; and the first kernel must completely define every element before
any read.  Its exact positive extent must be a literal or an immutable `int`
parameter.  If any proof is missing, the optimizer keeps the CPU-backed
representation or declines the region.  Direct device-only return,
dirty subrange transfer, asynchronous execution, and persistence across GPU
regions or function calls are not supported.

GPU optimization is available only while compiling source for an explicit
`--gpu` run.  `.nbc` and `.nap` preserve the ordinary CPU body and contain no
GPU program.  Once an eligible loop has been replaced and GPU execution has
started, a GPU error terminates that invocation; the removed CPU loop is not
replayed.  Successful GPU execution preserves the ordinary value semantics,
but explicit GPU execution can fail from device-resource exhaustion at a
different point than the CPU implementation would report host-memory
exhaustion.

The accelerator-enabled CLI chooses from the devices reported by
`--gpu-list`.  Canonical selectors have the forms `vulkan:NAME`,
`opengles:NAME`, `d3d12:NAME`, and `metal:NAME`; an unqualified exact name is
accepted only when it is unique.  `--gpu` without a selector chooses a default
suitable device.  Accelerator builds select platform backends automatically:
Direct3D 12 on Windows, Vulkan and OpenGL ES on Linux and FreeBSD, and Metal on
macOS.  These facilities are not used by `.nbc`, `.nap`, `--compile`, or
`--app`.

# `__fast func`

`__fast func` defines a statically constrained CPU function intended for
automatic optimization and future multicore parallelization.  Every parameter,
explicit local, and return value must have an explicit exact type.  Scalar
parameters and locals use `int`, `long`, `float`, or `double`; a return may also
be `void`.

Packed parameters must use an `rpacked*` element type and an exact shape:

```noct
__fast func scale(image: rpackedfloat(3, 224, 224), factor: float): void {
    for (c in 0..3) {
        for (y in 0..224) {
            for (x in 0..224) {
                image[c, y, x] = image[c, y, x] * factor;
            }
        }
    }
}
```

Shape rank is 1 through 8.  Each extent is a positive integer literal or the
name of an `int`/`long` parameter.  The shape is exact: `(10, 5, 2)` requires a
100-element Packed object.  Multi-dimensional indices are zero-based and use C
row-major order (the last axis is contiguous).  Each axis is checked separately
on the safe path.

Calls check primitive tags, Packed element kinds, dynamic extent positivity,
checked shape products, and exact element counts before executing the callee.
These checks apply even in a build that omits the optimizer.  The `rpacked`
non-aliasing property remains the caller's contract and is not checked at
runtime.

Inside a fast function, globals, closures, methods, and ordinary functions are
not available.  Direct calls to other fast functions and the compiler-owned
`min`, `max`, `abs`, `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`,
`atan2`, `exp`, `ln`, `log2`, `log10`, and numeric conversion intrinsics are
allowed.  Direct or mutual recursion between fast functions is rejected.

`static __inline __fast func` is accepted and remains file-local.  At this stage
it is deliberately not inlined: its calls use the same checked fast-function
path as other fast calls.

At optimization level 0, and in builds that omit the optimizer module, shaped
accesses use checked row-major indexing.  With the optimizer enabled at level
1 or above, the fast-function pass shares the ordinary HIR expression and
interval analyses.  A provably in-range multi-dimensional access may be
flattened to unchecked row-major arithmetic, while a provably out-of-range
access is a compile error.  An access that cannot be proven remains checked.
The optimizer may also supply the exact shape and programmer-declared
restricted facts to later ABCE and SIMD passes.
