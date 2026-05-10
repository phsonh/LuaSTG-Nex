#pragma once
#include <cstdint>

namespace luastg {
	struct Unit {
		uint32_t id{};
		uint32_t generation{};
		bool alive{};

		double x{};
		double y{};
		double vx{};
		double vy{};
		double ax{};
		double ay{};
		double rot{};

		uint64_t timer{};

		// 内部帧戳。
		// 用于防止“本帧 Lua 逻辑中新创建的 Unit”立刻执行 native movement。
		// 不暴露给 Lua。
		uint64_t born_frame{};
	};
}