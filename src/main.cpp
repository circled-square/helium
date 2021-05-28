#include <cmath>
#include <cstring>
#include <cassert>
#include <complex>
#include <filesystem>

#include <csignal>
#include <boost/fiber/buffered_channel.hpp>

#include <scluk/language_extension.hpp>
#include <scluk/math.hpp>
#include <scluk/array.hpp>
#include <scluk/functional.hpp>

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

    //first iteration, just to populate the arrays
    dft.push_frames(cb_chan.cb_to_main.value_pop());
    audio::dft_array phase_adjusted_dft(dft.clone()), old_dft(dft.clone());

    //main loop
    while(!gui_thread.data.do_exit) {
        //push the frames received from portaudio
        dft.push_frames(cb_chan.cb_to_main.value_pop());

        const f32 pitch_mul = gui_thread.data.do_apply_effect ? gui_thread.data.pitch_mul : 1.f;

        //phase adjustment to avoid artifacts (this is what makes this a phase vocoder)
        for(u64 i : index(dft)) {
            using std::abs, std::arg;

            //0 means new harmonic, 1 means old, p means phase, A means amplitude
            const f32 p0 = arg(dft[i]), p1 = arg(old_dft[i]), 
                      p1_adj = arg(phase_adjusted_dft[i]), A0 = abs(dft[i]);
            const f32 p_delta = (p0 - p1) * pitch_mul;
            const f32 p0_adj = p1_adj + p_delta;

            phase_adjusted_dft[i] = std::polar(A0, p0_adj);
        }

        //calculate the inverse fourier transform
        audio::frame_chunk frames = phase_adjusted_dft.ifft<audio::ft_dist>(pitch_mul);

        //send the frames to the callback
        cb_chan.main_to_cb.push(std::move(frames));

        //send the old dft to the gui and repopulate it
        gui_thread.channel.try_push(std::move(old_dft));
        old_dft = dft.clone();
    }
}
