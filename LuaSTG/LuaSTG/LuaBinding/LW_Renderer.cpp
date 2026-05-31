#include "LuaBinding/LuaWrapper.hpp"
#include "lua/plus.hpp"
#include "LuaBinding/PostEffectShader.hpp"
#include "LuaBinding/modern/Unit.hpp"
#include "Unit/UnitPool.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "LuaBinding/modern/Vector2.hpp"
#include "LuaBinding/modern/Vector3.hpp"
#include "LuaBinding/modern/Vector4.hpp"
#include "LuaBinding/modern/RenderTarget.hpp"
#include "LuaBinding/modern/DepthStencilBuffer.hpp"
#include "AppFrame.h"
#include "GameResource/Implement/ResourceTextureImpl.hpp"
#include "GameResource/LegacyBlendStateHelper.hpp"

namespace luastg {
	inline core::Graphics::IRenderer* LR2D() { return LAPP.getRenderer2D(); }
	inline ResourceMgr& LRESMGR() { return LAPP.GetResourceMgr(); }

#ifndef NDEBUG
#define check_rendertarget_usage(P_TEXTURE) assert(!LAPP.GetRenderTargetManager()->CheckRenderTargetInUse(P_TEXTURE.get()));
#else
#define check_rendertarget_usage(P_TEXTURE)
#endif

#define validate_render_scope() if (!LR2D()->isBatchScope()) return luaL_error(L, "invalid render operation");

	enum class RenderError {
		None,
		SpriteNotFound,
		SpriteSequenceNotFound,
	};

	inline void rotate_float2(float& x, float& y, const float r) {
		float const sinv = std::sinf(r);
		float const cosv = std::cosf(r);
		float const tx = x * cosv - y * sinv;
		float const ty = x * sinv + y * cosv;
		x = tx;
		y = ty;
	}
	inline void rotate_float2x4(float& x1, float& y1, float& x2, float& y2, float& x3, float& y3, float& x4, float& y4, const float r) {
		float const sinv = std::sinf(r);
		float const cosv = std::cosf(r);
		{
			float const tx = x1 * cosv - y1 * sinv;
			float const ty = x1 * sinv + y1 * cosv;
			x1 = tx;
			y1 = ty;
		}
		{
			float const tx = x2 * cosv - y2 * sinv;
			float const ty = x2 * sinv + y2 * cosv;
			x2 = tx;
			y2 = ty;
		}
		{
			float const tx = x3 * cosv - y3 * sinv;
			float const ty = x3 * sinv + y3 * cosv;
			x3 = tx;
			y3 = ty;
		}
		{
			float const tx = x4 * cosv - y4 * sinv;
			float const ty = x4 * sinv + y4 * cosv;
			x4 = tx;
			y4 = ty;
		}
	}
	inline void translate_blend(core::Graphics::IRenderer*, const luastg::BlendMode blend) {
		LAPP.updateGraph2DBlendMode(blend);
	}
	inline core::Graphics::IRenderer::BlendState translate_blend_3d(const luastg::BlendMode blend) {
		[[maybe_unused]] auto const [v, b] = translateLegacyBlendState(blend);
		return b;
	}

	inline RenderError api_drawSprite(IResourceSprite* pimg2dres, float const x, float const y, float const rot, float const hscale, float const vscale, float const z) {
		pimg2dres->Render(x, y, rot, hscale, vscale, z);
		return RenderError::None;
	}
	inline RenderError api_drawSprite(IResourceSprite* pimg2dres, float const x, float const y, float const rot, float const hscale, float const vscale, BlendMode const blend, core::Color4B const color, float const z) {
		pimg2dres->Render(x, y, rot, hscale, vscale, blend, color, z);
		return RenderError::None;
	}
	inline RenderError api_drawSprite(char const* name, float const x, float const y, float const rot, float const hscale, float const vscale, float const z) {
		core::SmartReference<IResourceSprite> pimg2dres = LRESMGR().FindSprite(name);
		if (!pimg2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSprite failed, can't find sprite '{}'", name);
			return RenderError::SpriteNotFound;
		}
		return api_drawSprite(*pimg2dres, x, y, rot, hscale, vscale, z);
	}
	inline RenderError api_drawSprite(char const* name, float const x, float const y, float const rot, float const hscale, float const vscale, BlendMode const blend, core::Color4B const color, float const z) {
		core::SmartReference<IResourceSprite> pimg2dres = LRESMGR().FindSprite(name);
		if (!pimg2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSpriteEx failed, can't find sprite '{}'", name);
			return RenderError::SpriteNotFound;
		}
		return api_drawSprite(*pimg2dres, x, y, rot, hscale, vscale, blend, color, z);
	}
	inline RenderError api_drawSpriteRect(IResourceSprite* pimg2dres, float const l, float const r, float const b, float const t, float const z) {
		pimg2dres->RenderRect(l, r, b, t, z);
		return RenderError::None;
	}
	inline RenderError api_drawSpriteRect(char const* name, float const l, float const r, float const b, float const t, float const z) {
		core::SmartReference<IResourceSprite> pimg2dres = LRESMGR().FindSprite(name);
		if (!pimg2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSpriteRect failed, can't find sprite '{}'", name);
			return RenderError::SpriteNotFound;
		}
		return api_drawSpriteRect(*pimg2dres, l, r, b, t, z);
	}
	inline RenderError api_drawSprite4V(IResourceSprite* pimg2dres, float const x1, float const y1, float const z1, float const x2, float const y2, float const z2, float const x3, float const y3, float const z3, float const x4, float const y4, float const z4) {
		pimg2dres->Render4V(x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4);
		return RenderError::None;
	}
	inline RenderError api_drawSprite4V(char const* name, float const x1, float const y1, float const z1, float const x2, float const y2, float const z2, float const x3, float const y3, float const z3, float const x4, float const y4, float const z4) {
		core::SmartReference<IResourceSprite> pimg2dres = LRESMGR().FindSprite(name);
		if (!pimg2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSprite4V failed, can't find sprite '{}'", name);
			return RenderError::SpriteNotFound;
		}
		return api_drawSprite4V(*pimg2dres, x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4);
	}

	inline RenderError api_drawSpriteSequence(IResourceAnimation* pani2dres, int const ani_timer, float const x, float const y, float const rot, float const hscale, float const vscale, float const z) {
		pani2dres->Render(ani_timer, x, y, rot, hscale, vscale, z);
		return RenderError::None;
	}
	inline RenderError api_drawSpriteSequence(IResourceAnimation* pani2dres, int const ani_timer, float const x, float const y, float const rot, float const hscale, float const vscale, BlendMode const blend, core::Color4B const color, float const z) {
		pani2dres->Render(ani_timer, x, y, rot, hscale, vscale, blend, color, z);
		return RenderError::None;
	}
	inline RenderError api_drawSpriteSequence(char const* name, int const ani_timer, float const x, float const y, float const rot, float const hscale, float const vscale, float const z) {
		core::SmartReference<IResourceAnimation> pani2dres = LRESMGR().FindAnimation(name);
		if (!pani2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSpriteSequence failed, can't find sprite sequence '{}'", name);
			return RenderError::SpriteSequenceNotFound;
		}
		return api_drawSpriteSequence(*pani2dres, ani_timer, x, y, rot, hscale, vscale, z);
	}
	inline RenderError api_drawSpriteSequence(char const* name, int const ani_timer, float const x, float const y, float const rot, float const hscale, float const vscale, BlendMode const blend, core::Color4B const color, float const z) {
		core::SmartReference<IResourceAnimation> pani2dres = LRESMGR().FindAnimation(name);
		if (!pani2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawSpriteSequenceEx failed, can't find sprite sequence '{}'", name);
			return RenderError::SpriteSequenceNotFound;
		}
		return api_drawSpriteSequence(*pani2dres, ani_timer, x, y, rot, hscale, vscale, blend, color, z);
	}

	static void api_setFogState(float start, float end, core::Color4B color) {
		auto* ctx = LR2D();
		if (start != end) {
			if (start == -1.0f) {
				ctx->setFogState(core::Graphics::IRenderer::FogState::Exp, color, end, 0.0f);
			}
			else if (start == -2.0f) {
				ctx->setFogState(core::Graphics::IRenderer::FogState::Exp2, color, end, 0.0f);
			}
			else {
				ctx->setFogState(core::Graphics::IRenderer::FogState::Linear, color, start, end);
			}
		}
		else {
			ctx->setFogState(core::Graphics::IRenderer::FogState::Disable, core::Color4B(), 0.0f, 0.0f);
		}
	}

	static int lib_beginScene(lua_State* L)noexcept {
		if (!LR2D()->beginBatch())
			return luaL_error(L, "[luastg] lstg.Renderer.BeginScene failed");
		return 0;
	}
	static int lib_endScene(lua_State* L)noexcept {
		if (!LR2D()->endBatch())
			return luaL_error(L, "[luastg] lstg.Renderer.endScene failed");
		return 0;
	}

	static int lib_clearRenderTarget(lua_State* L)noexcept {
		core::Color4B color;
		if (lua_isnumber(L, 1)) {
			color = core::Color4B((uint32_t)lua_tonumber(L, 1));
		}
		else {
			color = *binding::Color::Cast(L, 1);
		}

		if (!LAPP.GetRenderTargetManager()->IsRenderTargetStackEmpty()) {
			uint16_t const r = color.a * color.r;
			color.r = static_cast<uint8_t>((r + ((r + 257) >> 8)) >> 8);
			uint16_t const g = color.a * color.g;
			color.g = static_cast<uint8_t>((g + ((g + 257) >> 8)) >> 8);
			uint16_t const b = color.a * color.b;
			color.b = static_cast<uint8_t>((b + ((b + 257) >> 8)) >> 8);
		}
		LR2D()->clearRenderTarget(color);
		return 0;
	}
	static int lib_clearDepthBuffer(lua_State* L)noexcept {
		LR2D()->clearDepthBuffer((float)luaL_checknumber(L, 1));
		return 0;
	}

	static int lib_setOrtho(lua_State* L)noexcept {
		core::BoxF box;
		if (lua_gettop(L) < 6) {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 4),
				0.0f,
				(float)luaL_checknumber(L, 2),
				(float)luaL_checknumber(L, 3),
				1.0f
			);
		}
		else {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 4),
				(float)luaL_checknumber(L, 5),
				(float)luaL_checknumber(L, 2),
				(float)luaL_checknumber(L, 3),
				(float)luaL_checknumber(L, 6)
			);
		}
		LR2D()->setOrtho(box);
		return 0;
	}
	static int lib_setPerspective(lua_State* L)noexcept {
		core::Vector3F eye;
		eye.x = (float)luaL_checknumber(L, 1);
		eye.y = (float)luaL_checknumber(L, 2);
		eye.z = (float)luaL_checknumber(L, 3);
		core::Vector3F lookat;
		lookat.x = (float)luaL_checknumber(L, 4);
		lookat.y = (float)luaL_checknumber(L, 5);
		lookat.z = (float)luaL_checknumber(L, 6);
		core::Vector3F headup;
		headup.x = (float)luaL_checknumber(L, 7);
		headup.y = (float)luaL_checknumber(L, 8);
		headup.z = (float)luaL_checknumber(L, 9);
		auto const fov = (float)luaL_checknumber(L, 10);
		auto const aspect_ratio = (float)luaL_checknumber(L, 11);
		core::Vector2F zrange;
		zrange.x = (float)luaL_checknumber(L, 12);
		zrange.y = (float)luaL_checknumber(L, 13);
		if (fov < 0.0f || fov >= L_PI_F)
			return luaL_error(L, "invalid parameters, require (0 < fov < pi ~= 3.1415...), receive (fov = %f)", fov);
		if (zrange.x <= 0.0f || zrange.y <= zrange.x)
			return luaL_error(L, "invalid parameters, require (0 < z_near < z_far), receive (z_near = %f, z_far = %f)", zrange.x, zrange.y);
		LR2D()->setPerspective(eye, lookat, headup, fov, aspect_ratio, zrange.x, zrange.y);
		return 0;
	}

	static int lib_setViewport(lua_State* L)noexcept {
		core::BoxF box;
		if (lua_gettop(L) < 6) {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 2),
				0.0f,
				(float)luaL_checknumber(L, 3),
				(float)luaL_checknumber(L, 4),
				1.0f
			);
		}
		else {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 2),
				(float)luaL_checknumber(L, 5),
				(float)luaL_checknumber(L, 3),
				(float)luaL_checknumber(L, 4),
				(float)luaL_checknumber(L, 6)
			);
		}
		LR2D()->setViewport(box);
		return 0;
	}
	static int lib_setScissorRect(lua_State* L)noexcept {
		LR2D()->setScissorRect(core::RectF(
			(float)luaL_checknumber(L, 1),
			(float)luaL_checknumber(L, 2),
			(float)luaL_checknumber(L, 3),
			(float)luaL_checknumber(L, 4)
		));
		return 0;
	}

	static int lib_setVertexColorBlendState(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->setVertexColorBlendState((core::Graphics::IRenderer::VertexColorBlendState)luaL_checkinteger(L, 1));
		return 0;
	}
	static int lib_setFogState(lua_State* L)noexcept {
		validate_render_scope();
		core::Color4B color;
		if (lua_isnumber(L, 2)) {
			color = core::Color4B((uint32_t)lua_tonumber(L, 2));
		}
		else {
			color = *binding::Color::Cast(L, 2);
		}
		LR2D()->setFogState(
			(core::Graphics::IRenderer::FogState)luaL_checkinteger(L, 1),
			color,
			(float)luaL_checknumber(L, 3),
			(float)luaL_optnumber(L, 4, 0.0));
		return 0;
	}
	static int lib_setDepthState(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->setDepthState((core::Graphics::IRenderer::DepthState)luaL_checkinteger(L, 1));
		return 0;
	}
	static int lib_setBlendState(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->setBlendState((core::Graphics::IRenderer::BlendState)luaL_checkinteger(L, 1));
		return 0;
	}
	static int lib_setTexture(lua_State* L)noexcept {
		validate_render_scope();
		char const* name = luaL_checkstring(L, 1);
		core::SmartReference<IResourceTexture> p = LRESMGR().FindTexture(name);
		if (!p) {
			spdlog::error("[luastg] lstg.Renderer.setTexture failed: can't find texture '{}'", name);
			return luaL_error(L, "can't find texture '%s'", name);
		}
		check_rendertarget_usage(p);
		LR2D()->setTexture(p->GetTexture());
		return 0;
	}

	static int lib_drawTriangle(lua_State* L) {
		validate_render_scope();

		core::Graphics::IRenderer::DrawVertex vertex[3];

		lua_rawgeti(L, 1, 1);
		lua_rawgeti(L, 1, 2);
		lua_rawgeti(L, 1, 3);
		lua_rawgeti(L, 1, 4);
		lua_rawgeti(L, 1, 5);
		lua_rawgeti(L, 1, 6);
		vertex[0].x = (float)luaL_checknumber(L, 4);
		vertex[0].y = (float)luaL_checknumber(L, 5);
		vertex[0].z = (float)luaL_checknumber(L, 6);
		vertex[0].u = (float)luaL_checknumber(L, 7);
		vertex[0].v = (float)luaL_checknumber(L, 8);
		vertex[0].color = (uint32_t)luaL_checknumber(L, 9);
		lua_pop(L, 6);

		lua_rawgeti(L, 2, 1);
		lua_rawgeti(L, 2, 2);
		lua_rawgeti(L, 2, 3);
		lua_rawgeti(L, 2, 4);
		lua_rawgeti(L, 2, 5);
		lua_rawgeti(L, 2, 6);
		vertex[1].x = (float)luaL_checknumber(L, 4);
		vertex[1].y = (float)luaL_checknumber(L, 5);
		vertex[1].z = (float)luaL_checknumber(L, 6);
		vertex[1].u = (float)luaL_checknumber(L, 7);
		vertex[1].v = (float)luaL_checknumber(L, 8);
		vertex[1].color = (uint32_t)luaL_checknumber(L, 9);
		lua_pop(L, 6);

		lua_rawgeti(L, 3, 1);
		lua_rawgeti(L, 3, 2);
		lua_rawgeti(L, 3, 3);
		lua_rawgeti(L, 3, 4);
		lua_rawgeti(L, 3, 5);
		lua_rawgeti(L, 3, 6);
		vertex[2].x = (float)luaL_checknumber(L, 4);
		vertex[2].y = (float)luaL_checknumber(L, 5);
		vertex[2].z = (float)luaL_checknumber(L, 6);
		vertex[2].u = (float)luaL_checknumber(L, 7);
		vertex[2].v = (float)luaL_checknumber(L, 8);
		vertex[2].color = (uint32_t)luaL_checknumber(L, 9);
		lua_pop(L, 6);

		LR2D()->drawTriangle(vertex[0], vertex[1], vertex[2]);
		return 0;
	}
	static int lib_drawQuad(lua_State* L) {
		validate_render_scope();

		core::Graphics::IRenderer::DrawVertex vertex[4];

		lua_rawgeti(L, 1, 1);
		lua_rawgeti(L, 1, 2);
		lua_rawgeti(L, 1, 3);
		lua_rawgeti(L, 1, 4);
		lua_rawgeti(L, 1, 5);
		lua_rawgeti(L, 1, 6);
		vertex[0].x = (float)luaL_checknumber(L, 5);
		vertex[0].y = (float)luaL_checknumber(L, 6);
		vertex[0].z = (float)luaL_checknumber(L, 7);
		vertex[0].u = (float)luaL_checknumber(L, 8);
		vertex[0].v = (float)luaL_checknumber(L, 9);
		vertex[0].color = (uint32_t)luaL_checknumber(L, 10);
		lua_pop(L, 6);

		lua_rawgeti(L, 2, 1);
		lua_rawgeti(L, 2, 2);
		lua_rawgeti(L, 2, 3);
		lua_rawgeti(L, 2, 4);
		lua_rawgeti(L, 2, 5);
		lua_rawgeti(L, 2, 6);
		vertex[1].x = (float)luaL_checknumber(L, 5);
		vertex[1].y = (float)luaL_checknumber(L, 6);
		vertex[1].z = (float)luaL_checknumber(L, 7);
		vertex[1].u = (float)luaL_checknumber(L, 8);
		vertex[1].v = (float)luaL_checknumber(L, 9);
		vertex[1].color = (uint32_t)luaL_checknumber(L, 10);
		lua_pop(L, 6);

		lua_rawgeti(L, 3, 1);
		lua_rawgeti(L, 3, 2);
		lua_rawgeti(L, 3, 3);
		lua_rawgeti(L, 3, 4);
		lua_rawgeti(L, 3, 5);
		lua_rawgeti(L, 3, 6);
		vertex[2].x = (float)luaL_checknumber(L, 5);
		vertex[2].y = (float)luaL_checknumber(L, 6);
		vertex[2].z = (float)luaL_checknumber(L, 7);
		vertex[2].u = (float)luaL_checknumber(L, 8);
		vertex[2].v = (float)luaL_checknumber(L, 9);
		vertex[2].color = (uint32_t)luaL_checknumber(L, 10);
		lua_pop(L, 6);

		lua_rawgeti(L, 4, 1);
		lua_rawgeti(L, 4, 2);
		lua_rawgeti(L, 4, 3);
		lua_rawgeti(L, 4, 4);
		lua_rawgeti(L, 4, 5);
		lua_rawgeti(L, 4, 6);
		vertex[3].x = (float)luaL_checknumber(L, 5);
		vertex[3].y = (float)luaL_checknumber(L, 6);
		vertex[3].z = (float)luaL_checknumber(L, 7);
		vertex[3].u = (float)luaL_checknumber(L, 8);
		vertex[3].v = (float)luaL_checknumber(L, 9);
		vertex[3].color = (uint32_t)luaL_checknumber(L, 10);
		lua_pop(L, 6);

		LR2D()->drawQuad(vertex[0], vertex[1], vertex[2], vertex[3]);
		return 0;
	}

	static core::Color4B optionalColor(lua_State* L, int const index, core::Color4B const default_value = core::Color4B::white()) {
		if (lua_gettop(L) < index || lua_isnil(L, index)) {
			return default_value;
		}
		if (lua_isnumber(L, index)) {
			return core::Color4B(static_cast<uint32_t>(lua_tonumber(L, index)));
		}
		return *binding::Color::Cast(L, index);
	}

	static int lib_drawSprite(lua_State* L) {
		validate_render_scope();
		float const hscale = (float)luaL_optnumber(L, 5, 1.0);
		RenderError re = api_drawSprite(
			luaL_checkstring(L, 1),
			(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
			(float)(luaL_optnumber(L, 4, 0.0) * L_DEG_TO_RAD),
			hscale * LRESMGR().GetGlobalImageScaleFactor(), (float)luaL_optnumber(L, 6, hscale) * LRESMGR().GetGlobalImageScaleFactor(),
			(float)luaL_optnumber(L, 7, 0.5));
		if (re == RenderError::SpriteNotFound) {
			return luaL_error(L, "can't find sprite '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}
	static int lib_drawSpriteEx(lua_State* L) {
		validate_render_scope();
		// New visual API: (img, x, y, z, rot=0, hscale=1, vscale=hscale, blend='mul+alpha', color=white)
		float const hscale = static_cast<float>(luaL_optnumber(L, 6, 1.0));
		auto const blend = lua_gettop(L) >= 8 && !lua_isnil(L, 8) ? TranslateBlendMode(L, 8) : BlendMode::MulAlpha;
		auto const color = optionalColor(L, 9);
		RenderError re = api_drawSprite(
			luaL_checkstring(L, 1),
			static_cast<float>(luaL_checknumber(L, 2)),
			static_cast<float>(luaL_checknumber(L, 3)),
			static_cast<float>(luaL_optnumber(L, 5, 0.0) * L_DEG_TO_RAD),
			hscale * LRESMGR().GetGlobalImageScaleFactor(),
			static_cast<float>(luaL_optnumber(L, 7, hscale)) * LRESMGR().GetGlobalImageScaleFactor(),
			blend,
			color,
			static_cast<float>(luaL_checknumber(L, 4)));
		if (re == RenderError::SpriteNotFound) {
			return luaL_error(L, "can't find sprite '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}
	
	
	
	static uint8_t clamp_u8(lua_Number value) noexcept {
		if (value < 0.0) {
			return 0;
		}
		if (value > 255.0) {
			return 255;
		}
		return static_cast<uint8_t>(value);
	}



	constexpr char const* kSpriteComponentMetatable = "lstg.Renderer.SpriteComponent.instance";

	struct SpriteComponentHandle {
		uint32_t id{};
		uint32_t generation{};
	};

	enum class SpriteOffsetMode : uint8_t {
		Local,
		World,
	};

	struct SpriteComponent {
		uint32_t id{};
		uint32_t generation{};
		bool alive{};

		bool visible{ true };
		bool render_enabled{ false };

		bool has_owner{};
		luastg::UnitHandle owner{};

		bool follow_position{ true };
		bool follow_rotation{ true };

		std::string sprite;
		std::vector<std::string> frames;
		uint32_t frame_interval{ 1 };
		bool loop{ true };

		double layer{};
		double z{ 0.5 };

		double x{};
		double y{};

		double offset_x{};
		double offset_y{};
		SpriteOffsetMode offset_mode{ SpriteOffsetMode::Local };

		double local_rot{};
		double rot_offset{};
		double spin{};

		double scale_x{ 1.0 };
		double scale_y{ 1.0 };

		BlendMode blend{ BlendMode::MulAlpha };

		uint8_t a{ 255 };
		uint8_t r{ 255 };
		uint8_t g{ 255 };
		uint8_t b{ 255 };

		uint64_t timer{};
	};

	struct SpriteComponentUserData {
		SpriteComponentHandle handle{};
	};

	class SpriteComponentPool {
	public:
		static constexpr size_t kDefaultMaxComponents = 65536;
		static constexpr uint32_t kInvalidActivePosition = UINT32_MAX;
		static constexpr uint32_t kInvalidUpdatePosition = UINT32_MAX;

		SpriteComponentHandle create(luastg::UnitHandle const owner, bool const has_owner) {
			uint32_t index{};

			if (!m_free_list.empty()) {
				index = m_free_list.back();
				m_free_list.pop_back();
			}
			else {
				if (m_slots.size() >= m_max_components) {
					return {};
				}

				index = static_cast<uint32_t>(m_slots.size());
				m_slots.emplace_back();
				m_slots.back().generation = 1;
				m_slots.back().active_position = kInvalidActivePosition;
			}

			auto& slot = m_slots[index];

			slot.component = SpriteComponent{};
			slot.component.id = index + 1;
			slot.component.generation = slot.generation;
			slot.component.alive = true;
			slot.component.sprite = "img_void";
			slot.component.owner = owner;
			slot.component.has_owner = has_owner;

			slot.active_position = static_cast<uint32_t>(m_active_indices.size());
			m_active_indices.push_back(index);

			++m_alive_count;

			return SpriteComponentHandle{
				slot.component.id,
				slot.component.generation,
			};
		}

		bool destroy(SpriteComponentHandle const handle) noexcept {
			auto* component = get(handle);

			if (!component) {
				return false;
			}

			auto const index = handle.id - 1;
			removeUpdateIndex(index);
			removeActiveIndex(index);

			auto& slot = m_slots[index];

			slot.component.alive = false;
			slot.component = SpriteComponent{};

			advanceGeneration(slot.generation);

			m_free_list.push_back(index);
			--m_alive_count;

			return true;
		}

		SpriteComponent* get(SpriteComponentHandle const handle) noexcept {
			if (handle.id == 0) {
				return nullptr;
			}

			auto const index = handle.id - 1;

			if (index >= m_slots.size()) {
				return nullptr;
			}

			auto& slot = m_slots[index];

			if (slot.component.generation != handle.generation || !slot.component.alive) {
				return nullptr;
			}

			return &slot.component;
		}

		SpriteComponent const* get(SpriteComponentHandle const handle) const noexcept {
			if (handle.id == 0) {
				return nullptr;
			}

			auto const index = handle.id - 1;

			if (index >= m_slots.size()) {
				return nullptr;
			}

			auto const& slot = m_slots[index];

			if (slot.component.generation != handle.generation || !slot.component.alive) {
				return nullptr;
			}

			return &slot.component;
		}


		void updateAll() noexcept {
			for (auto const index : m_update_indices) {
				if (index >= m_slots.size()) {
					continue;
				}

				auto& c = m_slots[index].component;

				if (!c.alive) {
					continue;
				}

				if (c.spin != 0.0) {
					c.local_rot += c.spin;
				}

				// timer 目前只服务 native frames。
				// 没有 frames 的 component 不需要 native timer。
				if (!c.frames.empty()) {
					++c.timer;
				}
			}
		}

		void beginRenderFrame() noexcept {
			// 这里稳定压缩 active list。
			// 删除留下的洞会被移除，但剩余元素的相对顺序不变。
			compactActiveIndicesStable();

			for (auto const index : m_active_indices) {
				if (index == kInvalidActivePosition || index >= m_slots.size()) {
					continue;
				}

				auto& c = m_slots[index].component;

				m_slots[index].component.render_enabled = false;
			}
		}

		void renderLayerInternal(double const layer) {
			size_t pos = 0;

			while (pos < m_active_indices.size()) {
				auto const index = m_active_indices[pos];

				if (index == kInvalidActivePosition || index >= m_slots.size()) {
					++pos;
					continue;
				}

				auto const& c = m_slots[index].component;

				if (!isRenderableInLayer(c, layer)) {
					++pos;
					continue;
				}

				pos = emitSpriteBatchRun(pos, layer);
			}
		}

		void renderLayer(double const layer) {
			compactActiveIndicesStable();
			renderLayerInternal(layer);
		}

		void renderAll() {
			compactActiveIndicesStable();

			m_render_layers.clear();

			for (auto const index : m_active_indices) {
				if (index == kInvalidActivePosition || index >= m_slots.size()) {
					continue;
				}

				auto const& c = m_slots[index].component;

				if (!c.alive || !c.visible || !c.render_enabled) {
					continue;
				}

				pushRenderLayer(c.layer);
			}

			if (m_render_layers.size() > 1) {
				std::sort(m_render_layers.begin(), m_render_layers.end());
			}

			for (auto const layer : m_render_layers) {
				renderLayerInternal(layer);
			}
		}

		void clear() noexcept {
			m_active_indices.clear();
			m_update_indices.clear();
			m_free_list.clear();
			m_active_tombstone_count = 0;

			auto const slot_count = static_cast<uint32_t>(m_slots.size());

			for (uint32_t i = 0; i < slot_count; ++i) {
				auto& slot = m_slots[i];

				slot.component = SpriteComponent{};
				slot.active_position = kInvalidActivePosition;
				slot.update_position = kInvalidUpdatePosition;

				advanceGeneration(slot.generation);

				m_free_list.push_back(i);
			}

			m_alive_count = 0;
		}

		[[nodiscard]] size_t count() const noexcept {
			return m_alive_count;
		}

	private:
		struct Slot {
			SpriteComponent component{};
			uint32_t generation{ 1 };
			uint32_t active_position{ kInvalidActivePosition };
			uint32_t update_position{ kInvalidUpdatePosition };
		};



		struct SpriteBatchMaterial {
			std::string sprite_name;
			core::SmartReference<IResourceSprite> resource;
			BlendMode blend{ BlendMode::MulAlpha };

			core::Graphics::IRenderer::VertexColorBlendState vertex_color_blend_state{};
			core::Graphics::IRenderer::BlendState blend_state{};
		};


		static void advanceGeneration(uint32_t& generation) noexcept {
			++generation;

			if (generation == 0) {
				++generation;
			}
		}

		void removeActiveIndex(uint32_t const index) noexcept {
			if (index >= m_slots.size()) {
				return;
			}

			auto& slot = m_slots[index];
			auto const position = slot.active_position;

			if (position == kInvalidActivePosition) {
				return;
			}

			// 渲染顺序必须稳定：
			// 不能把最后一个 active sprite 换到当前位置，
			// 否则同 layer 内“后生成覆盖先生成”的顺序会被打乱。
			if (position < m_active_indices.size() && m_active_indices[position] != kInvalidActivePosition) {
				m_active_indices[position] = kInvalidActivePosition;
				++m_active_tombstone_count;
			}

			slot.active_position = kInvalidActivePosition;
		}

		static bool componentNeedsUpdate(SpriteComponent const& c) noexcept {
			return c.alive && (c.spin != 0.0 || !c.frames.empty());
		}

		void addUpdateIndex(uint32_t const index) {
			if (index >= m_slots.size()) {
				return;
			}

			auto& slot = m_slots[index];

			if (slot.update_position != kInvalidUpdatePosition) {
				return;
			}

			slot.update_position = static_cast<uint32_t>(m_update_indices.size());
			m_update_indices.push_back(index);
		}

		void removeUpdateIndex(uint32_t const index) noexcept {
			if (index >= m_slots.size()) {
				return;
			}

			auto& slot = m_slots[index];
			auto const position = slot.update_position;

			if (position == kInvalidUpdatePosition) {
				return;
			}

			auto const last_index = m_update_indices.back();

			m_update_indices[position] = last_index;
			m_slots[last_index].update_position = position;

			m_update_indices.pop_back();

			slot.update_position = kInvalidUpdatePosition;
		}

		void refreshUpdateIndex(uint32_t const index) {
			if (index >= m_slots.size()) {
				return;
			}

			auto const& c = m_slots[index].component;

			if (componentNeedsUpdate(c)) {
				addUpdateIndex(index);
			}
			else {
				removeUpdateIndex(index);
			}
		}

		public:
		void refreshUpdateIndex(SpriteComponentHandle const handle) {
			if (handle.id == 0) {
				return;
			}

			auto const index = handle.id - 1;

			if (index >= m_slots.size()) {
				return;
			}

			auto const& slot = m_slots[index];

			if (slot.component.generation != handle.generation || !slot.component.alive) {
				return;
			}

			refreshUpdateIndex(index);
		}
		private:


		void compactActiveIndicesStable() noexcept {
			if (m_active_tombstone_count == 0) {
				return;
			}

			size_t write = 0;

			for (size_t read = 0; read < m_active_indices.size(); ++read) {
				auto const index = m_active_indices[read];

				if (index == kInvalidActivePosition) {
					continue;
				}

				if (index >= m_slots.size()) {
					continue;
				}

				auto& slot = m_slots[index];

				if (!slot.component.alive) {
					slot.active_position = kInvalidActivePosition;
					continue;
				}

				m_active_indices[write] = index;
				slot.active_position = static_cast<uint32_t>(write);
				++write;
			}

			m_active_indices.resize(write);
			m_active_tombstone_count = 0;
		}

		void pushRenderLayer(double const layer) {
			for (auto const existing : m_render_layers) {
				if (existing == layer) {
					return;
				}
			}

			m_render_layers.push_back(layer);
		}

		static char const* currentSpriteName(SpriteComponent const& c) noexcept {
			if (!c.frames.empty()) {
				auto const interval = std::max<uint32_t>(1, c.frame_interval);
				auto frame_index = static_cast<size_t>(c.timer / interval);

				if (c.loop) {
					frame_index %= c.frames.size();
				}
				else if (frame_index >= c.frames.size()) {
					frame_index = c.frames.size() - 1;
				}

				return c.frames[frame_index].c_str();
			}

			return c.sprite.c_str();
		}


		

		static void rotateOffset(double& x, double& y, double const degrees) noexcept {
			auto const rad = degrees * L_DEG_TO_RAD;
			auto const sinv = std::sin(rad);
			auto const cosv = std::cos(rad);

			auto const tx = x * cosv - y * sinv;
			auto const ty = x * sinv + y * cosv;

			x = tx;
			y = ty;
		}


		static constexpr size_t kMaxBatchQuads = 10000;

		bool makeMaterial(SpriteComponent const& c, SpriteBatchMaterial& out) {
			char const* sprite_name = currentSpriteName(c);

			if (sprite_name == nullptr || sprite_name[0] == '\0') {
				return false;
			}

			auto resource = LRESMGR().FindSprite(sprite_name);

			if (!resource || resource->GetSprite() == nullptr || resource->GetSprite()->getTexture() == nullptr) {
				return false;
			}

			auto const blend = translateLegacyBlendState(c.blend);

			out.sprite_name = sprite_name;
			out.resource = resource;
			out.blend = c.blend;
			out.vertex_color_blend_state = blend.vertex_color_blend_state;
			out.blend_state = blend.blend_state;

			return true;
		}

		bool isSameMaterial(SpriteComponent const& c, SpriteBatchMaterial const& material) {
			if (c.blend != material.blend) {
				return false;
			}

			char const* sprite_name = currentSpriteName(c);

			if (sprite_name == nullptr) {
				return false;
			}

			return material.sprite_name == sprite_name;
		}

		static void writeQuadVertices(
			core::Graphics::IRenderer::DrawVertex* const vertices,
			core::Graphics::IRenderer::DrawIndex* const indices,
			uint16_t const index_offset,
			size_t const quad_index,
			SpriteComponent const& c,
			IResourceSprite* const resource,
			float const global_scale
		) {
			auto* sprite = resource->GetSprite();

			auto const texture_rect = sprite->getTextureRect();
			auto const texture_size = sprite->getTexture()->getSize();

			auto const u_scale = 1.0f / static_cast<float>(texture_size.x);
			auto const v_scale = 1.0f / static_cast<float>(texture_size.y);

			float const u0 = texture_rect.a.x * u_scale;
			float const v0 = texture_rect.a.y * v_scale;
			float const u1 = texture_rect.b.x * u_scale;
			float const v1 = texture_rect.b.y * v_scale;

			auto const center = texture_rect.a + sprite->getTextureCenter();
			auto const rect0 = texture_rect - center;

			float const unit_scale = sprite->getUnitsPerPixel();

			float const sx = static_cast<float>(c.scale_x * resource->GetScaleX() * global_scale);
			float const sy = static_cast<float>(c.scale_y * resource->GetScaleY() * global_scale);

			float x0 = rect0.a.x * unit_scale * sx;
			float y0 = rect0.a.y * -unit_scale * sy;
			float x1 = rect0.b.x * unit_scale * sx;
			float y1 = rect0.b.y * -unit_scale * sy;

			float px{};
			float py{};
			float rot_degree{};

			resolveTransform(c, px, py, rot_degree);

			float vx[4] = {
				x0,
				x1,
				x1,
				x0,
			};

			float vy[4] = {
				y0,
				y0,
				y1,
				y1,
			};

			float const rot = static_cast<float>(rot_degree * L_DEG_TO_RAD);

			if (std::fabs(rot) > std::numeric_limits<float>::min()) {
				float const sinv = std::sinf(rot);
				float const cosv = std::cosf(rot);

				for (int i = 0; i < 4; ++i) {
					float const tx = vx[i] * cosv - vy[i] * sinv;
					float const ty = vx[i] * sinv + vy[i] * cosv;

					vx[i] = tx;
					vy[i] = ty;
				}
			}

			auto const color = core::Color4B(c.r, c.g, c.b, c.a).color();
			float const z = static_cast<float>(c.z);

			auto* v = vertices + quad_index * 4;

			v[0].x = px + vx[0];
			v[0].y = py + vy[0];
			v[0].z = z;
			v[0].u = u0;
			v[0].v = v0;
			v[0].color = color;

			v[1].x = px + vx[1];
			v[1].y = py + vy[1];
			v[1].z = z;
			v[1].u = u1;
			v[1].v = v0;
			v[1].color = color;

			v[2].x = px + vx[2];
			v[2].y = py + vy[2];
			v[2].z = z;
			v[2].u = u1;
			v[2].v = v1;
			v[2].color = color;

			v[3].x = px + vx[3];
			v[3].y = py + vy[3];
			v[3].z = z;
			v[3].u = u0;
			v[3].v = v1;
			v[3].color = color;

			auto* idx = indices + quad_index * 6;

			uint16_t const base = static_cast<uint16_t>(index_offset + quad_index * 4);

			idx[0] = base + 0;
			idx[1] = base + 1;
			idx[2] = base + 2;
			idx[3] = base + 2;
			idx[4] = base + 3;
			idx[5] = base + 0;
		}

		bool isRenderableInLayer(SpriteComponent const& c, double const layer) const noexcept {
			return c.alive
				&& c.visible
				&& c.render_enabled
				&& c.layer == layer;
		}

		size_t emitSpriteBatchRun(size_t const begin_pos, double const layer) {
			if (begin_pos >= m_active_indices.size()) {
				return begin_pos + 1;
			}

			auto const first_index = m_active_indices[begin_pos];

			if (first_index == kInvalidActivePosition || first_index >= m_slots.size()) {
				return begin_pos + 1;
			}

			auto const& first_component = m_slots[first_index].component;

			if (!isRenderableInLayer(first_component, layer)) {
				return begin_pos + 1;
			}

			SpriteBatchMaterial material;

			if (!makeMaterial(first_component, material)) {
				return begin_pos + 1;
			}

			size_t count = 0;
			size_t end_pos = begin_pos;

			for (size_t pos = begin_pos; pos < m_active_indices.size() && count < kMaxBatchQuads; ++pos) {
				auto const index = m_active_indices[pos];

				if (index == kInvalidActivePosition || index >= m_slots.size()) {
					end_pos = pos + 1;
					continue;
				}

				auto const& c = m_slots[index].component;

				if (!isRenderableInLayer(c, layer)) {
					end_pos = pos + 1;
					continue;
				}

				if (!isSameMaterial(c, material)) {
					break;
				}

				++count;
				end_pos = pos + 1;
			}

			if (count == 0) {
				return begin_pos + 1;
			}

			auto* renderer = LR2D();

			renderer->setVertexColorBlendState(material.vertex_color_blend_state);
			renderer->setBlendState(material.blend_state);
			renderer->setTexture(material.resource->GetSprite()->getTexture());

			core::Graphics::IRenderer::DrawVertex* vertices = nullptr;
			core::Graphics::IRenderer::DrawIndex* indices = nullptr;
			uint16_t index_offset = 0;

			uint16_t const vertex_count = static_cast<uint16_t>(count * 4);
			uint16_t const index_count = static_cast<uint16_t>(count * 6);

			if (!renderer->drawRequest(vertex_count, index_count, &vertices, &indices, &index_offset)) {
				// 保底：如果 drawRequest 失败，退回旧路径，至少不丢渲染。
				for (size_t pos = begin_pos; pos < end_pos; ++pos) {
					auto const index = m_active_indices[pos];

					if (index == kInvalidActivePosition || index >= m_slots.size()) {
						continue;
					}

					auto& c = m_slots[index].component;

					if (!isRenderableInLayer(c, layer) || !isSameMaterial(c, material)) {
						continue;
					}

					float x{};
					float y{};
					float rot{};

					resolveTransform(c, x, y, rot);

					core::Color4B const color(c.r, c.g, c.b, c.a);

					api_drawSprite(
						*material.resource,
						x,
						y,
						static_cast<float>(rot * L_DEG_TO_RAD),
						static_cast<float>(c.scale_x * LRESMGR().GetGlobalImageScaleFactor()),
						static_cast<float>(c.scale_y * LRESMGR().GetGlobalImageScaleFactor()),
						c.blend,
						color,
						static_cast<float>(c.z)
					);
				}

				return end_pos;
			}

			float const global_scale = LRESMGR().GetGlobalImageScaleFactor();

			size_t written = 0;

			for (size_t pos = begin_pos; pos < end_pos && written < count; ++pos) {
				auto const index = m_active_indices[pos];

				if (index == kInvalidActivePosition || index >= m_slots.size()) {
					continue;
				}

				auto const& c = m_slots[index].component;

				if (!isRenderableInLayer(c, layer) || !isSameMaterial(c, material)) {
					continue;
				}

				writeQuadVertices(
					vertices,
					indices,
					index_offset,
					written,
					c,
					*material.resource,
					global_scale
				);

				++written;
			}

			return end_pos;
		}

		static void resolveTransform(SpriteComponent const& c, float& out_x, float& out_y, float& out_rot) noexcept {
			double base_x = c.x;
			double base_y = c.y;
			double base_rot = 0.0;

			luastg::Unit const* owner = nullptr;

			if (c.has_owner) {
				owner = luastg::GetUnitPool().get(c.owner);
			}

			if (owner) {
				if (c.follow_position) {
					base_x = owner->x;
					base_y = owner->y;
				}

				if (c.follow_rotation) {
					base_rot = owner->rot;
				}
			}

			double ox = c.offset_x;
			double oy = c.offset_y;

			if (c.offset_mode == SpriteOffsetMode::Local && c.follow_rotation) {
				rotateOffset(ox, oy, base_rot);
			}

			out_x = static_cast<float>(base_x + ox);
			out_y = static_cast<float>(base_y + oy);
			out_rot = static_cast<float>(base_rot + c.rot_offset + c.local_rot);
		}

		std::vector<Slot> m_slots;
		std::vector<uint32_t> m_free_list;
		std::vector<uint32_t> m_active_indices;
		std::vector<uint32_t> m_update_indices;

		// active_indices 中被删除后留下的空洞数量。
		// 渲染顺序要求稳定，所以不能用 swap-with-last 删除。
		size_t m_active_tombstone_count{};

		// renderAll() 复用的 layer 缓存，避免每帧重复分配。
		std::vector<double> m_render_layers;

		size_t m_alive_count{};
		size_t m_max_components{ kDefaultMaxComponents };
	};

	SpriteComponentPool& GetSpriteComponentPool() noexcept {
		static SpriteComponentPool pool;
		return pool;
	}

	SpriteComponentUserData* check_sprite_component_userdata(lua_State* const L, int const index) {
		return static_cast<SpriteComponentUserData*>(
			luaL_checkudata(L, index, kSpriteComponentMetatable)
		);
	}

	SpriteComponent* check_sprite_component(lua_State* const L, int const index) {
		auto const ud = check_sprite_component_userdata(L, index);
		auto* component = GetSpriteComponentPool().get(ud->handle);

		if (!component) {
			luaL_error(L, "invalid or destroyed lstg.Renderer.SpriteComponent");
			return nullptr;
		}

		return component;
	}

	void push_sprite_component(lua_State* const L, SpriteComponentHandle const handle) {
		auto* ud = static_cast<SpriteComponentUserData*>(
			lua_newuserdata(L, sizeof(SpriteComponentUserData))
		);

		ud->handle = handle;

		luaL_getmetatable(L, kSpriteComponentMetatable);
		lua_setmetatable(L, -2);
	}

	bool read_optional_unit_handle(lua_State* const L, int const index, luastg::UnitHandle& out) {
		if (lua_isnoneornil(L, index)) {
			out = {};
			return false;
		}

		if (!luastg::binding::Unit::checkHandle(L, index, out)) {
			out = {};
			return false;
		}

		return true;
	}

	BlendMode optional_blend(lua_State* const L, int const index) {
		if (lua_isnoneornil(L, index)) {
			return BlendMode::MulAlpha;
		}

		return TranslateBlendMode(L, index);
	}

	static int sprite_component_new(lua_State* L) {
		luastg::UnitHandle owner{};
		bool const has_owner = read_optional_unit_handle(L, 1, owner);

		auto const handle = GetSpriteComponentPool().create(owner, has_owner);

		if (handle.id == 0) {
			return luaL_error(L, "SpriteComponentPool is full");
		}

		push_sprite_component(L, handle);
		return 1;
	}

	static int sprite_component_delete(lua_State* L) {
		auto const ud = check_sprite_component_userdata(L, 1);
		lua_pushboolean(L, GetSpriteComponentPool().destroy(ud->handle));
		return 1;
	}

	static int sprite_component_is_valid(lua_State* L) {
		auto const ud = check_sprite_component_userdata(L, 1);
		lua_pushboolean(L, GetSpriteComponentPool().get(ud->handle) != nullptr);
		return 1;
	}

	static int sprite_component_set_owner(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		luastg::UnitHandle owner{};
		bool const has_owner = read_optional_unit_handle(L, 2, owner);

		c->owner = owner;
		c->has_owner = has_owner;

		return 0;
	}

	static int sprite_component_clear_owner(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->owner = {};
		c->has_owner = false;

		return 0;
	}

	static int sprite_component_set_sprite(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->sprite = luaL_checkstring(L, 2);
		c->frames.clear();

		auto const ud = check_sprite_component_userdata(L, 1);
		GetSpriteComponentPool().refreshUpdateIndex(ud->handle);

		return 0;
	}

	static int sprite_component_set_frames(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->frames.clear();

		if (lua_isnoneornil(L, 2)) {
			return 0;
		}

		luaL_checktype(L, 2, LUA_TTABLE);

		auto const count = static_cast<int>(lua_objlen(L, 2));

		c->frames.reserve(static_cast<size_t>(count));

		for (int i = 1; i <= count; ++i) {
			lua_rawgeti(L, 2, i);

			if (!lua_isnil(L, -1)) {
				c->frames.emplace_back(luaL_checkstring(L, -1));
			}

			lua_pop(L, 1);
		}

		c->timer = 0;

		auto const ud = check_sprite_component_userdata(L, 1);
		GetSpriteComponentPool().refreshUpdateIndex(ud->handle);

		return 0;
	}

	static int sprite_component_set_frame_interval(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		auto interval = static_cast<uint32_t>(std::max<lua_Integer>(1, luaL_checkinteger(L, 2)));
		c->frame_interval = interval;

		return 0;
	}

	static int sprite_component_set_loop(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->loop = lua_toboolean(L, 2) != 0;

		return 0;
	}

	static int sprite_component_reset_timer(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->timer = static_cast<uint64_t>(std::max<lua_Integer>(0, luaL_optinteger(L, 2, 0)));

		return 0;
	}

	static int sprite_component_set_visible(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->visible = lua_toboolean(L, 2) != 0;

		return 0;
	}

	static int sprite_component_set_render_enabled(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->render_enabled = lua_toboolean(L, 2) != 0;

		return 0;
	}

	static int sprite_component_set_layer(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->layer = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_z(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->z = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_blend(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->blend = optional_blend(L, 2);

		return 0;
	}

	static int sprite_component_follow_master(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		bool const value = lua_toboolean(L, 2) != 0;

		c->follow_position = value;
		c->follow_rotation = value;

		return 0;
	}

	static int sprite_component_follow_position(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->follow_position = lua_toboolean(L, 2) != 0;

		return 0;
	}

	static int sprite_component_follow_rotation(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->follow_rotation = lua_toboolean(L, 2) != 0;

		return 0;
	}

	static int sprite_component_set_position(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->x = luaL_checknumber(L, 2);
		c->y = luaL_checknumber(L, 3);

		return 0;
	}

	static int sprite_component_set_x(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->x = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_y(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->y = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_offset(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->offset_x = luaL_checknumber(L, 2);
		c->offset_y = luaL_checknumber(L, 3);

		return 0;
	}

	static int sprite_component_set_offset_mode(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		char const* mode = luaL_checkstring(L, 2);

		if (std::strcmp(mode, "local") == 0) {
			c->offset_mode = SpriteOffsetMode::Local;
		}
		else if (std::strcmp(mode, "world") == 0) {
			c->offset_mode = SpriteOffsetMode::World;
		}
		else {
			return luaL_error(L, "offset mode must be 'local' or 'world'");
		}

		return 0;
	}

	static int sprite_component_set_local_rot(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->local_rot = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_rot_offset(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->rot_offset = luaL_checknumber(L, 2);

		return 0;
	}

	static int sprite_component_set_spin(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->spin = luaL_checknumber(L, 2);

		auto const ud = check_sprite_component_userdata(L, 1);
		GetSpriteComponentPool().refreshUpdateIndex(ud->handle);

		return 0;
	}

	static int sprite_component_set_scale(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->scale_x = luaL_checknumber(L, 2);
		c->scale_y = c->scale_x;

		return 0;
	}

	static int sprite_component_set_scale_xy(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->scale_x = luaL_checknumber(L, 2);
		c->scale_y = luaL_checknumber(L, 3);

		return 0;
	}

	static int sprite_component_set_alpha(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->a = clamp_u8(luaL_checknumber(L, 2));

		return 0;
	}

	static int sprite_component_set_color(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->r = clamp_u8(luaL_checknumber(L, 2));
		c->g = clamp_u8(luaL_checknumber(L, 3));
		c->b = clamp_u8(luaL_checknumber(L, 4));

		return 0;
	}

	static int sprite_component_set_rgba(lua_State* L) {
		auto* c = check_sprite_component(L, 1);

		c->r = clamp_u8(luaL_checknumber(L, 2));
		c->g = clamp_u8(luaL_checknumber(L, 3));
		c->b = clamp_u8(luaL_checknumber(L, 4));
		c->a = clamp_u8(luaL_checknumber(L, 5));

		return 0;
	}

	static int sprite_component_update_all(lua_State* L) {
		GetSpriteComponentPool().updateAll();
		return 0;
	}

	static int sprite_component_begin_render_frame(lua_State* L) {
		GetSpriteComponentPool().beginRenderFrame();
		return 0;
	}

	static int sprite_component_render_layer(lua_State* L) {
		validate_render_scope();

		GetSpriteComponentPool().renderLayer(luaL_checknumber(L, 1));

		return 0;
	}

	static int sprite_component_render_all(lua_State* L) {
		validate_render_scope();

		GetSpriteComponentPool().renderAll();

		return 0;
	}


	static int sprite_component_clear(lua_State* L) {
		GetSpriteComponentPool().clear();
		return 0;
	}

	static int sprite_component_count(lua_State* L) {
		lua_pushinteger(L, static_cast<lua_Integer>(GetSpriteComponentPool().count()));
		return 1;
	}

	static int sprite_component_gc(lua_State* L) {
		auto const ud = check_sprite_component_userdata(L, 1);

		GetSpriteComponentPool().destroy(ud->handle);

		return 0;
	}

	static int sprite_component_tostring(lua_State* L) {
		auto const ud = check_sprite_component_userdata(L, 1);
		auto* c = GetSpriteComponentPool().get(ud->handle);

		if (c) {
			lua_pushfstring(L, "lstg.Renderer.SpriteComponent<%u:%u>", c->id, c->generation);
		}
		else {
			lua_pushfstring(L, "lstg.Renderer.SpriteComponent<destroyed:%u:%u>", ud->handle.id, ud->handle.generation);
		}

		return 1;
	}

	static void register_sprite_component(lua_State* L) {
		luaL_Reg const methods[] = {
			{ "delete", &sprite_component_delete },
			{ "destroy", &sprite_component_delete },
			{ "isValid", &sprite_component_is_valid },

			{ "setOwner", &sprite_component_set_owner },
			{ "clearOwner", &sprite_component_clear_owner },

			{ "setSprite", &sprite_component_set_sprite },
			{ "setImage", &sprite_component_set_sprite },
			{ "setFrames", &sprite_component_set_frames },
			{ "setFrameInterval", &sprite_component_set_frame_interval },
			{ "setLoop", &sprite_component_set_loop },
			{ "resetTimer", &sprite_component_reset_timer },

			{ "setVisible", &sprite_component_set_visible },
			{ "setRenderEnabled", &sprite_component_set_render_enabled },

			{ "setLayer", &sprite_component_set_layer },
			{ "setZ", &sprite_component_set_z },
			{ "setBlend", &sprite_component_set_blend },

			{ "followMaster", &sprite_component_follow_master },
			{ "followPosition", &sprite_component_follow_position },
			{ "followRotation", &sprite_component_follow_rotation },

			{ "setPosition", &sprite_component_set_position },
			{ "setX", &sprite_component_set_x },
			{ "setY", &sprite_component_set_y },
			{ "setOffset", &sprite_component_set_offset },
			{ "setOffsetMode", &sprite_component_set_offset_mode },

			{ "setLocalRot", &sprite_component_set_local_rot },
			{ "setRotOffset", &sprite_component_set_rot_offset },
			{ "setSpin", &sprite_component_set_spin },

			{ "setScale", &sprite_component_set_scale },
			{ "setScaleXY", &sprite_component_set_scale_xy },

			{ "setAlpha", &sprite_component_set_alpha },
			{ "setColor", &sprite_component_set_color },
			{ "setRGBA", &sprite_component_set_rgba },

			{ nullptr, nullptr },
		};

		luaL_Reg const metamethods[] = {
			{ "__gc", &sprite_component_gc },
			{ "__tostring", &sprite_component_tostring },
			{ nullptr, nullptr },
		};

		if (luaL_newmetatable(L, kSpriteComponentMetatable)) {
			luaL_register(L, nullptr, metamethods);

			lua_newtable(L);
			luaL_register(L, nullptr, methods);
			lua_setfield(L, -2, "__index");
		}

		lua_pop(L, 1);

		luaL_Reg const api[] = {
			{ "new", &sprite_component_new },
			{ "updateAll", &sprite_component_update_all },
			{ "beginRenderFrame", &sprite_component_begin_render_frame },
			{ "renderLayer", &sprite_component_render_layer },
			{ "renderAll", &sprite_component_render_all },
			{ "clear", &sprite_component_clear },
			{ "count", &sprite_component_count },
			{ nullptr, nullptr },
		};

		lua_newtable(L);
		luaL_register(L, nullptr, api);
		lua_setfield(L, -2, "SpriteComponent");
	}

	static int lib_Sprite(lua_State* L) {
		validate_render_scope();

		// New Nex API:
		// lstg.Renderer.Sprite(
		//     img,
		//     x,
		//     y,
		//     rot,
		//     scale_x,
		//     scale_y,
		//     blend,
		//     a,
		//     r,
		//     g,
		//     b,
		//     z
		// )

		char const* img = luaL_checkstring(L, 1);

		float const x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
		float const y = static_cast<float>(luaL_optnumber(L, 3, 0.0));
		float const rot = static_cast<float>(luaL_optnumber(L, 4, 0.0) * L_DEG_TO_RAD);

		float const scale_x = static_cast<float>(luaL_optnumber(L, 5, 1.0));
		float const scale_y = static_cast<float>(luaL_optnumber(L, 6, scale_x));

		BlendMode const blend =
			lua_gettop(L) >= 7 && !lua_isnil(L, 7)
				? TranslateBlendMode(L, 7)
				: BlendMode::MulAlpha;

		auto const a = clamp_u8(luaL_optnumber(L, 8, 255.0));
		auto const r = clamp_u8(luaL_optnumber(L, 9, 255.0));
		auto const g = clamp_u8(luaL_optnumber(L, 10, 255.0));
		auto const b = clamp_u8(luaL_optnumber(L, 11, 255.0));

		float const z = static_cast<float>(luaL_optnumber(L, 12, 0.5));

		// core::Color4B 构造顺序是 r, g, b, a。
		core::Color4B const color(r, g, b, a);

		RenderError re = api_drawSprite(
			img,
			x,
			y,
			rot,
			scale_x * LRESMGR().GetGlobalImageScaleFactor(),
			scale_y * LRESMGR().GetGlobalImageScaleFactor(),
			blend,
			color,
			z
		);

		if (re == RenderError::SpriteNotFound) {
			return luaL_error(L, "can't find sprite '%s'", img);
		}

		return 0;
	}



	static int lib_drawSpriteRect(lua_State* L) {
		validate_render_scope();
		RenderError re = api_drawSpriteRect(
			luaL_checkstring(L, 1),
			(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
			(float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5),
			(float)luaL_optnumber(L, 6, 0.5));
		if (re == RenderError::SpriteNotFound) {
			return luaL_error(L, "can't find sprite '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}
	static int lib_drawSprite4V(lua_State* L) {
		validate_render_scope();
		RenderError re = api_drawSprite4V(
			luaL_checkstring(L, 1),
			(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
			(float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6), (float)luaL_checknumber(L, 7),
			(float)luaL_checknumber(L, 8), (float)luaL_checknumber(L, 9), (float)luaL_checknumber(L, 10),
			(float)luaL_checknumber(L, 11), (float)luaL_checknumber(L, 12), (float)luaL_checknumber(L, 13));
		if (re == RenderError::SpriteNotFound) {
			return luaL_error(L, "can't find sprite '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}

	static int lib_drawSpriteSequence(lua_State* L) {
		validate_render_scope();
		float const hscale = (float)luaL_optnumber(L, 6, 1.0);
		RenderError re = api_drawSpriteSequence(
			luaL_checkstring(L, 1),
			(int)luaL_checkinteger(L, 2),
			(float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
			(float)(luaL_optnumber(L, 5, 0.0) * L_DEG_TO_RAD),
			hscale * LRESMGR().GetGlobalImageScaleFactor(), (float)luaL_optnumber(L, 7, hscale) * LRESMGR().GetGlobalImageScaleFactor(),
			(float)luaL_optnumber(L, 8, 0.5));
		if (re == RenderError::SpriteSequenceNotFound) {
			return luaL_error(L, "can't find animation '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}
	static int lib_drawSpriteSequenceEx(lua_State* L) {
		validate_render_scope();
		// New visual API: (ani, timer, x, y, z, rot=0, hscale=1, vscale=hscale, blend='mul+alpha', color=white)
		float const hscale = static_cast<float>(luaL_optnumber(L, 7, 1.0));
		auto const blend = lua_gettop(L) >= 9 && !lua_isnil(L, 9) ? TranslateBlendMode(L, 9) : BlendMode::MulAlpha;
		auto const color = optionalColor(L, 10);
		RenderError re = api_drawSpriteSequence(
			luaL_checkstring(L, 1),
			static_cast<int>(luaL_checkinteger(L, 2)),
			static_cast<float>(luaL_checknumber(L, 3)),
			static_cast<float>(luaL_checknumber(L, 4)),
			static_cast<float>(luaL_optnumber(L, 6, 0.0) * L_DEG_TO_RAD),
			hscale * LRESMGR().GetGlobalImageScaleFactor(),
			static_cast<float>(luaL_optnumber(L, 8, hscale)) * LRESMGR().GetGlobalImageScaleFactor(),
			blend,
			color,
			static_cast<float>(luaL_checknumber(L, 5)));
		if (re == RenderError::SpriteSequenceNotFound) {
			return luaL_error(L, "can't find animation '%s'", luaL_checkstring(L, 1));
		}
		return 0;
	}

	static int lib_drawTexture(lua_State* L) noexcept {
		validate_render_scope();

		const char* name = luaL_checkstring(L, 1);
		auto const blend = TranslateBlendMode(L, 2);
		core::Graphics::IRenderer::DrawVertex vertex[4];

		for (int i = 0; i < 4; ++i) {
			lua_pushinteger(L, 1);
			lua_gettable(L, 3 + i);
			vertex[i].x = (float)lua_tonumber(L, -1);

			lua_pushinteger(L, 2);
			lua_gettable(L, 3 + i);
			vertex[i].y = (float)lua_tonumber(L, -1);

			lua_pushinteger(L, 3);
			lua_gettable(L, 3 + i);
			vertex[i].z = (float)lua_tonumber(L, -1);

			lua_pushinteger(L, 4);
			lua_gettable(L, 3 + i);
			vertex[i].u = (float)lua_tonumber(L, -1);

			lua_pushinteger(L, 5);
			lua_gettable(L, 3 + i);
			vertex[i].v = (float)lua_tonumber(L, -1);

			lua_pushinteger(L, 6);
			lua_gettable(L, 3 + i);
			if (lua_isnumber(L, -1)) {
				vertex[i].color = (uint32_t)lua_tonumber(L, -1);
			}
			else {
				vertex[i].color = binding::Color::Cast(L, -1)->color();
			}

			lua_pop(L, 6);
		}

		auto* ctx = LR2D();

		translate_blend(ctx, blend);

		core::SmartReference<IResourceTexture> ptex2dres = LRESMGR().FindTexture(name);
		if (!ptex2dres) {
			spdlog::error("[luastg] lstg.Renderer.drawTexture failed: can't find texture '{}'", name);
			return luaL_error(L, "can't find texture '%s'", name);
		}
		check_rendertarget_usage(ptex2dres);
		core::ITexture2D* ptex2d = ptex2dres->GetTexture();
		float const uscale = 1.0f / (float)ptex2d->getSize().x;
		float const vscale = 1.0f / (float)ptex2d->getSize().y;
		for (int i = 0; i < 4; ++i) {
			vertex[i].u *= uscale;
			vertex[i].v *= vscale;
		}
		ctx->setTexture(ptex2d);

		ctx->drawQuad(vertex[0], vertex[1], vertex[2], vertex[3]);

		return 0;
	}

	static int lib_drawModel(lua_State* L) {
		const char* name = luaL_checkstring(L, 1);

		float const x = (float)luaL_checknumber(L, 2);
		float const y = (float)luaL_checknumber(L, 3);
		float const z = (float)luaL_checknumber(L, 4);

		float const roll = (float)(L_DEG_TO_RAD * luaL_optnumber(L, 5, 0.0));
		float const pitch = (float)(L_DEG_TO_RAD * luaL_optnumber(L, 6, 0.0));
		float const yaw = (float)(L_DEG_TO_RAD * luaL_optnumber(L, 7, 0.0));

		float const sx = (float)luaL_optnumber(L, 8, 1.0);
		float const sy = (float)luaL_optnumber(L, 9, 1.0);
		float const sz = (float)luaL_optnumber(L, 10, 1.0);

		core::SmartReference<IResourceModel> pmodres = LRESMGR().FindModel(name);
		if (!pmodres) {
			spdlog::error("[luastg] lstg.Renderer.drawModel failed: can't find model '{}'", name);
			return false;
		}

		pmodres->GetModel()->setScaling(core::Vector3F(sx, sy, sz));
		pmodres->GetModel()->setRotationRollPitchYaw(roll, pitch, yaw);
		pmodres->GetModel()->setPosition(core::Vector3F(x, y, z));
		LR2D()->drawModel(pmodres->GetModel());

		return 0;
	}

#define MKFUNC(X) {#X, &lib_##X}

	static luaL_Reg const lib_func[] = {
		MKFUNC(beginScene),
		MKFUNC(endScene),

		MKFUNC(clearRenderTarget),
		MKFUNC(clearDepthBuffer),

		MKFUNC(setOrtho),
		MKFUNC(setPerspective),

		MKFUNC(setViewport),
		MKFUNC(setScissorRect),

		MKFUNC(setVertexColorBlendState),
		MKFUNC(setFogState),
		MKFUNC(setDepthState),
		MKFUNC(setBlendState),
		MKFUNC(setTexture),

		MKFUNC(drawTriangle),
		MKFUNC(drawQuad),

		MKFUNC(drawSprite),
		MKFUNC(drawSpriteEx),
		{ "Sprite", &lib_Sprite },
		MKFUNC(drawSpriteRect),
		MKFUNC(drawSprite4V),

		MKFUNC(drawSpriteSequence),
		MKFUNC(drawSpriteSequenceEx),

		MKFUNC(drawTexture),

		{ NULL, NULL },
	};

	static int compat_SetViewport(lua_State* L)noexcept {
		core::BoxF box;
		if (lua_gettop(L) >= 6) {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 4),
				(float)luaL_checknumber(L, 5),
				(float)luaL_checknumber(L, 2),
				(float)luaL_checknumber(L, 3),
				(float)luaL_checknumber(L, 6)
			);
		}
		else {
			box = core::BoxF(
				(float)luaL_checknumber(L, 1),
				(float)luaL_checknumber(L, 4),
				0.0f,
				(float)luaL_checknumber(L, 2),
				(float)luaL_checknumber(L, 3),
				1.0f
			);
		}
		core::Vector2U const backbuf_size = LAPP.GetRenderTargetManager()->GetTopRenderTargetSize();
		box.a.y = (float)backbuf_size.y - box.a.y;
		box.b.y = (float)backbuf_size.y - box.b.y;
		LR2D()->setViewport(box);
		return 0;
	}
	static int compat_SetScissorRect(lua_State* L)noexcept {
		core::RectF rect(
			(float)luaL_checknumber(L, 1),
			(float)luaL_checknumber(L, 4),
			(float)luaL_checknumber(L, 2),
			(float)luaL_checknumber(L, 3)
		);
		core::Vector2U const backbuf_size = LAPP.GetRenderTargetManager()->GetTopRenderTargetSize();
		rect.a.y = (float)backbuf_size.y - rect.a.y;
		rect.b.y = (float)backbuf_size.y - rect.b.y;
		LR2D()->setScissorRect(rect);
		return 0;
	}
	static int compat_SetFog(lua_State* L)noexcept {
		int const argc = lua_gettop(L);
		if (argc >= 3) {
			api_setFogState(
				static_cast<float>(luaL_checknumber(L, 1)),
				static_cast<float>(luaL_checknumber(L, 2)),
				*binding::Color::Cast(L, 3)
			);
		}
		else if (argc == 2) {
			api_setFogState(
				static_cast<float>(luaL_checknumber(L, 1)),
				static_cast<float>(luaL_checknumber(L, 2)),
				core::Color4B(0xFF000000)
			);
		}
		else {
			api_setFogState(0.0f, 0.0f, core::Color4B(0x00000000));
		}
		return 0;
	}
	static int compat_SetZBufferEnable(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->setDepthState((core::Graphics::IRenderer::DepthState)luaL_checkinteger(L, 1));
		return 0;
	}
	static int compat_ClearZBuffer(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->clearDepthBuffer((float)luaL_optnumber(L, 1, 1.0));
		return 0;
	}
	static int compat_PushRenderTarget(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->flush();
		if (lua_isstring(L, 1)) {
			core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "rendertarget '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsRenderTarget())
				return luaL_error(L, "'%s' is not a rendertarget.", luaL_checkstring(L, 1));

			if (!LAPP.GetRenderTargetManager()->PushRenderTarget(p.get()))
				return luaL_error(L, "push rendertarget '%s' failed.", luaL_checkstring(L, 1));
		}
		else {
			auto const rt = luastg::binding::RenderTarget::as(L, 1);
			luastg::binding::DepthStencilBuffer* ds{};
			if (lua_isuserdata(L, 2)) {
				ds = luastg::binding::DepthStencilBuffer::as(L, 2);
				if (rt->data->getTexture()->getSize() != ds->data->getSize()) {
					return luaL_error(L, "RenderTarget and DepthStencilBuffer size do not match.");
				}
			}
			core::SmartReference<luastg::IResourceTexture> texture;
			texture.attach(new luastg::RenderTargetStackResourceTextureImpl(rt->data, ds ? ds->data : nullptr));
			if (!LAPP.GetRenderTargetManager()->PushRenderTarget(texture.get())) {
				return luaL_error(L, ds
								  ? "push RenderTarget and DepthStencilBuffer failed."
								  : "push RenderTarget failed.");
			}
		}
		LR2D()->setViewportAndScissorRect();
		return 0;
	}
	static int compat_PopRenderTarget(lua_State* L)noexcept {
		validate_render_scope();
		LR2D()->flush();
		if (!LAPP.GetRenderTargetManager()->PopRenderTarget())
			return luaL_error(L, "pop rendertarget failed.");
		LR2D()->setViewportAndScissorRect();
		return 0;
	}
	static int compat_PostEffect(lua_State* L) {
		validate_render_scope();

		// PostEffectShader 对象风格
		if (lua_isuserdata(L, 1)) {
			auto* p_effect = binding::PostEffectShader::Cast(L, 1);
			const core::Graphics::IRenderer::BlendState blend = translate_blend_3d(TranslateBlendMode(L, 2));
			LR2D()->drawPostEffect(p_effect, blend);
			return 0;
		}

		// 传统风格
		if (lua_type(L, 1) == LUA_TSTRING && lua_type(L, 2) == LUA_TSTRING && lua_type(L, 3) == LUA_TSTRING && lua_type(L, 3) != LUA_TNUMBER) {
			const char* rt_name = luaL_checkstring(L, 1);
			const char* ps_name = luaL_checkstring(L, 2);
			const core::Graphics::IRenderer::BlendState blend = translate_blend_3d(TranslateBlendMode(L, 3));

			core::SmartReference<IResourceTexture> prt = LRES.FindTexture(rt_name);
			if (!prt)
				return luaL_error(L, "texture '%s' not found.", rt_name);
			check_rendertarget_usage(prt);

			core::SmartReference<IResourcePostEffectShader> pfx = LRES.FindFX(ps_name);
			if (!pfx)
				return luaL_error(L, "posteffect '%s' not found.", ps_name);

			core::Graphics::IPostEffectShader* p_effect = pfx->GetPostEffectShader();

			p_effect->setTexture2D("screen_texture", prt->GetTexture());

			auto const rt_size = prt->GetTexture()->getSize();
			p_effect->setFloat4("screen_texture_size", core::Vector4F(float(rt_size.x), float(rt_size.y), 0.0f, 0.0f));

			auto const vp = LR2D()->getViewport();
			p_effect->setFloat4("viewport", core::Vector4F(vp.a.x, vp.a.y, vp.b.x, vp.b.y));

			if (lua_istable(L, 4)) {
				lua_pushnil(L);  // ... t ... nil
				while (0 != lua_next(L, 4)) {
					// ... t ... key value
					const char* key = luaL_checkstring(L, -2);
					if (lua_isnumber(L, -1)) {
						p_effect->setFloat(key, (float)lua_tonumber(L, -1));
					}
					else if (lua_isstring(L, -1)) {
						core::SmartReference<IResourceTexture> ptex = LRES.FindTexture(lua_tostring(L, -1));
						if (!ptex)
							return luaL_error(L, "texture '%s' not found.", rt_name);
						check_rendertarget_usage(ptex);
						p_effect->setTexture2D(key, ptex->GetTexture());
					}
					else if (binding::Vector2::is(L, -1)) {
						auto const p_value =  binding::Vector2::as(L, -1);
						p_effect->setFloat2(key, core::Vector2F(
							static_cast<float>(p_value->data.x),
							static_cast<float>(p_value->data.y)
						));
					}
					else if (binding::Vector3::is(L, -1)) {
						auto const p_value = binding::Vector3::as(L, -1);
						p_effect->setFloat3(key, core::Vector3F(
							static_cast<float>(p_value->data.x),
							static_cast<float>(p_value->data.y),
							static_cast<float>(p_value->data.z)
						));
					}
					else if (binding::Vector4::is(L, -1)) {
						auto const p_value = binding::Vector4::as(L, -1);
						p_effect->setFloat4(key, core::Vector4F(
							static_cast<float>(p_value->data.x),
							static_cast<float>(p_value->data.y),
							static_cast<float>(p_value->data.z),
							static_cast<float>(p_value->data.w)
						));
					}
					else if (lua_isuserdata(L, -1)) {
						auto const color = *binding::Color::Cast(L, -1);
						p_effect->setFloat4(key, core::Vector4F(
							static_cast<float>(color.r) / 255.0f,
							static_cast<float>(color.g) / 255.0f,
							static_cast<float>(color.b) / 255.0f,
							static_cast<float>(color.a) / 255.0f
						));
					}
					else {
						return luaL_error(L, "PostEffect: invalid data type.");
					}
					lua_pop(L, 1);  // ... t ... key
				}
			}

			LR2D()->drawPostEffect(pfx->GetPostEffectShader(), blend);

			return 0;
		}

		// 下面是一是脑抽设计出来的，以后必须干掉

		const char* ps_name = luaL_checkstring(L, 1);
		const char* rt_name = luaL_checkstring(L, 2);
		const core::Graphics::IRenderer::SamplerState rtsv = (core::Graphics::IRenderer::SamplerState)luaL_checkinteger(L, 3);
		const core::Graphics::IRenderer::BlendState blend = translate_blend_3d(TranslateBlendMode(L, 4));

		core::SmartReference<IResourcePostEffectShader> pfx = LRES.FindFX(ps_name);
		if (!pfx)
			return luaL_error(L, "posteffect '%s' not found.", ps_name);

		core::SmartReference<IResourceTexture> prt = LRES.FindTexture(rt_name);
		if (!prt)
			return luaL_error(L, "texture '%s' not found.", rt_name);
		check_rendertarget_usage(prt);

		core::Vector4F cbdata[8] = {};
		core::ITexture2D* tdata[4] = {};
		core::Graphics::IRenderer::SamplerState tsdata[4] = {};

		size_t cbdata_n = lua_objlen(L, 5);
		cbdata_n = (cbdata_n <= 8) ? cbdata_n : 8;
		for (int i = 1; i <= (int)cbdata_n; i += 1) {
			lua_rawgeti(L, 5, i);  // ??? t
			luaL_argcheck(L, lua_istable(L, -1), 5, "shader constant values must be an array of lua table, each table contains 4 lua numbers");
			lua_rawgeti(L, -1, 1); // ??? t f1
			lua_rawgeti(L, -2, 2); // ??? t f1 f2
			lua_rawgeti(L, -3, 3); // ??? t f1 f2 f3
			lua_rawgeti(L, -4, 4); // ??? t f1 f2 f3 f4
			cbdata[i - 1].x = (float)luaL_checknumber(L, -4);
			cbdata[i - 1].y = (float)luaL_checknumber(L, -3);
			cbdata[i - 1].z = (float)luaL_checknumber(L, -2);
			cbdata[i - 1].w = (float)luaL_checknumber(L, -1);
			lua_pop(L, 5);
		}
		size_t tdata_n = lua_objlen(L, 6);
		tdata_n = (tdata_n <= 8) ? tdata_n : 4;
		for (int i = 1; i <= (int)tdata_n; i += 1) {
			lua_rawgeti(L, 6, i);  // ??? t
			luaL_argcheck(L, lua_istable(L, -1), 6, "shader resources must be an array of lua table, each table contains the name of texture and sampler type");
			lua_rawgeti(L, -1, 1); // ??? t tex
			lua_rawgeti(L, -2, 2); // ??? t tex sampler
			const char* tx_name = luaL_checkstring(L, -2);
			core::SmartReference<IResourceTexture> ptex = LRES.FindTexture(tx_name);
			if (!ptex)
				return luaL_error(L, "texture '%s' not found.", tx_name);
			check_rendertarget_usage(ptex);
			tdata[i - 1] = ptex->GetTexture();
			tsdata[i - 1] = (core::Graphics::IRenderer::SamplerState)luaL_checkinteger(L, -1);
		}

		LR2D()->drawPostEffect(pfx->GetPostEffectShader(), blend, prt->GetTexture(), rtsv, cbdata, cbdata_n, tdata, tsdata, tdata_n);

		return 0;
	}

	static luaL_Reg const lib_compat[] = {
		{ "BeginScene", &lib_beginScene },
		{ "EndScene", &lib_endScene },
		{ "RenderClear", &lib_clearRenderTarget },
		{ "SetViewport", &compat_SetViewport },
		{ "SetScissorRect", &compat_SetScissorRect },
		{ "SetOrtho", &lib_setOrtho },
		{ "SetPerspective", &lib_setPerspective },
		{ "Render", &lib_drawSprite },
		{ "RenderEx", &lib_drawSpriteEx },
		{ "RenderRect", &lib_drawSpriteRect },
		{ "Render4V", &lib_drawSprite4V },
		{ "RenderAnimation", &lib_drawSpriteSequence },
		{ "RenderAnimationEx", &lib_drawSpriteSequenceEx },
		{ "RenderTexture", &lib_drawTexture },
		{ "RenderModel", &lib_drawModel },
		{ "SetFog", &compat_SetFog },
		{ "SetZBufferEnable", &compat_SetZBufferEnable },
		{ "ClearZBuffer", &compat_ClearZBuffer },
		{ "PushRenderTarget", &compat_PushRenderTarget },
		{ "PopRenderTarget", &compat_PopRenderTarget },
		{ "PostEffect", &compat_PostEffect },
		{ NULL, NULL },
	};
}

void luastg::binding::Renderer::Register(lua_State* L)noexcept {
	luaL_register(L, LUASTG_LUA_LIBNAME, lib_compat);           // ... lstg
	luaL_register(L, LUASTG_LUA_LIBNAME ".Renderer", lib_func); // ... lstg lstg.Renderer

	register_sprite_component(L);                              // ... lstg lstg.Renderer

	lua_setfield(L, -1, "Renderer");                            // ... lstg
	lua_pop(L, 1);                                              // ...
}
