#ifndef sdl_FORM_HPP
#define sdl_FORM_HPP

#include <SDL2/SDL_blendmode.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>
#include <ostream>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <scluk/language_extension.hpp>
//#include <scluk/silence_warning.hpp>

namespace sdl {
  using namespace scluk::language_extension;

  struct color : public SDL_Color {
    color() : SDL_Color() { }
    color(u8 r, u8 g, u8 b, u8 a) : SDL_Color({r,g,b,a}) { }
    color(const SDL_Color& other) : SDL_Color(other) { }
    color(const color& other) : color() { operator=(other); }
    color& operator=(const color& o) {
      r = o.r;
      g = o.g;
      b = o.b;
      a = o.a;
      return *this;
    }
    //a couple of shitty colors mainly for debug
    static constexpr SDL_Color
    transparent {   0,   0,   0,   0 },
    white       { 255, 255, 255, 255 },
    black       {   0,   0,   0, 255 },
    grey        { 128, 128, 128, 255 },
    dark_grey   {  64,  64,  64, 255 },
    light_grey  { 160, 160, 160, 255 },
    red         { 255,   0,   0, 255 },
    green       {   0, 255,   0, 255 },
    blue        {   0,   0, 255, 255 },
    yellow      { 255, 255,   0, 255 };
  };

  template<typename T = i32> 
  struct point {
    /*
      A pair of homogenous variables (+ utility functions). The point (pun intended) is to have some kind of struct
      that can be used to hold coordinates in both floating point (needed for calculations) and integer types
      (needed for input/output/APIs as numbers of pixels are integers), allowing automatic conversions between the
      two.
    */
    T x, y;

    point<T>(T x, T y) : x(x), y(y) { }
    point<T>() { }
    template <typename target_t> 
    operator point<target_t>() const {
      return { static_cast<target_t>(x), static_cast<target_t>(y) };
    }
    operator SDL_Point() const {//SDL_Point is internally identical to (and thus trivially constructible from) point<int>
      return this->operator point<int>();
    }
    
    template <typename distance_t, typename angle_t> 
    inline point<T> get_point_at(distance_t distance, angle_t degrees) const noexcept {//utility function to avoid duplication of code. Returns the coordinates of the point at a certain angle and at a certain distance
      angle_t& angle = (degrees*=0.0174532925199);//convert from degrees to radians 
      return {
        this->x + distance * cos(angle),
        this->y + distance * sin(angle)
      };
    }
  };

  using rect = SDL_Rect;

  template<typename T>
  std::ostream& operator<<(std::ostream& os, const point<T>& p) {
    return os << p.x << ", " << p.y;
  }

  struct sdl_error : std::runtime_error {
    sdl_error(std::string message) 
    : std::runtime_error(message) { }
    virtual ~sdl_error();
  };

  class font {
    TTF_Font* font_ptr = nullptr;
  public:
    int size() const {
      return TTF_FontHeight(font_ptr);
    }
    [[gnu::always_inline]]
    font(const char* path, int size) : font_ptr(TTF_OpenFont(path, size)){
      if(!font_ptr)
        throw sdl_error(sout("TTF_OpenFont error: %", SDL_GetError()));
    }
    [[gnu::always_inline]] ~font() {
      TTF_CloseFont(font_ptr);
    }
    friend class window;
    friend class button;
  };

  class window;

  class button : rect {
    bool pressed = false;
  public:
    enum : int { null_sz = -1 };

    std::string text;
    std::optional<font> font_info;
    std::function<void(window&)> cb;
    color fg, bg;

    button(rect bounds,const std::string& text, const std::optional<font>& font, const std::function<void(window&)>& cb, 
    color fg = color::black, color bg = color::white)
      : rect(bounds), text(text), font_info(font), cb(cb), fg(fg), bg(bg) {
      //if the size was set to null_sz initialise it as the size of the text + 20
      if(font && (bounds.w < 0 || bounds.h < 0)) {
        TTF_SizeText(font->font_ptr, text.c_str(), &bounds.w, &bounds.h);
        if(w < 0)
          w = bounds.w + 20;
        if(h < 0)
          h = bounds.h + 20;
      }
    }
    bool is_pressed() { return pressed; }
    
    friend class window;
  };

  class window {
    SDL_Window* win_ptr = nullptr;
    SDL_Renderer* renderer_ptr = nullptr;
    const u8* kbd_ptr;
    std::function<void(window&)> draw_cb, quit_cb;
    std::function<void(window&,SDL_KeyboardEvent)> key_cb;
    color text_fg, text_bg;
    static u32 window_count;
  public:
    point<int> res;
    std::vector<button> buttons;

    //non inline function declaration
    window(const char* name, point<int> res, std::optional<point<int>> pos = {}, bool resizeable = true);
    ~window();
    void poll_events();
    void render_frame();
    void draw_text(const font& font, const std::string& text, point<int> pos);
    void draw_button(const button& b);
    void set_icon(const char* path);
    
    //inline functions definition
    inline const u8*  kbd() { return kbd_ptr; }
    inline void draw_line(point<int> a, point<int> b) { SDL_RenderDrawLine(renderer_ptr, a.x, a.y, b.x, b.y); }
    inline void set_draw_color(color c) { SDL_SetRenderDrawColor(renderer_ptr, c.r, c.g, c.b, c.a); }
    inline void clear(color c) { set_draw_color(c); SDL_RenderClear(renderer_ptr); }
    inline void set_text_color(color fg, color bg) { text_fg = fg; text_bg = bg; }
    inline void set_draw_cb(const std::function<void(window&)>& cb) { draw_cb = cb; }
    inline void set_quit_cb(const std::function<void(window&)>& cb) { quit_cb = cb; }
    inline void set_key_cb(const std::function<void(window&, SDL_KeyboardEvent)>& cb) { key_cb = cb; }
    inline void draw_rect(point<int> p, point<int> sz) {
      rect r {p.x, p.y, sz.x, sz.y};
      draw_rect(r);
    }
    inline void draw_rect(rect r) { SDL_RenderFillRect(renderer_ptr, &r); }

    template<typename T = int> [[gnu::always_inline]]
    inline point<T> get_mouse_pos() {
      point<int> ret;
      SDL_GetMouseState(&ret.x, &ret.y);
      return point<T>(ret);
    }
  };
}

#endif //SDL_FORM_HPP
