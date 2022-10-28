#ifndef dft_SLIDING_DFT_HPP
#define dft_SLIDING_DFT_HPP

#include <ratio>
#include <complex>
#include <cmath>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <concepts>
#include <scluk/sliding_queue.hpp>
#include <scluk/language_extension.hpp>
#include <scluk/math.hpp>
#include <scluk/array.hpp>
#include <scluk/metaprogramming.hpp>
#include "fft.hpp"

namespace dft {
    using namespace scluk::language_extension;
    using scluk::heap_array, scluk::sliding_queue;

    template <std::floating_point T, size_t N> 
    struct dft_array : public heap_array<std::complex<T>, N> {
        static constexpr size_t sz = N;
        constexpr T get_frequency_per_frame(u32 i) const    		{ return T(i) / T(N); }
        constexpr T get_frames_per_period(u32 i) const      		{ return T(N) / T(i); }
        constexpr T get_frequency_hz(u32 i, u32 rate) const 		{ return get_frequency_per_frame(i) * T(rate); }
        constexpr T get_seconds_per_period(u32 i, u32 rate) const	{ return get_frames_per_period(i) / T(rate); }

        dft_array() : heap_array<std::complex<T>, N>() {}
        dft_array(dft_array&& o) : heap_array<std::complex<T>, N>(std::move(o)) {}
        dft_array(dft_array& o) : heap_array<std::complex<T>, N>(o) {}


        template<u64 ret_len>
        heap_array<T, ret_len> ifft(f32 pitch_factor) {
            heap_array<T, ret_len> ret;
            const u64 n_of_hrms = u64(f32(N) / pitch_factor); // also equal to the number of frames in a period
            const u64 hrms_to_be_copied = std::min(N, n_of_hrms);

            std::valarray<std::complex<T>> harmonics(std::complex<T>(0.), n_of_hrms);

            std::copy(this->begin(), this->begin() + hrms_to_be_copied, std::begin(harmonics));
            std::valarray<std::complex<T>> c_frames = dft::ifft(std::move(harmonics));

            std::valarray<T> frames(c_frames.size());
            for(u32 i : index(frames)) frames[i] = c_frames[i].real();

            for(u64 written_frames = 0; written_frames < ret_len; written_frames += frames.size()) {
                u64 to_copy = std::min(frames.size(), ret_len - written_frames);
                std::copy(std::begin(frames), std::begin(frames)+to_copy, ret.begin()+written_frames);
            }

            return ret;
        }
        dft_array& operator=(dft_array&& o) { 
            this->arr_ptr = std::move(o.arr_ptr); 
            return *this;
        }
        template<scluk::concepts::iterable iterable_t>
        dft_array& operator=(const iterable_t& o) { 
            assert(o.size() == this->size());
            if(!*this) *this = dft_array();
            std::copy(o.begin(), o.end(), this->begin());
            return *this;
        }
        
        void swap(dft_array& o) { std::swap(this->arr_ptr, o.arr_ptr); }

        dft_array clone() {
            dft_array n;
            std::copy(this->begin(), this->end(), n.begin());
            return n;
        }
    };

    template<std::floating_point T, u32 N, scluk::concepts::ratio damping_ratio = std::ratio<0, 1>>
    class sliding_dft : public dft_array<T, N> {
        static bool static_attributes_are_inited;
        static std::array<std::complex<T>, N> harmonic_phase;

        sliding_queue<std::complex<T>, N> queue;
    public:
        using harmonic_array_t = dft_array<T, N>;
        static constexpr u32 window_size = N;

        sliding_dft() : queue(0) {
            using namespace scluk::math::literals;
            if(!static_attributes_are_inited) {
                for(u32 k : index(*this))
                    harmonic_phase[k] = std::exp(std::complex<T>(0., T(2_pi_l) * T(k) / T(N)));
                static_attributes_are_inited = true;
            }
        }
        
        void reset() {
            for(auto& frame : queue)
                frame = 0;
            for(auto& harmonic : *this)
                harmonic = 0;
        }

        void push_frame(std::complex<T> new_frame) {
            static constexpr T damping_factor = T(damping_ratio::den - damping_ratio::num) / T(damping_ratio::den);
            const std::complex<T> old_frame = queue.push(new_frame);
            const std::complex<T> delta = (new_frame - old_frame);
            //fourier shift theorem:
            //  N is the sliding window size;
            //  f(t) is the input at time t;
            //  F(k,t) is the value at frequency k and time t
            //  F(k, t) = e^(i*2pi*k/N)*F(k, t-1)*(f(t)*f(t-N))
            for(u32 k : index(*this))
                (*this)[k] = ((*this)[k] * damping_factor + delta) * harmonic_phase[k];
        }
        void push_frame(T new_frame) { push_frame(std::complex<T>(new_frame, 0.)); }

        template<scluk::concepts::iterable iterable_t>
        void push_frames(const iterable_t& frames) {
            for(auto frame : frames)
                push_frame(frame);
        }

        template<scluk::concepts::iterable iterable_t>
        void push_frames_fft(const iterable_t& frames) {
            for(const auto& frame : frames) queue.push(frame);

            std::valarray<std::complex<T>> in(queue.size());
            std::copy(std::begin(queue), std::end(queue), std::begin(in));

            std::valarray<std::complex<T>> out = fft(scluk::math::hann_window(std::move(in)));
            std::copy(std::begin(out), std::end(out), this->begin());
        }
    };
}

#include "sliding_dft.tpp"

#endif //dft_SLIDING_DFT_HPP
