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
		static constexpr size_t kDefaultMaxUnits = 32768;

		UnitHandle create();
		bool destroy(UnitHandle handle) noexcept;

		Unit* get(UnitHandle handle) noexcept;
		Unit const* get(UnitHandle handle) const noexcept;

		void beginFrame() noexcept;
		void updateAll() noexcept;
		void clear() noexcept;

		// 由 Lua View 层同步。
		// 注意：这里同步的是 bound 删除矩形，不是可视世界矩形。
		void setWorldBounds(double left, double right, double bottom, double top) noexcept;

		// native bound 删除的 Unit 会进入这个队列。
		// Lua UnitManager 每帧 consume 后，回收 Lua wrapper / Visual。
		std::vector<UnitHandle> consumeNativeKilled();

		[[nodiscard]] size_t count() const noexcept;

	private:
		static constexpr uint32_t kInvalidActivePosition = UINT32_MAX;

		struct Slot {
			Unit unit{};
			uint32_t generation{ 1 };
			bool occupied{};
			uint32_t active_position{ kInvalidActivePosition };
		};

		void addActiveIndex(uint32_t index);
		void removeActiveIndex(uint32_t index) noexcept;

		// native bound 专用删除。
		// 会记录 killed handle；普通 Lua 主动 delete 不记录。
		void destroySlotFromNative(uint32_t index) noexcept;

		std::vector<Slot> m_slots;
		std::vector<uint32_t> m_free_list;

		// 只存活跃 slot index。
		std::vector<uint32_t> m_active_indices;

		// native bound 删除队列。
		std::vector<UnitHandle> m_native_killed;

		size_t m_alive_count{};
		size_t m_max_units{ kDefaultMaxUnits };

		uint64_t m_frame_epoch{};
		uint64_t m_update_gate_epoch{};

		// View 同步过来的 bound 删除范围。
		// 默认值按当前 Aether / THlib 默认 bound：
		// x: -224 ~ 224
		// y: -256 ~ 256
		double m_bound_left{ -224.0 };
		double m_bound_right{ 224.0 };
		double m_bound_bottom{ -256.0 };
		double m_bound_top{ 256.0 };
	};

	UnitPool& GetUnitPool() noexcept;
}