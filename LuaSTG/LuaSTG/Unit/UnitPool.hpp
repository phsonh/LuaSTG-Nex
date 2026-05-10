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

		std::vector<Slot> m_slots;
		std::vector<uint32_t> m_free_list;

		// 只存活跃 slot index。
		// updateAll() 只遍历这里，避免 m_slots 高水位扫描。
		std::vector<uint32_t> m_active_indices;

		size_t m_alive_count{};
		size_t m_max_units{ kDefaultMaxUnits };

		uint64_t m_frame_epoch{};
		uint64_t m_update_gate_epoch{};
	};

	UnitPool& GetUnitPool() noexcept;
}