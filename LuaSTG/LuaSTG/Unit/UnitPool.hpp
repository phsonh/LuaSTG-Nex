#pragma once
#include "Unit/Unit.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace luastg {
	struct UnitHandle {
		uint32_t id{};
		uint32_t generation{};
	};

	class UnitPool {
	public:
		// unit池最大容量
		static constexpr size_t kDefaultMaxUnits = 32768;

		UnitHandle create();
		bool destroy(UnitHandle handle) noexcept;
		Unit* get(UnitHandle handle) noexcept;
		Unit const* get(UnitHandle handle) const noexcept;

		// 每帧 Lua UnitManager.update_all() 开头调用一次。
		// beginFrame() 后创建的 Unit 会在本帧 updateAll() 中被跳过。
		void beginFrame() noexcept;

		void updateAll() noexcept;
		void clear() noexcept;

		[[nodiscard]] size_t count() const noexcept;

	private:
		struct Slot {
			Unit unit{};
			uint32_t generation{ 1 };
			bool occupied{};
		};

		std::vector<Slot> m_slots;
		std::vector<uint32_t> m_free_list;
		size_t m_alive_count{};
		size_t m_max_units{ kDefaultMaxUnits };

		// 单调帧戳。
		// beginFrame() 推进 m_frame_epoch；
		// create() 把当前 m_frame_epoch 写入 Unit::born_frame；
		// updateAll() 跳过 born_frame == m_update_gate_epoch 的 Unit。
		uint64_t m_frame_epoch{};
		uint64_t m_update_gate_epoch{};
	};

	UnitPool& GetUnitPool() noexcept;
}