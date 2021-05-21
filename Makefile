.PHONY: all lua53 build_dir clean demo

TOP := $(PWD)
BUILD_DIR := $(TOP)/build
BIN_DIR := $(TOP)/bin
INCLUDE_DIR := $(TOP)/include
LUA_EX_LIB := $(TOP)/lualib
EXTERNAL := $(TOP)/3rd
SRC := $(TOP)/src

CXX := g++ -std=c++11
CFLAGS := -g3 -O2 -rdynamic -Wall -I$(INCLUDE_DIR) -I$(SRC)
SHARED := -fPIC --shared
LDFLAGS = -L$(BUILD_DIR) -Wl,-rpath $(BUILD_DIR)
LDLUALIB = -llua -ldl

all: build_dir

build_dir:
	-mkdir $(BUILD_DIR)
	-mkdir $(BIN_DIR)
	-mkdir $(INCLUDE_DIR)
	-mkdir $(LUA_EX_LIB)

all: lua53

lua53:
	cd $(EXTERNAL)/lua && $(MAKE) clean && $(MAKE) MYCFLAGS="-O2 -fPIC -g" linux
	install -p -m 0755 $(EXTERNAL)/lua/src/lua $(BIN_DIR)/lua
	install -p -m 0755 $(EXTERNAL)/lua/src/luac $(BIN_DIR)/luac
	install -p -m 0644 $(EXTERNAL)/lua/src/liblua.a $(BUILD_DIR)
	install -p -m 0644 $(EXTERNAL)/lua/src/*.h $(INCLUDE_DIR)
	install -p -m 0644 $(EXTERNAL)/lua/src/*.hpp $(INCLUDE_DIR)

all: socket

socket: $(LUA_EX_LIB)/socket.so

$(LUA_EX_LIB)/socket.so: $(SRC)/socket/lua-socket.c $(SRC)/socket/lua-socket.h
	gcc $(CFLAGS) $(SHARED) $^ -o $@ $(LDFLAGS)

all: lkcp

lkcp: $(LUA_EX_LIB)/lkcp.so

$(LUA_EX_LIB)/lkcp.so: $(SRC)/lkcp/ikcp.c $(SRC)/lkcp/lkcp.c $(SRC)/lkcp/ikcp.h
	gcc $(CFLAGS) $(SHARED) $^ -o $@ $(LDFLAGS)

all: demo

demo: $(LUA_EX_LIB)/demo01.so $(BIN_DIR)/demo02

$(LUA_EX_LIB)/demo01.so: $(SRC)/test/demo01.c
	gcc $(CFLAGS) $(SHARED) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/demo02: $(SRC)/test/demo02.cc
	$(CXX) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLUALIB)

all:
	-rm -rf $(TOP)/*.a $(TOP)/*.o
	@echo 'make finish!!!!!!!!!!!!!!!!!'

clean:
	-rm -rf *.o *.a $(BIN_DIR) $(BUILD_DIR) $(INCLUDE_DIR) $(LUA_EX_LIB)
