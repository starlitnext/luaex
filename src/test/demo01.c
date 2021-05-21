#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int add(lua_State* L)
{
  double op1 = luaL_checknumber(L, 1);
  double op2 = luaL_checknumber(L, 2);
  lua_pushnumber(L, op1 + op2);
  return 1;
}

static int sub(lua_State* L)
{
  double op1 = luaL_checknumber(L, 1);
  double op2 = luaL_checknumber(L, 2);
  lua_pushnumber(L, op1 - op2);
  return 1;
}

static const struct luaL_Reg methods[] =
{
  { "add", add },
  { "sub", sub },
  {NULL, NULL}
};

int luaopen_demo01(lua_State* L)
{
  luaL_checkversion(L);
  luaL_newlib(L, methods);
  return 1;
}

