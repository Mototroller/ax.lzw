#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// For testing purposes:
#define private public
#include <lzw.hpp>

/// @returns vector with random symbols from Dict range (dictionary)
template <class Dict, class Ret = std::vector<typename Dict::value_type>>
Ret generate_random_vector(std::size_t length) {
    Ret out; out.reserve(length);
    std::generate_n(std::back_inserter(out), length, []{
        return Dict::symbol_by_index(std::rand() % Dict::length); });
    return out;
}

template <class Codec, std::size_t N>
struct codec_test {
    
    static bool run() {
        using namespace std;
        using namespace lzw;
        using namespace lzw::details;
        
        // Bits packing
        if(1) {
            for(size_t i = 0; i < N; ++i) {
                const size_t bit_depth = rand() % 32 + 1;
                const size_t bit_mask = (1ULL << bit_depth) - 1ULL;
                
                codes_vec in(rand() % 1024 + 1, 0);
                for(auto& code : in)
                    code = (rand() % 1024) & bit_mask;
                
                vector<typename Codec::Pack_dictionary::value_type> packed;
                Codec::pack_bits(in, back_inserter(packed), bit_depth);
                
                codes_vec out;
                Codec::unpack_bits(packed.begin(), packed.end(), out);
                
                if(in != out) {
                    for(auto const& code : in ) cout << code << " "; cout << "\n";
                    for(auto const& code : out) cout << code << " "; cout << "\n" << endl;
                    return false;
                }
            }
        }
        
        // Encoding/decoding
        if(1) {
            using src_t  = vector<typename Codec::Input_dictionary::value_type>;
            using pack_t = vector<typename Codec:: Pack_dictionary::value_type>;
            
            for(size_t i = 0; i < N; ++i) {
                const size_t length = 1 + rand() % (1 + i);
                
                src_t  src = generate_random_vector<typename Codec::Input_dictionary>(length);
                pack_t enc;
                src_t  dec;
                
                Codec::encode(src.begin(), src.end(), std::back_inserter(enc));
                Codec::decode(enc.begin(), enc.end(), std::back_inserter(dec));
                
                if(src != dec) {
                    for(auto const& x : src) cout << x << " "; cout << "\n";
                    for(auto const& x : enc) cout << x << " "; cout << "\n";
                    for(auto const& x : dec) cout << x << " "; cout << "\n" << endl;
                    return false;
                }
            }
        }
        
        // Perf
        if(1) {
            cout << "\nPerf: PackDict::length=" << Codec::Pack_dictionary::length << endl;
            
            using src_t  = vector<typename Codec::Input_dictionary::value_type>;
            using pack_t = vector<typename Codec:: Pack_dictionary::value_type>;
            
            constexpr size_t sample_length = 1024;
            
            auto perf_fun = [](src_t src){
                using namespace std::chrono;
                
                const float k = 1000.f/src.size();
                
                pack_t enc;
                src_t  dec;
                
                constexpr size_t M = 16;
                using clock_t = high_resolution_clock;
                clock_t::time_point t1, t2;
                
                
                t1 = clock_t::now();
                for(size_t i = 0; i < M; ++i) {
                    enc.clear();
                    Codec::encode(src.begin(), src.end(), back_inserter(enc)); }
                t2 = clock_t::now();
                
                printf("LZW encode = %f us/Ksymbol\n",
                    float(k*duration_cast<microseconds>(t2-t1).count()/M));
                
                
                t1 = clock_t::now();
                for(size_t i = 0; i < M; ++i) {
                    dec.clear();
                    Codec::decode(enc.begin(), enc.end(), back_inserter(dec)); }
                t2 = clock_t::now();
                
                printf("LZW decode = %f us/Ksymbol\n",
                    float(k*duration_cast<microseconds>(t2-t1).count()/M));
                
                
                printf("ZIP ratio = %f (enc/src) str={", float(enc.size())/src.size());
                for(size_t i = 0, sz = min<size_t>(src.size(), 24); i < sz; ++i)
                    cout << " " << hex << size_t(src[i]);
                cout << std::dec << "... }" << endl;
                
                
                if(src != dec) {
                    for(auto const& x : src) cout << x << " "; cout << "\n";
                    for(auto const& x : enc) cout << x << " "; cout << "\n";
                    for(auto const& x : dec) cout << x << " "; cout << "\n" << endl;
                    return false;
                }
                
                return true;
            };
            
            auto src = src_t(sample_length, Codec::Input_dictionary::symbol_by_index(0));
            cout << "Empty data:" << endl;
            if(!perf_fun(src)) return false;
            
            src = generate_random_vector<typename Codec::Input_dictionary>(sample_length);
            cout << "Random data:" << endl;
            if(!perf_fun(src)) return false;
            
            const size_t chunk_len = 16;
            for(size_t i = 0; i < (sample_length/chunk_len) - 1; ++i)
                for(size_t j = 0; j < chunk_len; ++j)
                    src[(i + 1)*chunk_len + j] = src[i*chunk_len + j];
            cout << "Repeating data (chunk_len=" << chunk_len << "):" << endl;
            if(!perf_fun(src)) return false;
            
        }
        
        return true;
    }
    
};

int main() {
    
    using namespace lzw;
    using namespace lzw::details;
    
    {
        using namespace std;
        
        using dict = dictionaries::ASCII_128_common;
        auto word = generate_random_vector<dict>(16);
        
        cout << "Random ASCII range example (codes): ";
        transform(word.begin(), word.end(), ostream_iterator<string>(cout),
            [](dict::value_type x){ return to_string(size_t(x)) + " "; });
        cout << endl;
    }
    
    {
        using Ucase_range = symbol_range<char, 'A', 'Z'>;
        using Lcase_range = symbol_range<char, 'a', 'z'>;
        using ASCII_range = symbol_range<char, 0, 127>;
        
        
        using ASCII_dict = piecewise_range<ASCII_range>;
        
        static_assert(ASCII_dict::index_of_symbol('A') == 'A', "");
        static_assert(ASCII_dict::index_of_symbol('Z') == 'Z', "");
        
        static_assert(ASCII_dict::symbol_by_index('A') == 'A', "");
        static_assert(ASCII_dict::symbol_by_index('Z') == 'Z', "");
        
        
        using URI_dict = piecewise_range<Ucase_range, Lcase_range>;
        
        static_assert(URI_dict::index_of_symbol('A') == 0, "");
        static_assert(URI_dict::index_of_symbol('Z') == ('Z' - 'A'), "");
        static_assert(URI_dict::index_of_symbol('a') == ('Z' - 'A') + 1, "");
        static_assert(URI_dict::index_of_symbol('z') == ('Z' - 'A') + ('z' - 'a') + 1, "");
        
        static_assert(URI_dict::symbol_by_index(0) == 'A', "");
        static_assert(URI_dict::symbol_by_index(('Z' - 'A')) == 'Z', "");
        static_assert(URI_dict::symbol_by_index(('Z' - 'A') + 1) == 'a', "");
        static_assert(URI_dict::symbol_by_index(('Z' - 'A') + ('z' - 'a') + 1) == 'z', "");
    }
    
    {
        static_assert(log2_floor( 1) == 0, "");
        
        static_assert(log2_floor( 2) == 1, "");
        static_assert(log2_floor( 3) == 1, "");
        static_assert(log2_floor( 4) == 2, "");
        static_assert(log2_floor( 5) == 2, "");
        static_assert(log2_floor( 7) == 2, "");
        static_assert(log2_floor( 8) == 3, "");
        static_assert(log2_floor( 9) == 3, "");
        static_assert(log2_floor(15) == 3, "");
        static_assert(log2_floor(16) == 4, "");
        static_assert(log2_floor(17) == 4, "");
        
        static_assert(log2_ceil( 2) == 1, "");
        static_assert(log2_ceil( 3) == 2, "");
        static_assert(log2_ceil( 4) == 2, "");
        static_assert(log2_ceil( 5) == 3, "");
        static_assert(log2_ceil( 7) == 3, "");
        static_assert(log2_ceil( 8) == 3, "");
        static_assert(log2_ceil( 9) == 4, "");
        static_assert(log2_ceil(15) == 4, "");
        static_assert(log2_ceil(16) == 4, "");
        static_assert(log2_ceil(17) == 5, "");
        static_assert(log2_ceil(31) == 5, "");
        static_assert(log2_ceil(32) == 5, "");
        static_assert(log2_ceil(33) == 6, "");
    }
    
    {
        using codec = codecs::string_to_URI;
        std::string src, enc, dec;
        
        src = "Ololo, test string, TOBEORNOTTOBEORTOBEORNOT!";
        codec::encode(src.begin(), src.end(), std::back_inserter(enc));
        codec::decode(enc.begin(), enc.end(), std::back_inserter(dec));
        
        std::cout << "SRC: " << src << std::endl
                  << "URI: " << enc << std::endl
                  << "DEC: " << dec << std::endl;
    }
    
    {
        using namespace std;
        
        constexpr size_t N = 1000;
        vector<bool> results = {
            codec_test<codecs::binary_to_binary, N>::run(),
            codec_test<codecs::string_to_string, N>::run(),
            codec_test<codecs::string_to_UTF16,  N>::run(),
            codec_test<codecs::string_to_URI,    N>::run()};
        
        for(size_t i = 0; i < results.size(); ++i) {
            if(!results[i]) {
                cout << "Test #" << i << " failed" << endl;
                return EXIT_FAILURE;
            }   
        }
        
        cout << "All codecs are OK" << endl;
    }
    
    return EXIT_SUCCESS;
}
