#include "LuaBinding/modern/Unit.hpp"
#include "LuaBinding/LuaWrapper.hpp"
#include "Unit/UnitPool.hpp"
#include <cmath>
#include <cstring>

namespace {

	constexpr char const* kUnitMetatable = "lstg.Unit.instance";
	constexpr double kPi = 3.141592653589793238462643383279502884;
	constexpr double kDegToRad = kPi / 180.0;
	constexpr double kRadToDeg = 180.0 / kPi;
	constexpr double kUnitEpsilon = 1e-12;

	double unwrap_degrees_near(double const reference, double const principal) noexcept {
		double delta = std::fmod(principal - reference, 360.0);

		if (delta > 180.0) {
			delta -= 360.0;
		}
		if (delta <= -180.0) {
			delta += 360.0;
		}

		return reference + delta;
	}

	void sync_rot_from_velocity(luastg::Unit& unit) noexcept {
		if ((unit.vx * unit.vx + unit.vy * unit.vy) <= kUnitEpsilon) {
			return;
		}

		auto const principal = std::atan2(unit.vy, unit.vx) * kRadToDeg;
		unit.rot = unwrap_degrees_near(unit.rot, principal);
	}

	void set_velocity(luastg::Unit& unit, double const vx, double const vy) noexcept {
		unit.vx = vx;
		unit.vy = vy;
		sync_rot_from_velocity(unit);
	}

	void set_rot_keep_speed(luastg::Unit& unit, double const rot) noexcept {
		unit.rot = rot;

		auto const speed = std::sqrt(unit.vx * unit.vx + unit.vy * unit.vy);
		if (speed <= kUnitEpsilon) {
			return;
		}

		auto const rad = unit.rot * kDegToRad;
		unit.vx = speed * std::cos(rad);
		unit.vy = speed * std::sin(rad);
	}

	struct UnitUserData {
		luastg::UnitHandle handle{};
	};

	UnitUserData* check_unit_userdata(lua_State* const vm, int const index) {
		return static_cast<UnitUserData*>(luaL_checkudata(vm, index, kUnitMetatable));
	}

	luastg::Unit* check_unit(lua_State* const vm, int const index) {
		auto const ud = check_unit_userdata(vm, index);
		auto* unit = luastg::GetUnitPool().get(ud->handle);

		if (!unit) {
			luaL_error(vm, "invalid or destroyed lstg.Unit");
			return nullptr;
		}

		return unit;
	}

	void push_unit(lua_State* const vm, luastg::UnitHandle const handle) {
		auto* ud = static_cast<UnitUserData*>(lua_newuserdata(vm, sizeof(UnitUserData)));
		ud->handle = handle;

		luaL_getmetatable(vm, kUnitMetatable);
		lua_setmetatable(vm, -2);
	}

	int unit_new(lua_State* const vm) {
		auto handle = luastg::GetUnitPool().create();

		if (handle.id == 0) {
			return luaL_error(vm, "UnitPool is full");
		}

		push_unit(vm, handle);
		return 1;
	}

	int unit_delete(lua_State* const vm) {
		auto const ud = check_unit_userdata(vm, 1);
		lua_pushboolean(vm, luastg::GetUnitPool().destroy(ud->handle));
		return 1;
	}

	int unit_is_valid(lua_State* const vm) {
		auto const ud = check_unit_userdata(vm, 1);
		lua_pushboolean(vm, luastg::GetUnitPool().get(ud->handle) != nullptr);
		return 1;
	}

	int unit_begin_frame(lua_State* const vm) {
		luastg::GetUnitPool().beginFrame();
		return 0;
	}

	int unit_update_all(lua_State* const vm) {
		luastg::GetUnitPool().updateAll();
		return 0;
	}

	int unit_set_world_bounds(lua_State* const vm) {
		auto const left = luaL_checknumber(vm, 1);
		auto const right = luaL_checknumber(vm, 2);
		auto const bottom = luaL_checknumber(vm, 3);
		auto const top = luaL_checknumber(vm, 4);

		luastg::GetUnitPool().setWorldBounds(left, right, bottom, top);

		return 0;
	}

	int unit_consume_native_killed(lua_State* const vm) {
		auto killed = luastg::GetUnitPool().consumeNativeKilled();

		lua_createtable(vm, static_cast<int>(killed.size()), 0);

		int index = 1;

		for (auto const& handle : killed) {
			lua_createtable(vm, 0, 2);

			lua_pushinteger(vm, static_cast<lua_Integer>(handle.id));
			lua_setfield(vm, -2, "id");

			lua_pushinteger(vm, static_cast<lua_Integer>(handle.generation));
			lua_setfield(vm, -2, "generation");

			lua_rawseti(vm, -2, index++);
		}

		return 1;
	}

	int unit_clear(lua_State* const vm) {
		luastg::GetUnitPool().clear();
		return 0;
	}

	int unit_count(lua_State* const vm) {
		lua_pushinteger(vm, static_cast<lua_Integer>(luastg::GetUnitPool().count()));
		return 1;
	}

	int unit_tostring(lua_State* const vm) {
		auto const ud = check_unit_userdata(vm, 1);
		auto* unit = luastg::GetUnitPool().get(ud->handle);

		if (!unit) {
			lua_pushfstring(vm, "lstg.Unit<destroyed:%u:%u>", ud->handle.id, ud->handle.generation);
		}
		else {
			lua_pushfstring(vm, "lstg.Unit<%u:%u>", unit->id, unit->generation);
		}

		return 1;
	}

	int unit_index(lua_State* const vm) {
		auto const ud = check_unit_userdata(vm, 1);
		char const* key = luaL_checkstring(vm, 2);

		// 这些方法即使 Unit 已经销毁，也应该能安全访问。
		if (std::strcmp(key, "delete") == 0 || std::strcmp(key, "destroy") == 0) {
			lua_pushcfunction(vm, unit_delete);
			return 1;
		}

		if (std::strcmp(key, "isValid") == 0) {
			lua_pushcfunction(vm, unit_is_valid);
			return 1;
		}

		auto* unit = luastg::GetUnitPool().get(ud->handle);

		// alive 是安全字段：销毁后返回 false，不抛错。
		if (std::strcmp(key, "alive") == 0) {
			lua_pushboolean(vm, unit != nullptr);
			return 1;
		}

		if (!unit) {
			luaL_error(vm, "invalid or destroyed lstg.Unit");
			return 0;
		}

		if (std::strcmp(key, "id") == 0) {
			lua_pushinteger(vm, static_cast<lua_Integer>(unit->id));
			return 1;
		}

		if (std::strcmp(key, "generation") == 0) {
			lua_pushinteger(vm, static_cast<lua_Integer>(unit->generation));
			return 1;
		}

		if (std::strcmp(key, "timer") == 0) {
			lua_pushinteger(vm, static_cast<lua_Integer>(unit->timer));
			return 1;
		}

		if (std::strcmp(key, "x") == 0) { lua_pushnumber(vm, unit->x); return 1; }
		if (std::strcmp(key, "y") == 0) { lua_pushnumber(vm, unit->y); return 1; }
		if (std::strcmp(key, "vx") == 0) { lua_pushnumber(vm, unit->vx); return 1; }
		if (std::strcmp(key, "vy") == 0) { lua_pushnumber(vm, unit->vy); return 1; }
		if (std::strcmp(key, "ax") == 0) { lua_pushnumber(vm, unit->ax); return 1; }
		if (std::strcmp(key, "ay") == 0) { lua_pushnumber(vm, unit->ay); return 1; }
		if (std::strcmp(key, "rot") == 0) { lua_pushnumber(vm, unit->rot); return 1; }

		if (std::strcmp(key, "bound") == 0) {
			lua_pushboolean(vm, unit->bound);
			return 1;
		}

		lua_pushnil(vm);
		return 1;
	}

	int unit_newindex(lua_State* const vm) {
		auto* unit = check_unit(vm, 1);
		char const* key = luaL_checkstring(vm, 2);

		if (std::strcmp(key, "x") == 0) { unit->x = luaL_checknumber(vm, 3); return 0; }
		if (std::strcmp(key, "y") == 0) { unit->y = luaL_checknumber(vm, 3); return 0; }
		if (std::strcmp(key, "vx") == 0) { set_velocity(*unit, luaL_checknumber(vm, 3), unit->vy); return 0; }
		if (std::strcmp(key, "vy") == 0) { set_velocity(*unit, unit->vx, luaL_checknumber(vm, 3)); return 0; }
		if (std::strcmp(key, "ax") == 0) { unit->ax = luaL_checknumber(vm, 3); return 0; }
		if (std::strcmp(key, "ay") == 0) { unit->ay = luaL_checknumber(vm, 3); return 0; }
		if (std::strcmp(key, "rot") == 0) { set_rot_keep_speed(*unit, luaL_checknumber(vm, 3)); return 0; }

		if (std::strcmp(key, "bound") == 0) {
			unit->bound = lua_toboolean(vm, 3) != 0;
			return 0;
		}

		return luaL_error(vm, "unknown or read-only lstg.Unit field '%s'", key);
	}

	int unit_set_velocity(lua_State* const vm) {
		auto* unit = check_unit(vm, 1);
		set_velocity(*unit, luaL_checknumber(vm, 2), luaL_checknumber(vm, 3));
		return 0;
	}

	void create_unit_metatable(lua_State* const vm) {
		if (luaL_newmetatable(vm, kUnitMetatable)) {
			lua_pushcfunction(vm, unit_index);
			lua_setfield(vm, -2, "__index");

			lua_pushcfunction(vm, unit_newindex);
			lua_setfield(vm, -2, "__newindex");

			lua_pushcfunction(vm, unit_tostring);
			lua_setfield(vm, -2, "__tostring");
		}

		lua_pop(vm, 1);
	}
}

namespace luastg::binding {
	bool Unit::testHandle(lua_State* const vm, int const index, luastg::UnitHandle& out) noexcept {
		if (!lua_isuserdata(vm, index)) {
			return false;
		}

		if (!lua_getmetatable(vm, index)) {
			return false;
		}

		luaL_getmetatable(vm, kUnitMetatable);

		bool const same_metatable = lua_rawequal(vm, -1, -2) != 0;

		lua_pop(vm, 2);

		if (!same_metatable) {
			return false;
		}

		auto* ud = static_cast<UnitUserData*>(lua_touserdata(vm, index));

		if (!ud) {
			return false;
		}

		out = ud->handle;
		return true;
	}

	bool Unit::checkHandle(lua_State* const vm, int const index, luastg::UnitHandle& out) {
		if (!testHandle(vm, index, out)) {
			luaL_error(vm, "lstg.Unit expected");
			return false;
		}

		return true;
	}

	void Unit::registerClass(lua_State* const vm) {
		create_unit_metatable(vm);

		luaL_Reg const unit_api[] = {
			{ "new", &unit_new },
			{ "delete", &unit_delete },
			{ "destroy", &unit_delete },
			{ "isValid", &unit_is_valid },
			{ "beginFrame", &unit_begin_frame },
			{ "updateAll", &unit_update_all },
			{ "setWorldBounds", &unit_set_world_bounds },
			{ "consumeNativeKilled", &unit_consume_native_killed },
			{ "clear", &unit_clear },
			{ "count", &unit_count },
			{ nullptr, nullptr },
		};

		luaL_register(vm, LUASTG_LUA_LIBNAME ".Unit", unit_api); // ... lstg.Unit
		luaL_register(vm, LUASTG_LUA_LIBNAME, nullptr);          // ... lstg.Unit lstg
		lua_pushvalue(vm, -2);                                   // ... lstg.Unit lstg lstg.Unit
		lua_setfield(vm, -2, "Unit");                            // ... lstg.Unit lstg
		lua_pop(vm, 2);
	}
}