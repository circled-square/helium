#include <cmath>
#include <cstring>
#include <cassert>
#include <complex>
#include <filesystem>

#include <signal.h>
#include <boost/fiber/buffered_channel.hpp>

#include <scluk/language_extension.hpp>
#include <scluk/math.hpp>
#include <scluk/array.hpp>
#include <scluk/functional.hpp>
#include <scluk/sliding_queue.hpp>

#include "dft/sliding_dft.hpp"
#include "sdl_gui_thread.hpp"
#include "audio_params.hpp"

int main() {
    using namespace scluk::language_extension;

    std::filesystem::current_path(EXECUTABLE_DIR);

    audio::sliding_dft dft;
    audio::duplex_chan cb_chan;
    portaudio::async_stream stream({ .frames_per_buffer=audio::ft_dist, .rate=audio::rate, .log="log.txt" }, audio::cb, cb_chan);

    sdl_gui_thread gui_thread;

	//interrupt signal handling
    signal(SIGINT, scluk::lambda_to_fnptr<void(int)>([&gui_thread](int) {
        out("caught sigint!");
        gui_thread.data.do_exit = true;
    }));


    scluk::sliding_queue<audio::ift_chunk, audio::ift_overlap> ift_queue;
    //first iteration, just to populate the arrays
    dft.push_frames(cb_chan.cb_to_main.value_pop());
    audio::dft_array phase_adjusted_dft(dft), old_dft(dft);

    //main loop
    while(!gui_thread.data.do_exit) {
        //push the frames received from portaudio
        dft.push_frames(cb_chan.cb_to_main.value_pop());

        const f32 pitch_mul = gui_thread.data.do_apply_effect ? gui_thread.data.pitch_mul : 1.f;

        //phase adjustment to avoid artifacts (this is what makes this a phase vocoder)
        for(u64 i : index(dft)) {
            using std::abs, std::arg;
            using namespace scluk::math_literals;

            //0 means new harmonic, 1 means old, p means phase, A means amplitude
            const f32 p0 = arg(dft[i]), p1 = arg(old_dft[i]), 
                      p1_adj = arg(phase_adjusted_dft[i]), A0 = abs(dft[i]);

            const f32 unwrap_addend = 2_pi_f * f32(i / audio::ift_overlap);//integer division allows me to automatically floor without additional cost

            const f32 raw_p_delta = p0 - p1;
            const f32 mod_p_delta = raw_p_delta + std::signbit(raw_p_delta) * 2_pi_f;

            const f32 adj_p_delta = (unwrap_addend + mod_p_delta) * pitch_mul;
            const f32 p0_adj = p1_adj + adj_p_delta;

            phase_adjusted_dft[i] = std::polar(A0, p0_adj);
        }

        //calculate the latest ift, apply the hann window and enqueue it
        ift_queue << scluk::math::hann_window(phase_adjusted_dft.ifft<audio::ft_win>(pitch_mul));

        audio::frame_chunk frames(0.f);

        for(u64 n : index(ift_queue)) {
            u64 starting_index = (audio::ift_overlap-1 - n) * audio::ft_dist;

            for(u64 i : range(audio::ft_dist))
                frames[i] += ift_queue[n][starting_index + i];
        }

        //send the frames to the callback
        if(gui_thread.data.do_output_audio)
            cb_chan.main_to_cb.push(std::move(frames));
        else cb_chan.main_to_cb.push(audio::frame_chunk(0.f));

        //send the old dft to the gui and repopulate it
        gui_thread.channel.try_push(std::move(old_dft));
        old_dft = dft;
    }
}
