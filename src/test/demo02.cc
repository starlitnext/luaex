
#include <iostream>
#include <string>
#include "lua.hpp"
#include <string.h>


#define RECV_BUFFER_LEN 4*1024*1024

static void stackDump(lua_State* L)
{
  int i;
  int top = lua_gettop(L);  // depth of the stack
  for (i = 1; i <= top; i++)
  {
    int t = lua_type(L, i);
    switch (t)
    {
    case LUA_TSTRING:
      printf("'%s'", lua_tostring(L, i));
      break;
    case LUA_TBOOLEAN:
      printf(lua_toboolean(L, i) ? "true": "false");
      break;
    case LUA_TNUMBER:
      printf("%g", lua_tonumber(L, i));
      break;
    default:
      printf("%s", lua_typename(L, t));
      break;
    }
    printf("  ");
  }
  printf("\n");
}

static int add(lua_State* L)
{
  double op1 = luaL_checknumber(L, 1);
  double op2 = luaL_checknumber(L, 2);
  lua_pushnumber(L, op1 + op2);
  return 1;
}

static const struct luaL_Reg lkcp_methods [] = {
  { "add" , add },
	{NULL, NULL},
};

int main()
{
  lua_State *L = luaL_newstate();   // 新建lua state
  luaL_openlibs(L);                 // 打开标准库

  luaL_newmetatable(L, "kcp_meta"); // registry.kcp_meta = {__name='kcp_meta'}
  stackDump(L); // table[kcp_meta]

  lua_newtable(L);  // 新建一个table，假设为A
  stackDump(L);   // table[kcp_meta] table[A]
  luaL_setfuncs(L, lkcp_methods, 0); // 把数组 l 中的所有函数 （参见 luaL_Reg） 注册到栈顶的表中
                                      // 也即注册到表A中
  lua_setfield(L, -2, "__index"); // kcp_meta.__index = A
  stackDump(L); // table[kcp_meta]
  // lua_pushcfunction(L, kcp_gc);
  // lua_setfield(L, -2, "__gc"); // kcp_meta.__gc = kcp_gc

  luaL_newmetatable(L, "recv_buffer");
  stackDump(L); // table[kcp_meta] table[recv_buff]

  char* global_recv_buffer = static_cast<char*>(lua_newuserdata(L, sizeof(char)*RECV_BUFFER_LEN));
  memset(global_recv_buffer, 0, sizeof(char)*RECV_BUFFER_LEN);
  luaL_getmetatable(L, "recv_buffer"); // 将注册表中 tname 对应的元表 （参见 luaL_newmetatable）压栈。 如果没有 tname 对应的元表，则将 nil 压栈并返回假。
  stackDump(L); // table[kcp_meta] table[recv_buff] userdata table[recv_buff]

  lua_setmetatable(L, -2); // userdata.meta_table = table[recv_buff]
  stackDump(L); // table[kcp_meta] table[recv_buff] userdata

  lua_setfield(L, LUA_REGISTRYINDEX, "kcp_lua_recv_buffer"); // registry.kcp_lua_recv_buffer = userdata
  stackDump(L); // table[kcp_meta] table[recv_buff] 

  /*
  lua_pushboolean(L, 1);
  lua_pushnumber(L, 10);
  lua_pushnil(L);
  lua_pushstring(L, "hello");

  stackDump(L); // true 10 nil 'hello'

  lua_pushvalue(L, -4); // lua_pushvalue pushes on the stack a copy of the element at the given index
  stackDump(L); // true 10 nil 'hello' true

  lua_replace(L, 3); // lua_replace pops a value and sets it as the value of the given index, without moving anything
  stackDump(L); // true 10 true 'hello'

  lua_settop(L, 6);
  stackDump(L); // true 10 true 'hello' nil nil

  lua_rotate(L, 3, 1);
  stackDump(L); // true 10 nil true 'hello' nil

  lua_remove(L, -3);
  stackDump(L); // true 10 nil 'hello' nil

  lua_settop(L, -5); 
  stackDump(L); // true
  */

  lua_close(L);
  return 0;
}
