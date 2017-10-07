#pragma once

#include <algorithm>
#include <climits>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>


namespace lzw {
    
    /// Main STL large unsigned type
    using size_t = std::size_t;
    
    
    /// Implementation details
    namespace details {
        
        /// Non-empty continuous symbol codes range [Lower, Upper]
        template <class CharT, CharT Lower, CharT Upper>
        struct symbol_range {
            static_assert(Lower <= Upper, "symbol_range: invalid bounds");
            
            /// Underlying symbol type
            using value_type = CharT;
            
            /// Full amount of symbols in range
            constexpr static size_t length  = Upper - Lower + 1ULL;
            
            constexpr static value_type lower = Lower; // begin-of-range character
            constexpr static value_type upper = Upper; //   end-of-range character
        };
        
        
        template <class...>
        struct piecewise_range;
        
        /// End of recursion, single piece range
        template <class RH>
        struct piecewise_range<RH> {
            using value_type = typename RH::value_type;
            constexpr static size_t length = RH::length;
            
            inline constexpr static value_type symbol_by_index(size_t idx) {
                return (idx < length) ? (RH::lower + idx) :
                       throw std::out_of_range{"symbol_by_index: out of range"};
            }
            
            inline constexpr static size_t index_of_symbol(value_type c) {
                return (RH::lower <= c && c <= RH::upper) ? (c - RH::lower) :
                       throw std::out_of_range{"index_of_symbol: out of range"};
            }
        };
        
        /// Represents set of symbol ranges
        template <class RH, class... RT>
        struct piecewise_range<RH, RT...> { private:
            using tail_piecewise_range = piecewise_range<RT...>;
            constexpr static size_t head_length = RH::length;
            
        public:
            using value_type = typename RH::value_type;
            constexpr static size_t length = head_length + tail_piecewise_range::length;
            
            static_assert(std::is_same<value_type, typename tail_piecewise_range::value_type>::value,
                "piecewise_range<...> with different underlying types are not supported");
            
            inline constexpr static value_type symbol_by_index(size_t idx) {
                return (idx < head_length) ? (RH::lower + idx) :
                       tail_piecewise_range::symbol_by_index(idx - head_length);
            }
            
            inline constexpr static size_t index_of_symbol(value_type c) {
                return (RH::lower <= c && c <= RH::upper) ? (c - RH::lower) :
                       head_length + tail_piecewise_range::index_of_symbol(c);
            }
        };
        
        
        /// @pre x > 0
        /// @returns log2(x) rounded down
        constexpr size_t log2_floor(size_t x) {
            return x == 1 ? 0 : 1 + log2_floor(x/2); }
        
        /// @returns log2(x) rounded up == bits amount needed to encode x values
        constexpr size_t log2_ceil(size_t x) {
            return x <= 1 ? 1 : log2_floor(x - 1) + 1; }
        
        
        /// @returns std::distance(first, last) for multi-pass iterators
        template <
            class Iter,
            class Category = typename std::iterator_traits<Iter>::iterator_category,
            class = typename std::enable_if<
                std::is_base_of<std::forward_iterator_tag, Category>::value>::type>
        size_t distance_advice(Iter first, Iter last) {
            return std::distance(first, last); }
        
        /// @returns 0 (unknown distance for single-pass iterators). Fallthrough callback.
        size_t distance_advice(...) {
            return 0; }
        
        
        /// Storage for LZW codes
        using codes_vec = std::vector<size_t>;
        
        /// ~= string<CharT>
        template <class CharT>
        using phrase_template = std::vector<CharT>;
        
        /// == map {phrase, code}
        template <class CharT>
        using encode_dict_template = std::map<phrase_template<CharT>, size_t>;
        
        /// == map {code, phrase}
        template <class CharT>
        using decode_dict_template = std::vector<phrase_template<CharT>>;
        
        
        /// LZW compression codec.
        /// @param IODict  - dictionary (piecewise_range) of allowed Input symbols
        /// @param PacDict - dictionary (piecewise_range) of packed representation symbols
        template <class IODict, class PackDict>
        class lzw_codec { private:
            
            using io_value_type     = typename IODict::value_type;
            using pack_value_type   = typename PackDict::value_type;
            
            using phrase_t      = phrase_template       <io_value_type>;
            using encode_dict_t = encode_dict_template  <io_value_type>;
            using decode_dict_t = decode_dict_template  <io_value_type>;
            
            /// Allowed bits amount to encode single packed symbol
            constexpr static size_t bit_capacity = log2_floor(PackDict::length);
            
            
            static_assert(bit_capacity <= CHAR_BIT * sizeof(size_t),
                "lzw_codec: PackDict is too large to be packed by size_t codes");
            
            static_assert(PackDict::length >= log2_ceil(IODict::length),
                "lzw_codec: PackDict is too small even for default IODict encoding");
            
            
            /// @returns cached copy of encoding dictionary
            static encode_dict_t encode_dict() {
                static const encode_dict_t d_ = []{
                    encode_dict_t dict;
                    for(size_t code = 0; code < IODict::length; ++code)
                        dict.emplace(std::piecewise_construct,
                            std::forward_as_tuple(1, IODict::symbol_by_index(code)),
                            std::forward_as_tuple(code));
                    return dict;
                }();
                return d_;
            }
            
            /// @returns cached copy of decoding dictionary
            static decode_dict_t decode_dict() {
                static const decode_dict_t d_ = []{
                    decode_dict_t dict; dict.reserve(IODict::length);
                    for(size_t code = 0; code < IODict::length; ++code)
                        dict.emplace_back(1, IODict::symbol_by_index(code));
                    return dict;
                }();
                return d_;
            }
            
            /// @pre: bit_depth <= log2(size_t)
            /// @pre: bit_depth <= 2^bit_capacity
            template<class OutputIt>
            static OutputIt pack_bits(codes_vec const& src, OutputIt d_first, size_t bit_depth) {
                const size_t bits_needed = bit_depth*src.size();
                const size_t output_symbols = (bits_needed - 1)/bit_capacity + 1;   // ceil rounding
                const size_t dead_bits = output_symbols*bit_capacity - bits_needed; // padding bits
                
                *d_first++ = PackDict::symbol_by_index(bit_depth);
                *d_first++ = PackDict::symbol_by_index(dead_bits);
                
                size_t symbol_done = 0; // filled bits of future symbol index
                size_t symbol_acc = 0;  // bitmask of future packed symbol index
                
                // Example, depth=11:
                // [11111111][11122222][222222..][........][........][........][........]
                // [11111111][11122222][22222233][........][........][........][........]
                // [11111111][11122222][22222233][33333333][........][........][........]
                // [11111111][11122222][22222233][33333333][3.......][........][........]
                
                // cycle over codes
                for(auto code : src) {
                    
                    // cycle over code bits
                    for(size_t code_done = 0; code_done < bit_depth;) {
                        
                        size_t symbol_left  = bit_capacity - symbol_done;
                        size_t code_left    = bit_depth - code_done;
                        
                        size_t bits_to_write = std::min(symbol_left, code_left);
                        
                        size_t mask = (code >> code_done);      // abcd|ef  => abcd
                        mask &= (1ULL << bits_to_write) - 1ULL; // abcd     => 00cd
                        mask <<= symbol_done;                   // 00cd     => 00cd0000
                        
                        symbol_acc = symbol_acc | mask;
                        
                        code_done   += bits_to_write;
                        symbol_done += bits_to_write;
                        
                        // output symbol ready, push it
                        if(symbol_done == bit_capacity) {
                            *d_first++ = PackDict::symbol_by_index(symbol_acc);
                            symbol_done = symbol_acc = 0;
                        }
                    }
                }
                
                // no codes left, but last symbol isn't saved
                if(symbol_done != 0)
                    *d_first++ = PackDict::symbol_by_index(symbol_acc);
                
                return d_first;
            }
            
            template <class InputIt>
            static void unpack_bits(InputIt first, InputIt last, codes_vec& dst) {
                if(first == last) return;
                
                const size_t bit_depth = PackDict::index_of_symbol(*first++);
                if(first == last) throw std::logic_error{"lzw_codec: bad data 1"};
                
                const size_t dead_bits = PackDict::index_of_symbol(*first++);
                if(first == last) throw std::logic_error{"lzw_codec: bad data 2"};
                
                const size_t input_length = distance_advice(first, last);
                const size_t payload_bits = bit_capacity*input_length;
                const size_t out_length   = (payload_bits - dead_bits)/bit_depth;
                
                // output preparing
                dst.clear(); dst.reserve(out_length + 1);
                auto d_first = std::back_inserter(dst);
                
                bool done = false;      // stop condition
                size_t symbol_done = 0; // read bits of symbol index
                
                // first chunk of data
                size_t chunk = PackDict::index_of_symbol(*first++);
                
                // cycle over codes
                do {
                    size_t code_acc = 0;
                    size_t code_done = 0;
                    
                    // cycle over code bits
                    while(code_done < bit_depth) {
                        
                        size_t symbol_left  = bit_capacity - symbol_done;
                        size_t code_left    = bit_depth - code_done;
                        
                        // previously processed code may be the last
                        if(symbol_left == dead_bits && code_done == 0 && first == last) {
                            done = true; break; }
                        
                        size_t bits_to_read = std::min(symbol_left, code_left);
                        
                        size_t data = chunk;
                        data >>= symbol_done;                   // abcde|fg => abcde
                        data &= (1ULL << bits_to_read) - 1ULL;  // abcde    => 00cde
                        data <<= code_done;                     // 00cde    => 00cde000
                        
                        code_acc |= data;
                        
                        code_done   += bits_to_read;
                        symbol_done += bits_to_read;
                        
                        // the whole symbol has been processed, move to next
                        if(symbol_done == bit_capacity) {
                            if(first == last) {
                                done = true; break; }
                            
                            chunk = PackDict::index_of_symbol(*first++);
                            symbol_done = 0;
                        }
                    }
                    
                    // no symbols left, but last code isn't saved
                    if(code_done > 0)
                        *d_first++ = code_acc;
                    
                } while(!done);
            }
            
        public:
            
            /// Compresses and encodes input range [first, last).
            /// @param first - begin input iterator,
            /// @param last  - end input iterator,
            /// @param d_first - begin output iterator,
            /// @returns output iterator, one past the last element copied.
            template <class InputIt, class OutputIt>
            static OutputIt encode(InputIt first, InputIt last, OutputIt d_first) {
                if(first == last) return d_first;
                
                auto dict = encode_dict();
                
                size_t next_code = dict.size();
                size_t max_code = next_code - 1;
                
                codes_vec codes;
                codes.reserve(distance_advice(first, last)*3/2);
                
                phrase_t phrase(1, *first++);
                phrase_t tmp;
                
                /// Single emplace step
                auto emplace_code = [&dict, &phrase, &max_code, &codes]{
                    auto code = dict.at(phrase);
                    max_code = std::max(max_code, code);
                    codes.emplace_back(code); };
                
                while(first != last) {
                    auto curr_symbol = *first++;
                    
                    tmp = phrase;
                    tmp.push_back(curr_symbol);
                    
                    // already exists, no insertion
                    if(!dict.emplace(tmp, next_code).second) {
                        phrase = tmp;
                    
                    // not exists, inserted
                    } else {
                        ++next_code;
                        emplace_code();
                        phrase.assign(1, curr_symbol);
                    }
                }
                
                // last step
                emplace_code();
                
                // values_num == max_code + 1
                auto bit_depth = log2_ceil(max_code + 1);
                
                // resulting dictionary is too big for size_t codes
                if(bit_depth > CHAR_BIT * sizeof(size_t))
                    throw std::logic_error{"lzw_codec: bit_depth > log2(size_t)"};
                
                // resulting dictionary is too big to be packed with PackDict
                if(bit_depth >= PackDict::length)
                    throw std::logic_error{"lzw_codec: bit_depth > PackDict::length"};
                
                return pack_bits(codes, d_first, bit_depth);
            }
            
            /// Decodes and decompress input range [first, last).
            /// @param first - begin input iterator,
            /// @param last  - end input iterator,
            /// @param d_first - begin output iterator,
            /// @returns output iterator, one past the last element copied.
            template <class InputIt, class OutputIt>
            static OutputIt decode(InputIt first, InputIt last, OutputIt d_first) {
                if(first == last) return d_first;
                
                // Binary unpacking
                codes_vec codes;
                unpack_bits(first, last, codes);
                
                auto rdict = decode_dict();
                
                size_t code = codes[0];
                size_t old = code;
                
                phrase_t tmp = rdict[code];
                d_first = std::copy(tmp.begin(), tmp.end(), d_first);
                
                for(size_t i = 1, sz = codes.size(); i < sz; ++i) {
                    code = codes[i];
                    tmp = rdict[old];
                    
                    if(code < rdict.size()) {
                        auto const& entry = rdict[code];
                        tmp.push_back(entry[0]);
                        d_first = std::copy(entry.begin(), entry.end(), d_first);
                        rdict.emplace_back(std::move(tmp));
                    } else {
                        tmp.push_back(tmp[0]);
                        d_first = std::copy(tmp.begin(), tmp.end(), d_first);
                        rdict.emplace_back(std::move(tmp));
                    }
                    
                    old = code;
                }
                
                return d_first;
            }
            
            using Input_dictionary  = IODict;
            using Pack_dictionary   = PackDict;
        };
    }
    
    
    /// Exporting main templates:
    
    using details::    symbol_range;    // Continuous range: ['A','Z']
    using details:: piecewise_range;    // Piecewise range (dictionary): {['A','Z'], ['0','9'], ...}
    using details::       lzw_codec;    // Main coder class template
    
    
    /// Predefined most useful dictionaries
    namespace dictionaries {
        
        /// Dictionary for binary input/output
        using BINARY_256_common = piecewise_range<
            symbol_range<unsigned char, 0, 255>>;
        
        /// Dictionary for ASCII input/output
        using ASCII_128_common = piecewise_range<
            symbol_range<char, 0, 127>>;
        
        /// Dictionary for UTF16 output (packing), printable symbols
        using UTF16_pack = piecewise_range<
            symbol_range<char16_t, 0x0020, 0xD7FF>,
            symbol_range<char16_t, 0xE000, 0xFFFF>>;
        
        /// Dictionary for URI-safe output (packing): [0-9A-Za-z]
        using URI_pack = piecewise_range<
            symbol_range<char, '0', '9'>,
            symbol_range<char, 'A', 'Z'>,
            symbol_range<char, 'a', 'z'>>;
    }
    
    
    /// Predefined most useful codecs
    namespace codecs {
        
        /// Simple std::string archiver
        class string_to_string : public lzw_codec<
            dictionaries::ASCII_128_common,
            dictionaries::ASCII_128_common
        > {};
        
        /// Simple binary archiver
        class binary_to_binary : public lzw_codec<
            dictionaries::BINARY_256_common,
            dictionaries::BINARY_256_common
        > {};
        
        /// String to UTF16 archiver
        class string_to_UTF16 : public lzw_codec<
            dictionaries::ASCII_128_common,
            dictionaries::UTF16_pack
        > {};
        
        /// String to URI-safe archiver
        class string_to_URI : public lzw_codec<
            dictionaries::ASCII_128_common,
            dictionaries::URI_pack
        > {};
    }
    
} // lzw
