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

		// beginFrame() 之后创建的 Unit，本帧不执行 native movement。
		uint64_t born_frame{};
	};
}