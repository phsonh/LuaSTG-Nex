#include "Unit/UnitPool.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

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

			// 0 作为无效 handle 的默认 generation 值，避免有效 slot generation 变成 0。
			if (generation == 0) {
				++generation;
			}
		}
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

		slot.unit.alive = false;
		slot.occupied = false;
		advanceGeneration(slot.generation);
		m_free_list.push_back(index);
		--m_alive_count;
		return true;
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
		for (auto& slot : m_slots) {
			if (!slot.occupied || !slot.unit.alive) {
				continue;
			}

			auto& u = slot.unit;

			// Aether 的帧语义：
			// 本帧 Lua frame/task 中创建的 Unit，本帧不执行 native movement。
			if (m_update_gate_epoch != 0 && u.born_frame == m_update_gate_epoch) {
				continue;
			}

			u.vx += u.ax;
			u.vy += u.ay;
			syncUnitRotFromVelocity(u);
			u.x += u.vx;
			u.y += u.vy;
			++u.timer;
		}

		// 避免外部直接调用 lstg.Unit.updateAll() 时长期残留 gate。
		m_update_gate_epoch = 0;
	}



	void UnitPool::clear() noexcept {
		m_free_list.clear();

		auto const slot_count = static_cast<uint32_t>(m_slots.size());
		for (uint32_t i = 0; i < slot_count; ++i) {
			auto& slot = m_slots[i];

			slot.occupied = false;
			slot.unit = Unit{};
			advanceGeneration(slot.generation);

			m_free_list.push_back(i);
		}

		m_alive_count = 0;
		m_frame_epoch = 0;
		m_update_gate_epoch = 0;
	}

	size_t UnitPool::count() const noexcept {
		return m_alive_count;
	}

	UnitPool& GetUnitPool() noexcept {
		static UnitPool pool;
		return pool;
	}
}
