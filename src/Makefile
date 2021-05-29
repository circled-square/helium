#directly used
CXX = g++
PKGS = portaudio-2.0 glew sdl2 SDL2_ttf SDL2_image
LIBS = `pkg-config $(PKGS) --libs` -lboost_fiber -lpthread
OUTDIR = `pwd`/..
OUTFILE = -o$(OUTDIR)/out
GENERAL_FLAGS = -std=c++20 -Wall -Wextra -DEXECUTABLE_DIR=\"$(OUTDIR)\"
INCLUDE_PATHS = -ISCLUK/include/ `pkg-config $(PKGS) --cflags-only-I`

#indirectly used
DEBUG_FLAGS = -ggdb3 -fno-fast-math -DLOWER_PERFORMANCE_MODE
PERFORMANCE_FLAGS = -Ofast -DNDEBUG -faggressive-loop-optimizations -fmodulo-sched -fno-rtti
NON_MAIN_TRANSLATION_UNITS = sdl/form.cpp portaudio/stream_wrapper.cpp

#parameters
MAINFILE = main.cpp
SOURCE = $(MAINFILE) $(NON_MAIN_TRANSLATION_UNITS)
ACCESSORY_FLAGS = $(PERFORMANCE_FLAGS)

all:
	reset
	$(CXX) $(GENERAL_FLAGS) $(ACCESSORY_FLAGS) $(INCLUDE_PATHS) $(OUTFILE) $(SOURCE) $(LIBS)
	rm -f log.txt
g:
	make ACCESSORY_FLAGS="$(DEBUG_FLAGS)"
