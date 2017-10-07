## LZW-based data encoder/decoder [![Build Status](https://travis-ci.org/Mototroller/ax.lzw.svg?branch=master)](https://travis-ci.org/Mototroller/ax.lzw)

Header-only library **lzw.hpp** contains implementation of original [LZW](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Welch) compression/decompression algorithms.

Features:

* `Input` and `Packing` dictionaries are fully customizable by symbol ranges, can be piecewise.
    
    _Predefined_: `BINARY_256_common`, `ASCII_128_common`, `UTF16_pack` (output), `URI_pack` (output). See **lzw.hpp** tail for definitions examples.
    
* Specific codec can be instantiated from template: `lzw_codec<InputDict, PackDict>`.
    
    _Predefined_: `string_to_string`, `binary_to_binary`, `string_to_UTF16`, `string_to_URI`. See **lzw.hpp** tail for definitions examples.
    
* Encoding with fixed bit depth (choses by maximum used code).
    
* Dense bit packing (library uses as many bits as possible).
    
* Header-only and STL-only open source.
    
* STL-compatible codec interface: same as `std::copy(first, last, d_first)`.
    
* Tests with usage examples and simple performance measurements are included.
    
* `RELEASE` typical performance:
    
    `encode = ~200 us/Ksymbol`
    
    `decode = ~30 us/Ksymbol`
    
    `zip_ratio < ~0.3 (dst.length/src.length)`

#### WASM

Library initially was designed as native LZW extension to be used with [WASM](http://webassembly.org/) and [Emscripten Embind](http://kripken.github.io/emscripten-site/docs/porting/connecting_cpp_and_javascript/embind.html#embind).

_To be continued_