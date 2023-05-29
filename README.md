# UTF in a single c++ file

## UTF wikipedia reference
 - [UTF-8](https://en.wikipedia.org/wiki/UTF-8)
 - [UTF-16](https://en.wikipedia.org/wiki/UTF-16)
 - [UTF-32](https://en.wikipedia.org/wiki/UTF-32)

<br/>

# Compiling
Once in your project
```c++
#declare UTF_IMPLEMENTATION
```
before
```c++
#include <utf.hpp>
```
everywhere else just simply include the header.

<br/>

# Usage
[...]

</br>

## Error descriptions

- `UNEXPECTED_CONTINUATION` — The first byte in the stream is a continuation byte

- `TOO_FEW_WORDS` — The null terminator or a different header is encountered before the sequence ends

- `OVERLONG`
> The standard specifies that the correct encoding of a code point uses only the minimum number of bytes required to hold the significant bits of the code point.
<i>&lt;u8 only&gt;</i> [[wikipedia]](https://en.wikipedia.org/wiki/UTF-8)

- `ILLEGAL_CODEPOINT` — these ranges are illegal Unicode codepoints:
  - 0xD800 – 0xDFFF
  - 0x110000 and up
