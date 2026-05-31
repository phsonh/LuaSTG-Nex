#pragma once
#include "lua.hpp"
#include "Unit/UnitPool.hpp"

namespace luastg::binding {
	struct Unit {
		static void registerClass(lua_State* vm);

		// 给其他绑定模块读取 lstg.Unit userdata 内部 handle。
		// 不暴露给 Lua。
		static bool testHandle(lua_State* vm, int index, luastg::UnitHandle& out) noexcept;
		static bool checkHandle(lua_State* vm, int index, luastg::UnitHandle& out);
	};
}