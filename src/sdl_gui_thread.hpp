#ifndef SDL_GUI_THREAD_HPP
#define SDL_GUI_THREAD_HPP

#include <boost/fiber/channel_op_status.hpp>
#include <scluk/language_extension.hpp>
#include <thread>
#include <complex>
#include "sdl/form.hpp"
#include "audio_params.hpp"

using namespace scluk::language_extension;

class sdl_gui_thread : std::jthread {
    i32 pitch_change = 12;

    public:
    struct gui_info_t { 
        bool do_output_audio = true, do_apply_effect = false, do_print = false, do_exit = false;
        f32 pitch_mul = 2.f;
        gui_info_t(){}
    } data;

    audio::gui_simplex_chan channel;
    sdl_gui_thread(gui_info_t gui_data = gui_info_t()) 
        : std::jthread(&sdl_gui_thread::run, this), data(gui_data), channel(16){}

    private:
    audio::dft_array hrm_arr;

    void run() {
        using namespace std::chrono_literals;

        sdl::window win("phase vocoder", { 1000, 540 });
        win.set_icon("music_note.png");
        const auto bg = sdl::color(0xa0, 0xe0, 0xa0, 0xff);
        const sdl::font font("/usr/share/fonts/TTF/FiraCode-Light.ttf", 12);
        win.set_text_color(sdl::color::black, bg);

        win.set_draw_cb([this, &font, bg](sdl::window& w) {
            if(channel.try_pop(hrm_arr) == boost::fibers::channel_op_status::success) {
                w.clear(bg);

                //find amplitude and freq of main harmonic
                const u64 N = hrm_arr.size()/2;

                u64 max_index = 0;
                f32 max_norm = 0.f;
                for(u64 i : range(N))
                    if(f32 norm = std::norm(hrm_arr[i]); norm > max_norm) {
                        max_norm = norm;
                        max_index = i;
                    }
                f32 max_ampl = std::abs(hrm_arr[max_index]);
                //draw the spectrum
                i32 min_y = 60;
                i32 max_y = w.res.y-10;
                f32 max_h = std::max(f32(max_y - min_y), 0.f);
                sdl::rect r { .x = 0, .y = max_y, .w = std::max(1, (w.res.x-20) / i32(N)), .h = 0 };

                for(u64 i : range(hrm_arr.size()/2)) {
                    r.h = -std::min(i32(max_h), i32(max_h * std::abs(hrm_arr[i]) / max_ampl));
                    r.x = 10 + i32(i) * r.w;
                    if(r.x > w.res.x - 10) break;
                    w.set_draw_color(i%2 ? sdl::color(0x7a, 0x9e, 0xb7, 0xff) : sdl::color(0x7a, 0x76, 0xb7, 0xff));
                    w.draw_rect(r);
                }
            }
            //draw text
            auto yn = [](bool b) -> const char* { return b ? "yes" : "no"; };
            w.draw_text(font, 
                sout("[P] Printing: %\n[A] Audio output: %\n[F] Effect: %; %% semitones",  
                yn(data.do_print), yn(data.do_output_audio), yn(data.do_apply_effect), pitch_change < 0 ? "-" : "+", abs(pitch_change)), 
                { 20, 10 });
        });

        win.set_key_cb([this](sdl::window&, SDL_KeyboardEvent e) {
            switch(e.keysym.sym) {
                case SDLK_ESCAPE:
                    quit();
                    break;
                case SDLK_a:
                    if(e.type == SDL_KEYDOWN) 
                        data.do_output_audio = !data.do_output_audio;
                    break;
                case SDLK_p:
                    if(e.type == SDL_KEYDOWN) 
                        data.do_print = !data.do_print;
                    break;
                case SDLK_SPACE:
                    {
                        static bool spacebar_state = false;
                        bool last_spacebar_state = spacebar_state;
                        spacebar_state = e.type == SDL_KEYDOWN;
                        if(spacebar_state != last_spacebar_state) 
                            data.do_apply_effect = !data.do_apply_effect;
                    }
                    break;
                case SDLK_f:
                    if(e.type == SDL_KEYDOWN) 
                        data.do_apply_effect = !data.do_apply_effect;
                    break;
                case SDLK_RIGHT:
                case SDLK_KP_6:
                    if(e.type == SDL_KEYDOWN) {
                        pitch_change++;
                        data.pitch_mul = std::pow(2.f, f32(pitch_change)/12.f);
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_KP_4:
                    if(e.type == SDL_KEYDOWN) {
                        pitch_change--;
                        data.pitch_mul = std::pow(2.f, f32(pitch_change)/12.f);
                    }
                    break;
            }
        });
        
        win.set_quit_cb([this](sdl::window&) { quit(); });

        while(!data.do_exit) {
            std::this_thread::sleep_for(25ms);
            win.poll_events();
            win.render_frame();
        }
    }

    void quit() {
        data.do_exit = true;
        scluk::out("GUI thread exiting");
    }
};

#endif //SDL_GUI_THREAD_HPP
