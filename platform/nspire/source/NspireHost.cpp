
// nSDL's SDL.h, like Ndless's other headers, opens an extern "C" block at
// the top and transitively pulls Ndless's <nucleus.h> / <syscall-decls.h>,
// which in turn `#include <lauxlib.h>` — resolved to z8lua because z8lua's
// include dir comes first. Pre-load *both* the C++ stdlib headers and the
// z8lua headers here, outside any extern "C", so their include guards make
// the later (in-extern-"C") includes no-ops.
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "../../../libs/z8lua/lua.h"
#include "../../../libs/z8lua/lualib.h"
#include "../../../libs/z8lua/lauxlib.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

// sdl (nSDL). Safe to include from C++ — nSDL's headers are well-behaved.
#include <SDL/SDL.h>

#include "../../../source/host.h"
#include "../../../source/hostVmShared.h"
#include "../../../source/nibblehelpers.h"
#include "../../../source/logger.h"
#include "../../../source/filehelpers.h"

// All access to the Ndless SDK goes through nspire_glue.c (plain C).
// We don't include <libndls.h> / <dirent.h> here because they transitively
// pull <syscall-decls.h>, whose stub Lua API clashes with z8lua.
extern "C" {
    int nspire_key_up(void);
    int nspire_key_down(void);
    int nspire_key_left(void);
    int nspire_key_right(void);
    int nspire_key_x(void);
    int nspire_key_o(void);
    int nspire_key_pause(void);
    int nspire_key_tab(void);
    int nspire_key_esc(void);

    typedef void (*nspire_dir_cb)(const char *name, void *user);
    int nspire_list_dir(const char *path, nspire_dir_cb cb, void *user);
    int nspire_mkdir(const char *path);
    int nspire_path_is_dir(const char *path);
}

#define SCREEN_SIZE_X 320
#define SCREEN_SIZE_Y 240

#define SCREEN_BPP 16

const int __screenWidth = SCREEN_SIZE_X;
const int __screenHeight = SCREEN_SIZE_Y;

int _windowWidth = 128;
int _windowHeight = 128;

int _screenWidth = 128;
int _screenHeight = 128;

int _maxNoStretchWidth = 128;
int _maxNoStretchHeight = 128;

const int PicoScreenWidth = 128;
const int PicoScreenHeight = 128;
const int pixelBlocksPerLine = PicoScreenWidth / 8;


StretchOption stretch;
uint32_t last_time;
uint32_t now_time;
uint32_t frame_time;
uint32_t targetFrameTimeMs;

uint8_t currKDown;
uint8_t currKHeld;
uint8_t prevKHeld;

bool prevTab;
bool prevEsc;

SDL_Event event;
SDL_Surface *window;
SDL_Surface *texture;
SDL_bool done = SDL_FALSE;
void *pixels;
uint16_t *base;

SDL_Rect SrcR;
SDL_Rect DestR;

int textureAngle;
uint8_t flip;
int drawModeScaleX = 1;
int drawModeScaleY = 1;

uint16_t _mapped16BitColors[144];


void postFlipFunction(){
    SDL_SoftStretch(texture, &SrcR, window, &DestR);
    SDL_Flip(window);
}

void _setSourceRect(int xoffset, int yoffset) {
    SrcR.x = xoffset;
    SrcR.y = yoffset;
    SrcR.w = PicoScreenWidth / drawModeScaleX - (xoffset * 2);
    SrcR.h = PicoScreenHeight / drawModeScaleY - (yoffset * 2);
}

void _changeStretch(StretchOption newStretch){
    int xoffset = 0;
    int yoffset = 0;

    if (newStretch == PixelPerfect) {
        _screenWidth = PicoScreenWidth;
        _screenHeight = PicoScreenHeight;
    }
    else if (newStretch == StretchToFit) {
        _screenWidth = _windowHeight;
        _screenHeight = _windowHeight;
    }
    else if (newStretch == StretchAndOverflow) {
        yoffset = 4 / drawModeScaleY;
        _screenWidth = PicoScreenWidth * 2;
        _screenHeight = _windowHeight;
    }
    else {
        _screenWidth = _windowWidth;
        _screenHeight = _windowHeight;
    }

    DestR.x = _windowWidth / 2 - _screenWidth / 2;
    DestR.y = _windowHeight / 2 - _screenHeight / 2;
    DestR.w = _screenWidth;
    DestR.h = _screenHeight;

    _setSourceRect(xoffset, yoffset);

    textureAngle = 0;
    flip = 0;

    SDL_FillRect(window, NULL, SDL_MapRGB(window->format, 0, 0, 0));
}


Host::Host(int windowWidth, int windowHeight)  {
    // Cart and log directories are hardcoded and independent of where the
    // .tns executable is installed on the calculator.
    _cartDirectory = "/documents/ndless/fake08/p8carts";
    _logFilePrefix = "/documents/ndless/fake08/";

    string cartdatadir = _logFilePrefix + "cdata";
    if (!nspire_path_is_dir("/documents/ndless/fake08")) {
        nspire_mkdir("/documents/ndless/fake08");
    }
    if (!nspire_path_is_dir(_cartDirectory.c_str())) {
        nspire_mkdir(_cartDirectory.c_str());
    }
    if (!nspire_path_is_dir(cartdatadir.c_str())) {
        nspire_mkdir(cartdatadir.c_str());
    }
    (void)windowWidth;
    (void)windowHeight;
 }

void Host::oneTimeSetup(Audio* audio){
    (void)audio;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL could not initialize\n");
        return;
    }

    SDL_ShowCursor(SDL_DISABLE);

    int flags = SDL_SWSURFACE;

    window = SDL_SetVideoMode(SCREEN_SIZE_X, SCREEN_SIZE_Y, SCREEN_BPP, flags);
    if (!window) {
        fprintf(stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError());
        return;
    }

    texture = SDL_CreateRGBSurface(flags, PicoScreenWidth, PicoScreenHeight, SCREEN_BPP, 0, 0, 0, 0);

    last_time = 0;
    now_time = 0;
    frame_time = 0;
    targetFrameTimeMs = 0;

    SDL_PixelFormat *f = window->format;

    for(int i = 0; i < 144; i++){
        _mapped16BitColors[i] = SDL_MapRGB(f, _paletteColors[i].Red, _paletteColors[i].Green, _paletteColors[i].Blue);
    }

    _windowWidth = SCREEN_SIZE_X;
    _windowHeight = SCREEN_SIZE_Y;

    stretch = PixelPerfect;
    loadSettingsIni();

    _changeStretch(stretch);

    prevKHeld = 0;
    prevTab = false;
    prevEsc = false;
}

void Host::oneTimeCleanup(){
    saveSettingsIni();

    SDL_FreeSurface(texture);
    SDL_FreeSurface(window);

    SDL_Quit();
}

void Host::setTargetFps(int targetFps){
    targetFrameTimeMs = 1000 / targetFps;
}

void Host::changeStretch(){
    if (stretchKeyPressed && resizekey == YesResize) {
        StretchOption newStretch = stretch;

        if (stretch == StretchAndOverflow) {
            newStretch = PixelPerfect;
        }
        else if (stretch == PixelPerfect) {
            newStretch = StretchToFit;
        }
        else if (stretch == StretchToFit) {
            newStretch = StretchToFill;
        }
        else{
            newStretch = StretchAndOverflow;
        }

        _changeStretch(newStretch);

        stretch = newStretch;
        scaleX = _screenWidth / (float)PicoScreenWidth;
        scaleY = _screenHeight / (float)PicoScreenHeight;
        mouseOffsetX = DestR.x;
        mouseOffsetY = DestR.y;
    }
}

void Host::forceStretch(StretchOption newStretch) {
    _changeStretch(newStretch);
    stretch = newStretch;
    scaleX = _screenWidth / (float)PicoScreenWidth;
    scaleY = _screenHeight / (float)PicoScreenHeight;
    mouseOffsetX = DestR.x;
    mouseOffsetY = DestR.y;
}

InputState_t Host::scanInput(){
    // Drain the SDL event queue so the system can process SDL_QUIT etc.
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            done = SDL_TRUE;
        }
    }

    currKHeld = 0;
    if (nspire_key_up())    currKHeld |= P8_KEY_UP;
    if (nspire_key_down())  currKHeld |= P8_KEY_DOWN;
    if (nspire_key_left())  currKHeld |= P8_KEY_LEFT;
    if (nspire_key_right()) currKHeld |= P8_KEY_RIGHT;
    if (nspire_key_x())     currKHeld |= P8_KEY_X;
    if (nspire_key_o())     currKHeld |= P8_KEY_O;
    if (nspire_key_pause()) currKHeld |= P8_KEY_PAUSE;

    currKDown = currKHeld & ~prevKHeld;
    prevKHeld = currKHeld;

    bool tab = nspire_key_tab();
    stretchKeyPressed = tab && !prevTab;
    prevTab = tab;

    bool esc = nspire_key_esc();
    if (esc && !prevEsc) {
        done = SDL_TRUE;
    }
    prevEsc = esc;

    return InputState_t {
        currKDown,
        currKHeld
    };
}

bool Host::shouldQuit() {
    return done == SDL_TRUE;
}

void Host::waitForTargetFps(){
    now_time = SDL_GetTicks();
    frame_time = now_time - last_time;
    last_time = now_time;

    if (frame_time < targetFrameTimeMs) {
        uint32_t msToSleep = targetFrameTimeMs - frame_time;

        SDL_Delay(msToSleep);

        last_time += msToSleep;
    }
}

void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode){
    drawModeScaleX = 1;
    drawModeScaleY = 1;
    switch(drawMode){
        case 1:
            drawModeScaleX = 2;
            textureAngle = 0;
            flip = 0;
            break;
        case 2:
            drawModeScaleY = 2;
            textureAngle = 0;
            flip = 0;
            break;
        case 3:
            drawModeScaleX = 2;
            drawModeScaleY = 2;
            textureAngle = 0;
            flip = 0;
            break;
        case 129:
            textureAngle = 0;
            flip = 1;
            break;
        case 130:
            textureAngle = 0;
            flip = 2;
            break;
        case 131:
            textureAngle = 0;
            flip = 3;
            break;
        case 133:
            textureAngle = 90;
            flip = 0;
            break;
        case 134:
            textureAngle = 180;
            flip = 0;
            break;
        case 135:
            textureAngle = 270;
            flip = 0;
            break;
        default:
            textureAngle = 0;
            flip = 0;
            break;
    }
    int yoffset = stretch == StretchAndOverflow ? 4 / drawModeScaleX : 0;

    _setSourceRect(0, yoffset);

    pixels = texture->pixels;

    if (textureAngle == 0 && flip == 1) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + ( y * PicoScreenHeight + (127 - x));
                base[0] = col;
            }
        }
    }
    else if (textureAngle == 0 && flip == 2) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + ((127 - y) * PicoScreenHeight + x);
                base[0] = col;
            }
        }
    }
    else if (textureAngle == 0 && flip == 3) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + ((127 - y) * PicoScreenHeight + (127 - x));
                base[0] = col;
            }
        }
    }
    else if (textureAngle == 90 && flip == 0) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + (x * PicoScreenHeight + (127 - y));
                base[0] = col;
            }
        }
    }
    else if (textureAngle == 180 && flip == 0) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + ((127 - y) * PicoScreenHeight + (127 - x));
                base[0] = col;
            }
        }
    }
    else if (textureAngle == 270 && flip == 0) {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < PicoScreenWidth; x ++){
                uint8_t c = getPixelNibble(x, y, picoFb);
                uint16_t col = _mapped16BitColors[screenPaletteMap[c] & 0x8f];

                base = ((uint16_t *)pixels) + ((127 - x) * PicoScreenHeight + y);
                base[0] = col;
            }
        }
    }
    else {
        for (int y = 0; y < PicoScreenHeight; y ++){
            for (int x = 0; x < pixelBlocksPerLine; x ++){
                int32_t eightPix = ((int32_t*)picoFb)[y * pixelBlocksPerLine + x];

                int h = (eightPix >> 28) & 0x0f;
                int g = (eightPix >> 24) & 0x0f;
                int f = (eightPix >> 20) & 0x0f;
                int e = (eightPix >> 16) & 0x0f;
                int d = (eightPix >> 12) & 0x0f;
                int c = (eightPix >>  8) & 0x0f;
                int b = (eightPix >>  4) & 0x0f;
                int a = (eightPix)       & 0x0f;

                int32_t cola = _mapped16BitColors[screenPaletteMap[a] & 0x8f];
                int32_t colb = _mapped16BitColors[screenPaletteMap[b] & 0x8f];
                int32_t colc = _mapped16BitColors[screenPaletteMap[c] & 0x8f];
                int32_t cold = _mapped16BitColors[screenPaletteMap[d] & 0x8f];
                int32_t cole = _mapped16BitColors[screenPaletteMap[e] & 0x8f];
                int32_t colf = _mapped16BitColors[screenPaletteMap[f] & 0x8f];
                int32_t colg = _mapped16BitColors[screenPaletteMap[g] & 0x8f];
                int32_t colh = _mapped16BitColors[screenPaletteMap[h] & 0x8f];

                base = ((uint16_t *)pixels + (y * PicoScreenHeight + x * 8));
                base[0] = cola;
                base[1] = colb;
                base[2] = colc;
                base[3] = cold;
                base[4] = cole;
                base[5] = colf;
                base[6] = colg;
                base[7] = colh;
            }
        }
    }

    postFlipFunction();
}

bool Host::shouldFillAudioBuff(){
    return false;
}

void* Host::getAudioBufferPointer(){
    return nullptr;
}

size_t Host::getAudioBufferSize(){
    return 0;
}

void Host::playFilledAudioBuffer(){
}

bool Host::shouldRunMainLoop(){
    if (shouldQuit()){
        return false;
    }

    return true;
}

namespace {
    struct CartListState {
        const std::string *cartDir;
        std::vector<std::string> *carts;
    };

    void cartListCallback(const char *name, void *user) {
        auto *st = static_cast<CartListState *>(user);
        std::string fname(name);
        if (hasEnding(fname, ".tns")) {
            fname.resize(fname.size() - 4);
        }
        if (isCartFile(fname)) {
            st->carts->push_back(*st->cartDir + "/" + name);
        }
    }
}

vector<string> Host::listcarts(){
    vector<string> carts;
    CartListState state{ &_cartDirectory, &carts };
    nspire_list_dir(_cartDirectory.c_str(), cartListCallback, &state);
    return carts;
}

const char* Host::logFilePrefix() {
    return "";
}

std::string Host::customBiosLua() {
    return "cartpath = \"/documents/ndless/fake08/p8carts/\"\n"
        "selectbtn = \"del\"\n"
        "pausebtn = \"enter\"\n"
        "exitbtn = \"esc\"\n"
        "sizebtn = \"tab\"";
}

std::string Host::getCartDirectory() {
    return _cartDirectory;
}

std::string Host::getCartDataFile(std::string cartDataKey) {
    return _logFilePrefix + "cdata/" + cartDataKey + ".p8d.txt.tns";
}

namespace {
    struct DirListState {
        const std::string *cartDir;
        std::vector<std::string> *dirs;
    };

    void dirListCallback(const char *name, void *user) {
        if (name[0] == '.') return;
        auto *st = static_cast<DirListState *>(user);
        std::string fullPath = *st->cartDir + "/" + name;
        if (nspire_path_is_dir(fullPath.c_str())) {
            st->dirs->push_back(name);
        }
    }
}

std::vector<std::string> Host::listdirs() {
    std::vector<std::string> dirs;
    DirListState state{ &_cartDirectory, &dirs };
    nspire_list_dir(_cartDirectory.c_str(), dirListCallback, &state);
    return dirs;
}
