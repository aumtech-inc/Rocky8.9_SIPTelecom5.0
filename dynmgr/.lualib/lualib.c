#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

lua_State *lua_ports[48];
pthread_t lua_threads[48];

int
map_create (lua_State * lua)
{
  fprintf(stderr, "%s() %p\n", __func__, lua);
  int w = luaL_checkinteger (lua, 1);
  int h = luaL_checkinteger (lua, 2);

  unsigned char *map = malloc (w * h);

  int n;
  for (n = 0; n != w * h; n++) {
    map[n] = n % 10;
  }

  lua_pushlightuserdata (lua, map);
  return 1;
}

int
map_slice (lua_State * lua)
{
  unsigned char *map = lua_touserdata (lua, 1);

  fprintf(stderr, "%s() %p\n", __func__, lua);

  int map_width = luaL_checkinteger (lua, 2);
  int x = luaL_checkinteger (lua, 3);
  int y = luaL_checkinteger (lua, 4);
  int w = luaL_checkinteger (lua, 5);
  int h = luaL_checkinteger (lua, 6);

  lua_newtable (lua);

  int cx, cy;
  for (cy = 0; cy != h; cy++)
    for (cx = 0; cx != w; cx++) {
      lua_pushnumber (lua, cx + w * cy);
      lua_pushnumber (lua, map[x + cx + (y + cy) * map_width]);
      lua_settable (lua, -3);
    }

  return 1;
}

static const struct luaL_reg luac_lib[] = {
  {"create", map_create},
  {"slice", map_slice},
  {NULL, NULL}
};


int
arc_debug_init ()
{

  int rc = -1;
  int i;

  for (i = 0; i < 48; i++) {
    lua_ports[i] = lua_open ();
    if (lua_ports[i]) {
      luaL_openlibs (lua_ports[i]);
      luaL_openlib (lua_ports[i], "Telecom", luac_lib, 0);
    }
  }

  return rc;
}

void
arc_debug_free ()
{

  int i;

  for(i = 0; i < 48; i++){
	lua_close(lua_ports[i]);
  }

};

int
arc_debug_call(int idx, char *file){

  int slot = idx % 48;
  int rc = -1;

  if(!file){
   return -1;
  }

  if(slot >= 0 && slot < 48){
	 luaL_loadfile(lua_ports[slot], file);
	 rc = lua_pcall(lua_ports[slot], 0, LUA_MULTRET, 0);
  }

  return rc;

}

#ifdef MAIN

int
main ()
{

  int i;
  arc_debug_init();
  int rc;

  for(i = 0; i < 48; i++){
     rc = arc_debug_call(i, "./test.lua");
	 fprintf(stderr, " rc=%d\n", rc);
  }

  arc_debug_free();

  return 0;
}

#endif
