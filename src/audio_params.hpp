#ifndef AUDIO_PARAMS_HPP
#define AUDIO_PARAMS_HPP

#include <scluk/aliases.hpp>
#include <scluk/array.hpp>
#include <boost/fiber/buffered_channel.hpp>
#include <bit>
#include "portaudio/stream_wrapper.hpp"
#include "dft/sliding_dft.hpp"

namespace audio {
    using scluk::u64, scluk::f32, scluk::heap_array;
    #ifndef LOWER_PERFORMANCE_MODE
    constexpr u64 ft_win = 1024;
    constexpr u64 rate = ft_win*50;//~40-50k
    constexpr u64 ift_overlap = 4;
    constexpr u64 ft_dist = ft_win / ift_overlap;
    #else
    constexpr u64 rate = 10000;
    constexpr u64 ft_win = 512;
    constexpr u64 ift_overlap = 4;
    constexpr u64 ft_dist = ft_win / ift_overlap;
    #endif
    using sliding_dft = dft::sliding_dft<f32, ft_win>;
    using dft_array = sliding_dft::dft_array;
    using frame_chunk = scluk::heap_array<f32, ft_dist>;
    using ift_chunk = scluk::heap_array<f32, ft_win>;
    using gui_simplex_chan = boost::fibers::buffered_channel<dft_array>;

    static_assert(std::has_single_bit(ft_win), "ft_win must be a power of two to allow us to use cooley-tukey ifft");

    struct duplex_chan {
        boost::fibers::buffered_channel<audio::frame_chunk> cb_to_main, main_to_cb;
        duplex_chan(u64 cb_to_main_sz=2, u64 main_to_cb_sz=2) : cb_to_main(cb_to_main_sz), main_to_cb(main_to_cb_sz) {}
    };

    int cb(const void* v_i_buf, void* v_o_buf, [[maybe_unused]]u64 frames, auto, auto, void* userdata) {
        using namespace std::chrono_literals;
        assert(frames == audio::ft_dist && "You were my brother Anakin");
        auto& chans = *reinterpret_cast<audio::duplex_chan*>(userdata);
        const f32* i_buf = reinterpret_cast<const f32*>(v_i_buf);
        f32* o_buf = reinterpret_cast<f32*>(v_o_buf);
        audio::frame_chunk to_main, to_pa;

        std::copy(i_buf, i_buf + to_main.size(), to_main.begin());
        chans.cb_to_main.push_wait_for(std::move(to_main), 100ms);

        chans.main_to_cb.pop_wait_for(to_pa, 100ms);
        std::copy(to_pa.begin(), to_pa.end(), o_buf);
        
        return paContinue;
    }
}

#endif
