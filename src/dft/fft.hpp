#ifndef dft_FFT_HPP

#include <complex>
#include <iostream>
#include <valarray>
#include <bit>
#include <cassert>

#include <scluk/math.hpp>
#include <scluk/aliases.hpp>
 
namespace dft {
    namespace detail {
        template<typename T>
        void in_place_fft(std::valarray<std::complex<T>>& x) {
            using namespace scluk::math_literals;
            using namespace scluk::type_aliases;
            using std::complex, std::valarray;

            const std::size_t N = x.size();
            if (N <= 1) return;
            assert(std::has_single_bit(N) && "the size of the valarray passed to fft must be a power of two");
        
            // divide
            valarray<complex<T>> even = x[std::slice(0, N/2, 2)];
            valarray<complex<T>>  odd = x[std::slice(1, N/2, 2)];
        
            // conquer
            in_place_fft(even);
            in_place_fft(odd);
        
            // combine
            for (size_t k = 0; k < N/2; k++) {
                complex<T> t = std::polar(T(1.), - T(2_pi) * k / N) * odd[k];
                x[k    ] = even[k] + t;
                x[k+N/2] = even[k] - t;
            }
        }

        template<typename T>
        void in_place_ifft(std::valarray<std::complex<T>>& x) {
            x = x.apply(std::conj);// conjugate the complex numbers
            detail::in_place_fft(x);// forward fft
            x = x.apply(std::conj);// conjugate the complex numbers again
            x /= x.size();// scale the numbers
        }
    }

    // Cooley–Tukey FFT (in-place, divide-and-conquer)
    template<typename T>
    std::valarray<std::complex<T>> fft(std::valarray<std::complex<T>> x) {
        detail::in_place_fft(x);
        return x;
    }

    // inverse fft (in-place)
    template<typename T>
    std::valarray<std::complex<T>> ifft(std::valarray<std::complex<T>> x) {
        detail::in_place_ifft(x);
        return x;
    }
}
#endif