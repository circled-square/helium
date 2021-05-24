#ifndef PORTAUDIO_STREAM_RAII_WRAPPER_HPP
#define PORTAUDIO_STREAM_RAII_WRAPPER_HPP

#include <portaudio.h>
#include <stdexcept>
#include <array>
#include <scluk/aliases.hpp>
#include <scluk/modern_print.hpp>

namespace portaudio {
    using namespace scluk::type_aliases;
    class is_inited {
        static bool v;
        friend class async_stream;
        operator bool(){ return v; }
    };

    class async_stream;
    struct stream_params_t {
        u32 frames_per_buffer = 0, rate = 44100;
        u8 i_chans = 1, o_chans = 1, start = 1;
        const char* log = nullptr;
    };

    class async_stream {
    protected:
        PaStream* m_stream;
        static inline void if_err_throw(PaError e, const std::string& msg) {
            if(e) throw std::runtime_error(msg + Pa_GetErrorText(e));
        }
    public:
        async_stream(stream_params_t params, PaStreamCallback* cb, void* userdata = nullptr) {
            using namespace std::string_literals;

            std::freopen(params.log, "w", stderr);

            //init pulseaudio if necessary
            if(!is_inited()) {
                if_err_throw(Pa_Initialize(), "Pa_Initialize failed:\n");
                is_inited::v = true;
            }
            //find best device; one with name "pulse" or else the one Portaudio thinks is default
            int num_devices = Pa_GetDeviceCount();
            if_err_throw(std::min(num_devices, 0), "Pa_GetDeviceCount returned a negative value:\n");
            int device_index = Pa_GetDefaultInputDevice();
            for(int i = 0; i < num_devices; i++) {
                if("pulse"s == Pa_GetDeviceInfo(i)->name) {
                    device_index = i;
                    break;
                }
            }
            const PaDeviceInfo* const device_ptr = Pa_GetDeviceInfo(device_index);
            //open stream
            PaStreamParameters i_params = { device_index, params.i_chans, paFloat32, device_ptr->defaultLowInputLatency, nullptr };
            PaStreamParameters o_params = { device_index, params.o_chans, paFloat32, device_ptr->defaultLowOutputLatency, nullptr };
            if_err_throw(Pa_OpenStream(&m_stream, &i_params, &o_params, f64(params.rate), params.frames_per_buffer, paNoFlag, cb, userdata), 
                "Pa_OpenStream failed:\n");

            if(params.start) this->start();
        }
        
        async_stream(stream_params_t p, PaStreamCallback* cb, auto& userdata) : async_stream(p, cb, reinterpret_cast<void*>(&userdata)){}


        inline void start() {
            if_err_throw(Pa_StartStream(m_stream), "Portaudio stream starting error:\n");
        }
        ~async_stream() {
            if_err_throw(Pa_CloseStream(m_stream), "Portaudio stream closing error:\n");
            if_err_throw(Pa_Terminate(), "PortAudio termination error:\n");
        }

        PaStream* ptr() { return m_stream; }
    };

    class synchronous_stream : public async_stream {
    public:
        synchronous_stream(stream_params_t params, void* userdata = nullptr) 
            : async_stream(params, nullptr, userdata) {}
        synchronous_stream(stream_params_t params, auto& userdata) 
            : async_stream(params, nullptr, userdata) {}

        inline void write(const f32* buf, u64 frames) {
            if_err_throw(Pa_WriteStream(m_stream, buf, frames), "Error writing to stream\n");
        }
        template<size_t sz>
        inline void write(const std::array<f32, sz>& buf) {
            write(buf.data(), sz);
        }
        inline void write(f32 v) {
            write(&v, 1);
        }

        inline void read(f32* buf, u64 frames) {
            if_err_throw(Pa_ReadStream(m_stream, buf, frames), "Error reading from stream\n");
        }
        template<size_t sz>
        inline std::array<f32, sz> read() {
            std::array<f32, sz> buf;
            read(buf.data(), sz);
            return buf;
        }
        inline f32 read() {
            f32 ret;
            read(&ret, 1);
            return ret;
        }
    };
}

#endif //PORTAUDIO_STREAM_RAII_WRAPPER_HPP
