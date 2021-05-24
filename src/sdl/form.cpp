#include "form.hpp"
#include <GL/glew.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_image.h>

namespace sdl {
  namespace detail {
    inline void init_sdl2() {
      if(SDL_Init(SDL_INIT_VIDEO))
        throw sdl_error(sout("SDL_Init sdl Error: %\n", SDL_GetError()));
      else if(TTF_Init()) 
        throw sdl_error(sout("TTF_Init sdl error: %", TTF_GetError()));
      else if (IMG_Init(IMG_INIT_PNG) == 0) {
	      throw sdl_error(sout("IMG_Init sdl error: %", IMG_GetError()));
      }
    }
  }

  window::window(const char* name, point<int> res, std::optional<point<int>> pos, bool resizeable) : res(res) {
    if(!window_count)
      detail::init_sdl2();
    
    if(!pos) pos = { SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED };
    
    if(!(win_ptr = SDL_CreateWindow(name, pos->x, pos->y, res.x, res.y, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | (resizeable ? SDL_WINDOW_RESIZABLE : 0)))) 
      throw sdl_error(sout("SDL_CreateWindow sdl error: %\n", SDL_GetError()));
    
    if (!(renderer_ptr = SDL_CreateRenderer(win_ptr, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC))) 
      throw sdl_error(sout("SDL_CreateRenderer sdl error: %\n", SDL_GetError()));
    
    kbd_ptr = SDL_GetKeyboardState(nullptr);

    window_count++;
  }

  window::~window() {
    if(renderer_ptr)
      SDL_DestroyRenderer(renderer_ptr);
    if(win_ptr)
      SDL_DestroyWindow(win_ptr);
    if(--window_count) {
      TTF_Quit();
      SDL_Quit();
    }
  }
  void window::poll_events() {
    for(SDL_Event event; SDL_PollEvent(&event);) {//poll for events
      switch(event.type) {
      case SDL_QUIT:
        if(quit_cb)
          quit_cb(*this);
        break;
      case SDL_KEYUP:
      case SDL_KEYDOWN:
        if(key_cb)
          key_cb(*this, event.key);
        break;
      case SDL_MOUSEBUTTONDOWN:
        for(button& b : buttons)
          if(b.x <= event.button.x && event.button.x <= b.x + b.w &&
              b.y <= event.button.y && event.button.y <= b.y + b.h)
                b.pressed = true;
        break;
      case SDL_MOUSEBUTTONUP:
        for(button& b : buttons)
          if(b.pressed) {
            b.pressed = false;
            if(b.x <= event.button.x && event.button.x <= b.x + b.w &&
              b.y <= event.button.y && event.button.y <= b.y + b.h)
                b.cb(*this);
          }
        break;
      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          res = { event.window.data1, event.window.data2 };
          glViewport(0, 0, event.window.data1, event.window.data2);
        }
        break;
      }
    }
  }

  void window::render_frame() {
    if(draw_cb)
      draw_cb(*this);
    
    for(const button& b : buttons)
      draw_button(b);
    SDL_RenderPresent(renderer_ptr);//render what you've drawn
  }

  void window::draw_text(const font& font, const std::string& text, point<int> pos) {
    std::istringstream output(text);
    std::string line;
    for(;std::getline(output, line); pos.y += font.size()) {
      if(!line.length())
        continue;
      SDL_Surface* txt = TTF_RenderText_Shaded(font.font_ptr, line.c_str(), text_fg, text_bg);
      SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_ptr, txt);
      SDL_Rect bounds { pos.x, pos.y, txt->w, txt->h}; //x, y, width, height
      SDL_RenderCopy(renderer_ptr, texture, nullptr, &bounds);
    
      SDL_FreeSurface(txt);
      SDL_DestroyTexture(texture);
    }
  }
  
  void window::draw_button(const button& b) {
    //create texture, set it as target
    SDL_Texture* b_texture = 
      SDL_CreateTexture(renderer_ptr, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, b.w, b.h);
    SDL_SetTextureBlendMode(b_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer_ptr, b_texture);

    //draw background
    SDL_Rect pos_in_texture = { 0, 0, b.w, b.h };
    color final_bg = !b.pressed ? b.bg : color{ u8(b.bg.r / 2 + 64), u8(b.bg.g / 2 + 64), u8(b.bg.b / 2 + 64), b.bg.a };
    set_draw_color(final_bg);
    SDL_RenderFillRect(renderer_ptr, &pos_in_texture);    


    if(b.font_info && b.text.length()) {
      set_text_color(b.fg, final_bg);
      point<int> pos;
      TTF_SizeText(b.font_info->font_ptr, b.text.c_str(), &pos.x, &pos.y);
      pos = {(b.w-pos.x)/2, (b.h-pos.y)/2};
      draw_text(*b.font_info, b.text, pos);
    }

    set_draw_color(b.fg);
    SDL_RenderDrawRect(renderer_ptr, &pos_in_texture);

    SDL_SetRenderTarget(renderer_ptr, nullptr);
    SDL_RenderCopy(renderer_ptr, b_texture, nullptr, &b);
  }
  
  void window::set_icon(const char* path) {
    SDL_Surface* icon_surface = IMG_Load(path);
    if (!icon_surface)
      throw sdl_error(sout("IMG_Load sdl error: %", IMG_GetError()));
    
    SDL_SetWindowIcon(win_ptr, icon_surface);
    SDL_FreeSurface(icon_surface);
  }

  u32 window::window_count = 0;

  sdl_error::~sdl_error() { }
}