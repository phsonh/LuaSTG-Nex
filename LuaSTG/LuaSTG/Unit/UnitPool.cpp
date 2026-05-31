#include "Unit/UnitPool.hpp"
#include <cmath>
#include <utility>

namespace luastg {
	namespace {
		constexpr double kPi = 3.141592653589793238462643383279502884;
		constexpr double kRadToDeg = 180.0 / kPi;
		constexpr double kUnitEpsilon = 1e-12;

		double unwrapDegreesNear(double const reference, double const principal) noexcept {
			double delta = std::fmod(principal - reference, 360.0);

			if (delta > 180.0) {
				delta -= 360.0;
			}
			if (delta <= -180.0) {
				delta += 360.0;
			}

			return reference + delta;
		}

		void syncUnitRotFromVelocity(Unit& unit) noexcept {
			if ((unit.vx * unit.vx + unit.vy * unit.vy) <= kUnitEpsilon) {
				return;
			}

			auto const principal = std::atan2(unit.vy, unit.vx) * kRadToDeg;
			unit.rot = unwrapDegreesNear(unit.rot, principal);
		}

		void advanceGeneration(uint32_t& generation) noexcept {
			++generation;

			if (generation == 0) {
				++generation;
			}
		}
	}

	void UnitPool::addActiveIndex(uint32_t const index) {
		auto& slot = m_slots[index];

		if (slot.active_position != kInvalidActivePosition) {
			return;
		}

		slot.active_position = static_cast<uint32_t>(m_active_indices.size());
		m_active_indices.push_back(index);
	}

	void UnitPool::removeActiveIndex(uint32_t const index) noexcept {
		if (index >= m_slots.size()) {
			return;
		}

		auto& slot = m_slots[index];
		auto const position = slot.active_position;

		if (position == kInvalidActivePosition) {
			return;
		}

		auto const last_index = m_active_indices.back();

		m_active_indices[position] = last_index;
		m_slots[last_index].active_position = position;

		m_active_indices.pop_back();

		slot.active_position = kInvalidActivePosition;
	}

	UnitHandle UnitPool::create() {
		uint32_t index{};

		if (!m_free_list.empty()) {
			index = m_free_list.back();
			m_free_list.pop_back();
		}
		else {
			if (m_slots.size() >= m_max_units) {
				return {};
			}

			index = static_cast<uint32_t>(m_slots.size());
			m_slots.emplace_back();
			m_slots.back().generation = 1;
		}

		auto& slot = m_slots[index];

		slot.occupied = true;
		slot.unit = Unit{};
		slot.unit.id = index + 1;
		slot.unit.generation = slot.generation;
		slot.unit.alive = true;
		slot.unit.born_frame = m_frame_epoch;

		addActiveIndex(index);

		++m_alive_count;

		return UnitHandle{ slot.unit.id, slot.unit.generation };
	}

	bool UnitPool::destroy(UnitHandle const handle) noexcept {
		if (handle.id == 0) {
			return false;
		}

		auto const index = handle.id - 1;

		if (index >= m_slots.size()) {
			return false;
		}

		auto& slot = m_slots[index];

		if (!slot.occupied || slot.generation != handle.generation || !slot.unit.alive) {
			return false;
		}

		removeActiveIndex(index);

		slot.unit.alive = false;
		slot.occupied = false;

		advanceGeneration(slot.generation);

		m_free_list.push_back(index);
		--m_alive_count;

		return true;
	}

	void UnitPool::destroySlotFromNative(uint32_t const index) noexcept {
		if (index >= m_slots.size()) {
			return;
		}

		auto& slot = m_slots[index];

		if (!slot.occupied || !slot.unit.alive) {
			return;
		}

		// 记录旧 handle。
		// Lua UnitManager 用这个 handle 找 Lua wrapper，执行 Unit:del()，回收 Visual。
		m_native_killed.push_back(UnitHandle{
			slot.unit.id,
			slot.unit.generation,
		});

		removeActiveIndex(index);

		slot.unit.alive = false;
		slot.occupied = false;

		advanceGeneration(slot.generation);

		m_free_list.push_back(index);
		--m_alive_count;
	}

	Unit* UnitPool::get(UnitHandle const handle) noexcept {
		if (handle.id == 0) {
			return nullptr;
		}

		auto const index = handle.id - 1;

		if (index >= m_slots.size()) {
			return nullptr;
		}

		auto& slot = m_slots[index];

		if (!slot.occupied || slot.generation != handle.generation || !slot.unit.alive) {
			return nullptr;
		}

		return &slot.unit;
	}

	Unit const* UnitPool::get(UnitHandle const handle) const noexcept {
		if (handle.id == 0) {
			return nullptr;
		}

		auto const index = handle.id - 1;

		if (index >= m_slots.size()) {
			return nullptr;
		}

		auto const& slot = m_slots[index];

		if (!slot.occupied || slot.generation != handle.generation || !slot.unit.alive) {
			return nullptr;
		}

		return &slot.unit;
	}

	void UnitPool::beginFrame() noexcept {
		++m_frame_epoch;
		m_update_gate_epoch = m_frame_epoch;
	}

	void UnitPool::updateAll() noexcept {
		size_t i = 0;

		while (i < m_active_indices.size()) {
			auto const index = m_active_indices[i];

			if (index >= m_slots.size()) {
				++i;
				continue;
			}

			auto& slot = m_slots[index];

			if (!slot.occupied || !slot.unit.alive) {
				++i;
				continue;
			}

			auto& u = slot.unit;

			if (m_update_gate_epoch != 0 && u.born_frame == m_update_gate_epoch) {
				++i;
				continue;
			}

			u.vx += u.ax;
			u.vy += u.ay;

			syncUnitRotFromVelocity(u);

			u.x += u.vx;
			u.y += u.vy;

			++u.timer;

			if (u.bound) {
				if (
					u.x < m_bound_left ||
					u.x > m_bound_right ||
					u.y < m_bound_bottom ||
					u.y > m_bound_top
				) {
					destroySlotFromNative(index);

					// removeActiveIndex 使用 swap-with-last。
					// 当前 i 位置已经换入了另一个 active unit，所以这里不能 ++i。
					continue;
				}
			}

			++i;
		}

		m_update_gate_epoch = 0;
	}

	void UnitPool::clear() noexcept {
		m_free_list.clear();
		m_active_indices.clear();
		m_native_killed.clear();

		auto const slot_count = static_cast<uint32_t>(m_slots.size());

		for (uint32_t i = 0; i < slot_count; ++i) {
			auto& slot = m_slots[i];

			slot.occupied = false;
			slot.unit = Unit{};
			slot.active_position = kInvalidActivePosition;

			advanceGeneration(slot.generation);

			m_free_list.push_back(i);
		}

		m_alive_count = 0;
		m_frame_epoch = 0;
		m_update_gate_epoch = 0;
	}

	void UnitPool::setWorldBounds(
		double const left,
		double const right,
		double const bottom,
		double const top
	) noexcept {
		m_bound_left = left;
		m_bound_right = right;
		m_bound_bottom = bottom;
		m_bound_top = top;
	}

	std::vector<UnitHandle> UnitPool::consumeNativeKilled() {
		auto result = std::move(m_native_killed);
		m_native_killed.clear();
		return result;
	}

	size_t UnitPool::count() const noexcept {
		return m_alive_count;
	}

	UnitPool& GetUnitPool() noexcept {
		static UnitPool pool;
		return pool;
	}
}