/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2012 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#include "Internal.h"

#include <string>
#include <vector>
#include <map>
#include <set>
using namespace std;

#include "modules/Renderer.h"
#include "modules/Screen.h"
#include "modules/GuiHooks.h"
#include "MemAccess.h"
#include "VersionInfo.h"
#include "Types.h"
#include "Error.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "PluginManager.h"
#include "LuaTools.h"

#include "MiscUtils.h"

using namespace DFHack;

#include "DataDefs.h"
#include "df/init.h"
#include "df/texture_handler.h"
#include "df/tile_page.h"
#include "df/interfacest.h"
#include "df/enabler.h"
#include "df/unit.h"
#include "df/item.h"
#include "df/job.h"
#include "df/building.h"
#include "df/renderer.h"
#include "df/plant.h"

using namespace df::enums;
using df::global::init;
using df::global::gps;
using df::global::texture;
using df::global::gview;
using df::global::enabler;

using Screen::Pen;
using Screen::PenArray;

using std::string;

/*
 * Screen painting API.
 */

// returns ui grid coordinates, even if the game map is scaled differently
df::coord2d Screen::getMousePos()
{
    if (!gps)
        return df::coord2d(-1, -1);
    return df::coord2d(gps->mouse_x_tile, gps->mouse_y_tile);
}

// returns the screen pixel coordinates
df::coord2d Screen::getMousePixels()
{
    if (!gps)
        return df::coord2d(-1, -1);
    return df::coord2d(gps->mouse_x_pixel, gps->mouse_y_pixel);
}

df::coord2d Screen::getWindowSize()
{
    if (!gps) return df::coord2d(80, 25);

    return df::coord2d(gps->dimx, gps->dimy);
}

void Screen::zoom(df::zoom_commands cmd) {
    enabler->zoom_display(cmd);
}

bool Screen::inGraphicsMode()
{
    return init && init->display.flag.is_set(init_display_flags::USE_GRAPHICS);
}

static bool doSetTile_default(const Pen &pen, int x, int y, bool map)
{
    auto dim = Screen::getWindowSize();
    if (x < 0 || x >= dim.x || y < 0 || y >= dim.y)
        return false;

// TODO: understand how this changes for v50
    int index = ((x * gps->dimy) + y);
    if (!map) {
        // don't let anything draw over us
        gps->screen1_forced_tile[index] = 0;
    }
    gps->screen1_opt_tile[index] = uint8_t(pen.tile);
    auto fg = &gps->palette[pen.fg][0];
    auto bg = &gps->palette[pen.bg][0];
    auto argb = &gps->screen1_asciirgb[index * 8];
    argb[0] = uint8_t(pen.ch);
    argb[1] = fg[0];
    argb[2] = fg[1];
    argb[3] = fg[2];
    argb[4] = bg[0];
    argb[5] = bg[1];
    argb[6] = bg[2];
/* old code
//     auto screen = gps->screen + index*4;
//     screen[0] = uint8_t(pen.ch);
//     screen[1] = uint8_t(pen.fg) & 15;
//     screen[2] = uint8_t(pen.bg) & 15;
//     screen[3] = uint8_t(pen.bold) & 1;
//     gps->screentexpos[index] = pen.tile;
//     gps->screentexpos_addcolor[index] = (pen.tile_mode == Screen::Pen::CharColor);
//     gps->screentexpos_grayscale[index] = (pen.tile_mode == Screen::Pen::TileColor);
//     gps->screentexpos_cf[index] = pen.tile_fg;
//     gps->screentexpos_cbr[index] = pen.tile_bg;
*/
    return true;
}

GUI_HOOK_DEFINE(Screen::Hooks::set_tile, doSetTile_default);
static bool doSetTile(const Pen &pen, int x, int y, bool map)
{
    return GUI_HOOK_TOP(Screen::Hooks::set_tile)(pen, x, y, map);
}

bool Screen::paintTile(const Pen &pen, int x, int y, bool map)
{
    if (!gps || !pen.valid()) return false;

    doSetTile(pen, x, y, map);
    return true;
}

static Pen doGetTile_default(int x, int y, bool map)
{
    auto dim = Screen::getWindowSize();
    if (x < 0 || x >= dim.x || y < 0 || y >= dim.y)
        return Pen(0,0,0,-1);

/* TODO: understand how this changes for v50
    int index = x*dim.y + y;
    auto screen = gps->screen + index*4;
    if (screen[3] & 0x80)
        return Pen(0,0,0,-1);

    Pen pen(
        screen[0], screen[1], screen[2], screen[3]?true:false,
        gps->screentexpos[index]
    );

    if (pen.tile)
    {
        if (gps->screentexpos_grayscale[index])
        {
            pen.tile_mode = Screen::Pen::TileColor;
            pen.tile_fg = gps->screentexpos_cf[index];
            pen.tile_bg = gps->screentexpos_cbr[index];
        }
        else if (gps->screentexpos_addcolor[index])
        {
            pen.tile_mode = Screen::Pen::CharColor;
        }
    }

    return pen;
*/ return Pen(0,0,0,-1);
}

GUI_HOOK_DEFINE(Screen::Hooks::get_tile, doGetTile_default);
static Pen doGetTile(int x, int y, bool map)
{
    return GUI_HOOK_TOP(Screen::Hooks::get_tile)(x, y, map);
}

Pen Screen::readTile(int x, int y, bool map)
{
    if (!gps) return Pen(0,0,0,-1);

    return doGetTile(x, y, map);
}

bool Screen::paintString(const Pen &pen, int x, int y, const std::string &text, bool map)
{
    auto dim = getWindowSize();
    if (!gps || y < 0 || y >= dim.y) return false;

    Pen tmp(pen);
    bool ok = false;

    for (size_t i = -std::min(0,x); i < text.size(); i++)
    {
        if (x + i >= size_t(dim.x))
            break;

        tmp.ch = text[i];
        tmp.tile = (pen.tile ? pen.tile + uint8_t(text[i]) : 0);
        paintTile(tmp, x+i, y, map);
        ok = true;
    }

    return ok;
}

bool Screen::fillRect(const Pen &pen, int x1, int y1, int x2, int y2, bool map)
{
    auto dim = getWindowSize();
    if (!gps || !pen.valid()) return false;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= dim.x) x2 = dim.x-1;
    if (y2 >= dim.y) y2 = dim.y-1;
    if (x1 > x2 || y1 > y2) return false;

    for (int x = x1; x <= x2; x++)
    {
        for (int y = y1; y <= y2; y++)
            doSetTile(pen, x, y, map);
    }

    return true;
}

bool Screen::drawBorder(const std::string &title)
{
    if (!gps) return false;

    auto dim = getWindowSize();
    Pen border('\xDB', 8);
    Pen text(0, 0, 7);
    Pen signature(0, 0, 8);

    for (int x = 0; x < dim.x; x++)
    {
        doSetTile(border, x, 0, false);
        doSetTile(border, x, dim.y - 1, false);
    }
    for (int y = 0; y < dim.y; y++)
    {
        doSetTile(border, 0, y, false);
        doSetTile(border, dim.x - 1, y, false);
    }

    paintString(signature, dim.x-8, dim.y-1, "DFHack");

    return paintString(text, (dim.x - title.length()) / 2, 0, title);
}

bool Screen::clear()
{
    if (!gps) return false;

    auto dim = getWindowSize();
    return fillRect(Pen(' ',0,0,false), 0, 0, dim.x-1, dim.y-1);
}

bool Screen::invalidate()
{
    if (!enabler) return false;

    enabler->flag.bits.render = true;
    return true;
}

const Pen Screen::Painter::default_pen(0,COLOR_GREY,0);
const Pen Screen::Painter::default_key_pen(0,COLOR_LIGHTGREEN,0);

void Screen::Painter::do_paint_string(const std::string &str, const Pen &pen, bool map)
{
    if (gcursor.y < clip.first.y || gcursor.y > clip.second.y)
        return;

    int dx = std::max(0, int(clip.first.x - gcursor.x));
    int len = std::min((int)str.size(), int(clip.second.x - gcursor.x + 1));

    if (len > dx)
        paintString(pen, gcursor.x + dx, gcursor.y, str.substr(dx, len-dx), map);
}

bool Screen::findGraphicsTile(const std::string &pagename, int x, int y, int *ptile, int *pgs)
{
    if (!gps || !texture || x < 0 || y < 0) return false;

    for (size_t i = 0; i < texture->page.size(); i++)
    {
        auto page = texture->page[i];
        if (!page->loaded || page->token != pagename) continue;

        if (x >= page->page_dim_x || y >= page->page_dim_y)
            break;
        int idx = y*page->page_dim_x + x;
        if (size_t(idx) >= page->texpos.size())
            break;

        if (ptile) *ptile = page->texpos[idx];
        if (pgs) *pgs = page->texpos_gs[idx];
        return true;
    }

    return false;
}

static std::map<df::viewscreen*, Plugin*> plugin_screens;

bool Screen::show(std::unique_ptr<df::viewscreen> screen, df::viewscreen *before, Plugin *plugin)
{
    CHECK_NULL_POINTER(screen);
    CHECK_INVALID_ARGUMENT(!screen->parent && !screen->child);

    if (!gps || !gview) return false;

    df::viewscreen *parent = &gview->view;
    while (parent && parent->child != before)
        parent = parent->child;

    if (!parent) return false;

    gps->force_full_display_count += 2;

    screen->child = parent->child;
    screen->parent = parent;
    df::viewscreen* s = screen.release();
    parent->child = s;
    if (s->child)
        s->child->parent = s;

    if (dfhack_viewscreen::is_instance(s))
        static_cast<dfhack_viewscreen*>(s)->onShow();

    if (plugin)
        plugin_screens[s] = plugin;

    return true;
}

void Screen::dismiss(df::viewscreen *screen, bool to_first)
{
    CHECK_NULL_POINTER(screen);

    auto it = plugin_screens.find(screen);
    if (it != plugin_screens.end())
        plugin_screens.erase(it);

    if (screen->breakdown_level != interface_breakdown_types::NONE)
        return;

    if (to_first)
        screen->breakdown_level = interface_breakdown_types::TOFIRST;
    else
        screen->breakdown_level = interface_breakdown_types::STOPSCREEN;

    if (dfhack_viewscreen::is_instance(screen))
        static_cast<dfhack_viewscreen*>(screen)->onDismiss();
}

bool Screen::isDismissed(df::viewscreen *screen)
{
    CHECK_NULL_POINTER(screen);

    return screen->breakdown_level != interface_breakdown_types::NONE;
}

bool Screen::hasActiveScreens(Plugin *plugin)
{
    if (plugin_screens.empty())
        return false;
    df::viewscreen *screen = &gview->view;
    while (screen)
    {
        auto it = plugin_screens.find(screen);
        if (it != plugin_screens.end() && it->second == plugin)
            return true;
        screen = screen->child;
    }
    return false;
}

namespace DFHack { namespace Screen {

Hide::Hide(df::viewscreen* screen, int flags) :
    screen{screen}, flags{flags}
{
    extract(screen);
}

Hide::~Hide()
{
    if (screen)
        merge(screen);
}

void Hide::extract(df::viewscreen* a)
{
    df::viewscreen* ap = a->parent;
    df::viewscreen* ac = a->child;

    ap->child = ac;
    if (ac) ac->parent = ap;
    else Core::getInstance().top_viewscreen = ap;
}

void Hide::merge(df::viewscreen* a)
{
    if (flags & RESTORE_AT_TOP) {
        a->parent = NULL;
        a->child = NULL;
        Screen::show(std::unique_ptr<df::viewscreen>(a));
        return;
    }

    df::viewscreen* ap = a->parent;
    df::viewscreen* ac = a->parent->child;

    ap->child = a;
    a->child = ac;
    if (ac) ac->parent = a;
    else Core::getInstance().top_viewscreen = a;
}
} }

string Screen::getKeyDisplay(df::interface_key key)
{
    if (enabler)
        return enabler->GetKeyDisplay(key);

    return "?";
}

int Screen::keyToChar(df::interface_key key)
{
    if (key < interface_key::STRING_A000 ||
        key > interface_key::STRING_A255)
        return -1;

    if (key < interface_key::STRING_A128)
        return key - interface_key::STRING_A000;

    return key - interface_key::STRING_A128 + 128;
}

df::interface_key Screen::charToKey(char code)
{
    int val = (unsigned char)code;
    if (val < 127)
        return df::interface_key(interface_key::STRING_A000 + val);
    else if (val == 127)
        return interface_key::NONE;
    else
        return df::interface_key(interface_key::STRING_A128 + (val-128));
}

/*
 * Pen array
 */

PenArray::PenArray(unsigned int bufwidth, unsigned int bufheight)
    :dimx(bufwidth), dimy(bufheight), static_alloc(false)
{
    buffer = new Pen[bufwidth * bufheight];
    clear();
}

PenArray::PenArray(unsigned int bufwidth, unsigned int bufheight, void *buf)
    :dimx(bufwidth), dimy(bufheight), static_alloc(true)
{
    buffer = (Pen*)((PenArray*)buf + 1);
    clear();
}

PenArray::~PenArray()
{
    if (!static_alloc)
        delete[] buffer;
}

void PenArray::clear()
{
    for (unsigned int x = 0; x < dimx; x++)
    {
        for (unsigned int y = 0; y < dimy; y++)
        {
            set_tile(x, y, Screen::Pen(0, 0, 0, 0, false));
        }
    }
}

Pen PenArray::get_tile(unsigned int x, unsigned int y)
{
    if (x < dimx && y < dimy)
        return buffer[(y * dimx) + x];
    return Pen(0, 0, 0, 0, false);
}

void PenArray::set_tile(unsigned int x, unsigned int y, Screen::Pen pen)
{
    if (x < dimx && y < dimy)
        buffer[(y * dimx) + x] = pen;
}

void PenArray::draw(unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                    unsigned int bufx, unsigned int bufy)
{
    if (!gps)
        return;
    for (unsigned int gridx = x; gridx < x + width; gridx++)
    {
        for (unsigned int gridy = y; gridy < y + height; gridy++)
        {
            if (gridx >= unsigned(gps->dimx) ||
                gridy >= unsigned(gps->dimy) ||
                gridx - x + bufx >= dimx ||
                gridy - y + bufy >= dimy)
                continue;
            Screen::paintTile(buffer[((gridy - y + bufy) * dimx) + (gridx - x + bufx)], gridx, gridy);
        }
    }
}

/*
 * Base DFHack viewscreen.
 */

static std::set<df::viewscreen*> dfhack_screens;

dfhack_viewscreen::dfhack_viewscreen() : text_input_mode(false)
{
    dfhack_screens.insert(this);

    last_size = Screen::getWindowSize();
}

dfhack_viewscreen::~dfhack_viewscreen()
{
    dfhack_screens.erase(this);
}

bool dfhack_viewscreen::is_instance(df::viewscreen *screen)
{
    return dfhack_screens.count(screen) != 0;
}

dfhack_viewscreen *dfhack_viewscreen::try_cast(df::viewscreen *screen)
{
    return is_instance(screen) ? static_cast<dfhack_viewscreen*>(screen) : NULL;
}

void dfhack_viewscreen::check_resize()
{
    auto size = Screen::getWindowSize();

    if (size != last_size)
    {
        last_size = size;
        resize(size.x, size.y);
    }
}

void dfhack_viewscreen::logic()
{
    check_resize();

    // Various stuff works poorly unless always repainting
    Screen::invalidate();
}

void dfhack_viewscreen::render()
{
    check_resize();
}

bool dfhack_viewscreen::key_conflict(df::interface_key key)
{
    if (key == interface_key::OPTIONS)
        return true;

    if (text_input_mode)
    {
        if (key == interface_key::HELP || key == interface_key::MOVIES)
            return true;
    }

    return false;
}

/*
 * Lua-backed viewscreen.
 */

static int DFHACK_LUA_VS_TOKEN = 0;

df::viewscreen *dfhack_lua_viewscreen::get_pointer(lua_State *L, int idx, bool make)
{
    df::viewscreen *screen;

    if (lua_istable(L, idx))
    {
        if (!Lua::IsCoreContext(L))
            luaL_error(L, "only the core context can create lua screens");

        lua_rawgetp(L, idx, &DFHACK_LUA_VS_TOKEN);

        if (!lua_isnil(L, -1))
        {
            if (make)
                luaL_error(L, "this screen is already on display");

            screen = (df::viewscreen*)lua_touserdata(L, -1);
        }
        else
        {
            if (!make)
                luaL_error(L, "this screen is not on display");

            screen = new dfhack_lua_viewscreen(L, idx);
        }

        lua_pop(L, 1);
    }
    else
        screen = Lua::CheckDFObject<df::viewscreen>(L, idx);

    return screen;
}

bool dfhack_lua_viewscreen::safe_call_lua(int (*pf)(lua_State *), int args, int rvs)
{
    CoreSuspendClaimer suspend;
    color_ostream_proxy out(Core::getInstance().getConsole());

    auto L = Lua::Core::State;
    lua_pushcfunction(L, pf);
    if (args > 0) lua_insert(L, -args-1);
    lua_pushlightuserdata(L, this);
    if (args > 0) lua_insert(L, -args-1);

    return Lua::Core::SafeCall(out, args+1, rvs);
}

dfhack_lua_viewscreen *dfhack_lua_viewscreen::get_self(lua_State *L)
{
    auto self = (dfhack_lua_viewscreen*)lua_touserdata(L, 1);
    lua_rawgetp(L, LUA_REGISTRYINDEX, self);
    if (!lua_istable(L, -1)) return NULL;
    return self;
}

int dfhack_lua_viewscreen::do_destroy(lua_State *L)
{
    auto self = get_self(L);
    if (!self) return 0;

    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, self);

    lua_pushnil(L);
    lua_rawsetp(L, -2, &DFHACK_LUA_VS_TOKEN);
    lua_pushnil(L);
    lua_setfield(L, -2, "_native");

    lua_getfield(L, -1, "onDestroy");
    if (lua_isnil(L, -1))
        return 0;

    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
    return 0;
}

void dfhack_lua_viewscreen::update_focus(lua_State *L, int idx)
{
    lua_getfield(L, idx, "text_input_mode");
    text_input_mode = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, idx, "allow_options");
    allow_options = lua_toboolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "focus_path");
    auto str = lua_tostring(L, -1);
    if (!str) str = "";
    focus = str;
    lua_pop(L, 1);

    if (focus.empty())
        focus = "lua";
    else
        focus = "lua/"+focus;
}

int dfhack_lua_viewscreen::do_render(lua_State *L)
{
    auto self = get_self(L);
    if (!self) return 0;

    lua_getfield(L, -1, "onRender");

    if (lua_isnil(L, -1))
    {
        Screen::clear();
        return 0;
    }

    lua_pushvalue(L, -2);
    lua_call(L, 1, 0);
    return 0;
}

int dfhack_lua_viewscreen::do_notify(lua_State *L)
{
    int args = lua_gettop(L);

    auto self = get_self(L);
    if (!self) return 0;

    lua_pushvalue(L, 2);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1))
        return 0;

    // self field args table fn -> table fn table args
    lua_replace(L, 1);
    lua_copy(L, -1, 2);
    lua_insert(L, 1);
    lua_call(L, args-1, 1);

    self->update_focus(L, 1);
    return 1;
}

int dfhack_lua_viewscreen::do_input(lua_State *L)
{
    auto self = get_self(L);
    if (!self) return 0;

    auto keys = (std::set<df::interface_key>*)lua_touserdata(L, 2);

    lua_getfield(L, -1, "onInput");

    if (lua_isnil(L, -1))
    {
        if (keys->count(interface_key::LEAVESCREEN))
            Screen::dismiss(self);

        return 0;
    }

    lua_pushvalue(L, -2);
    Lua::PushInterfaceKeys(L, *keys);

    lua_call(L, 2, 0);
    self->update_focus(L, -1);
    return 0;
}

dfhack_lua_viewscreen::dfhack_lua_viewscreen(lua_State *L, int table_idx)
{
    assert(Lua::IsCoreContext(L));

    Lua::PushDFObject(L, (df::viewscreen*)this);
    lua_setfield(L, table_idx, "_native");
    lua_pushlightuserdata(L, this);
    lua_rawsetp(L, table_idx, &DFHACK_LUA_VS_TOKEN);

    lua_pushvalue(L, table_idx);
    lua_rawsetp(L, LUA_REGISTRYINDEX, this);

    update_focus(L, table_idx);
}

dfhack_lua_viewscreen::~dfhack_lua_viewscreen()
{
    safe_call_lua(do_destroy, 0, 0);
}

void dfhack_lua_viewscreen::render()
{
    if (Screen::isDismissed(this))
    {
        if (parent)
            parent->render();
        return;
    }

    dfhack_viewscreen::render();

    safe_call_lua(do_render, 0, 0);
}

void dfhack_lua_viewscreen::logic()
{
    if (Screen::isDismissed(this)) return;

    dfhack_viewscreen::logic();

    lua_pushstring(Lua::Core::State, "onIdle");
    safe_call_lua(do_notify, 1, 0);
}

bool dfhack_lua_viewscreen::key_conflict(df::interface_key key)
{
    if (key == df::interface_key::OPTIONS)
        return !allow_options;
    return dfhack_viewscreen::key_conflict(key);
}

void dfhack_lua_viewscreen::help()
{
    if (Screen::isDismissed(this)) return;

    lua_pushstring(Lua::Core::State, "onHelp");
    safe_call_lua(do_notify, 1, 0);
}

void dfhack_lua_viewscreen::resize(int w, int h)
{
    if (Screen::isDismissed(this)) return;

    auto L = Lua::Core::State;
    lua_pushstring(L, "onResize");
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    safe_call_lua(do_notify, 3, 0);
}

void dfhack_lua_viewscreen::feed(std::set<df::interface_key> *keys)
{
    if (Screen::isDismissed(this)) return;

    lua_pushlightuserdata(Lua::Core::State, keys);
    safe_call_lua(do_input, 1, 0);
}

void dfhack_lua_viewscreen::onShow()
{
    lua_pushstring(Lua::Core::State, "onShow");
    safe_call_lua(do_notify, 1, 0);
}

void dfhack_lua_viewscreen::onDismiss()
{
    lua_pushstring(Lua::Core::State, "onDismiss");
    safe_call_lua(do_notify, 1, 0);
}

df::unit *dfhack_lua_viewscreen::getSelectedUnit()
{
    Lua::StackUnwinder frame(Lua::Core::State);
    lua_pushstring(Lua::Core::State, "onGetSelectedUnit");
    safe_call_lua(do_notify, 1, 1);
    return Lua::GetDFObject<df::unit>(Lua::Core::State, -1);
}

df::item *dfhack_lua_viewscreen::getSelectedItem()
{
    Lua::StackUnwinder frame(Lua::Core::State);
    lua_pushstring(Lua::Core::State, "onGetSelectedItem");
    safe_call_lua(do_notify, 1, 1);
    return Lua::GetDFObject<df::item>(Lua::Core::State, -1);
}

df::job *dfhack_lua_viewscreen::getSelectedJob()
{
    Lua::StackUnwinder frame(Lua::Core::State);
    lua_pushstring(Lua::Core::State, "onGetSelectedJob");
    safe_call_lua(do_notify, 1, 1);
    return Lua::GetDFObject<df::job>(Lua::Core::State, -1);
}

df::building *dfhack_lua_viewscreen::getSelectedBuilding()
{
    Lua::StackUnwinder frame(Lua::Core::State);
    lua_pushstring(Lua::Core::State, "onGetSelectedBuilding");
    safe_call_lua(do_notify, 1, 1);
    return Lua::GetDFObject<df::building>(Lua::Core::State, -1);
}

df::plant *dfhack_lua_viewscreen::getSelectedPlant()
{
    Lua::StackUnwinder frame(Lua::Core::State);
    lua_pushstring(Lua::Core::State, "onGetSelectedPlant");
    safe_call_lua(do_notify, 1, 1);
    return Lua::GetDFObject<df::plant>(Lua::Core::State, -1);
}

#define STATIC_FIELDS_GROUP
#include "../DataStaticsFields.cpp"

using df::identity_traits;

#define CUR_STRUCT dfhack_viewscreen
static const struct_field_info dfhack_viewscreen_fields[] = {
    { METHOD(OBJ_METHOD, is_lua_screen), 0, 0 },
    { METHOD(OBJ_METHOD, getFocusString), 0, 0 },
    { METHOD(OBJ_METHOD, onShow), 0, 0 },
    { METHOD(OBJ_METHOD, onDismiss), 0, 0 },
    { METHOD(OBJ_METHOD, getSelectedUnit), 0, 0 },
    { METHOD(OBJ_METHOD, getSelectedItem), 0, 0 },
    { METHOD(OBJ_METHOD, getSelectedJob), 0, 0 },
    { METHOD(OBJ_METHOD, getSelectedBuilding), 0, 0 },
    { METHOD(OBJ_METHOD, getSelectedPlant), 0, 0 },
    { FLD_END }
};
#undef CUR_STRUCT
virtual_identity dfhack_viewscreen::_identity(sizeof(dfhack_viewscreen), nullptr, "dfhack_viewscreen", nullptr, &df::viewscreen::_identity, dfhack_viewscreen_fields);

#define CUR_STRUCT dfhack_lua_viewscreen
static const struct_field_info dfhack_lua_viewscreen_fields[] = {
    { FLD_END }
};
#undef CUR_STRUCT
virtual_identity dfhack_lua_viewscreen::_identity(sizeof(dfhack_lua_viewscreen), nullptr, "dfhack_lua_viewscreen", nullptr, &dfhack_viewscreen::_identity, dfhack_lua_viewscreen_fields);
