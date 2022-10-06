#include "includes.h"

void DrawPlate(std::string text, std::string sub_text, int x, int y, bool centered, int barpos, float barpercent, Color barclr, int alpha) {
	int dst = false;
	float a = alpha / 255.f;
	int bar_size = 8;
	render::FontSize_t txts = render::hudster.size(text);
	bar_size += txts.m_width;

	if (sub_text != "") {
		dst = true;
		render::FontSize_t mini_txts = render::hudster.size(sub_text);
		bar_size += 2 + mini_txts.m_width;
	}

	int x_pos = x;
	if (centered)
		x_pos = floor(x - (bar_size / 2));

	render::rect_filled(x_pos, y, bar_size, 16, Color(0, 0, 0, 150 * a));
	render::hudster.string(x_pos + 4, y + 2, Color(255, 255, 255, 255 * a), text);

	if (dst)
		render::hudster.string(x_pos + txts.m_width + 6, y + 2, Color(200, 200, 200, 255 * a), sub_text);

	if (barpos != 0) {
		if (barpos == 1) {
			render::rect_filled(x_pos, y + 16, bar_size, 1, Color(0, 0, 0, 255 * a));
			render::rect_filled(x_pos + (bar_size / 2 - barpercent / 2 * bar_size), y + 16, barpercent * bar_size, 1, Color(barclr.r(), barclr.g(), barclr.b(), 255 * a));
		}
		else if (barpos == 2) {
			render::rect_filled(x_pos, y - 1, bar_size, 1, Color(0, 0, 0, 255 * a));
			render::rect_filled(x_pos + (bar_size / 2 - barpercent / 2 * bar_size), y - 1, barpercent * bar_size, 1, Color(barclr.r(), barclr.g(), barclr.b(), 255 * a));
		}
	}

}

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 3)
{
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a_value;
	return out.str();
}

void Visuals::DrawProjectile(Weapon* ent) {
	vec2_t screen;
	vec3_t origin = ent->GetAbsOrigin();
	if (!render::WorldToScreen(origin, screen))
		return;

	Color col = Color(255, 255, 255);
	auto moly_color = Color(255, 0, 0);
	auto smoke_color = Color(58, 214, 252);
	col.alpha(255);

	Color col_safe_icon = Color(255, 255, 255, 255);
	Color col_lethal_icon = Color(255, 255, 255, 255);
	int dist = g_cl.m_local->m_vecOrigin().dist_to(origin) / 12;

	// draw molotov.
	if (ent->is(HASH("CMolotovProjectile"))) {
		//render::circle(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 120));
	//	render::circle_outline(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 255));
		render::grenade.string(screen.x - 9, screen.y - 23, { 255,255,255,255 }, "l", render::ALIGN_LEFT);
	}

	else if (ent->is(HASH("CBaseCSGrenadeProjectile"))) {
		const model_t* model = ent->GetModel();

		if (model) {
			// grab modelname.
			std::string name{ ent->GetModel()->m_name };

			if (name.find(XOR("flashbang")) != std::string::npos) {

				//render::circle(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 120));
				//render::circle_outline(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 255));
				render::esp_small.string(screen.x - 9, screen.y - 23, { 255,255,255,255 }, "molotov", render::ALIGN_LEFT);


			}

			else if (name.find(XOR("fraggrenade")) != std::string::npos) {


				//render::circle(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 120));
				//render::circle_outline(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 255));
				render::esp_small.string(screen.x - 9, screen.y - 23, { 255,255,255,255 }, "he grenade", render::ALIGN_LEFT);

			}
		}
	}

	// find classes.
	else if (ent->is(HASH("CInferno"))) {
		const double spawn_time = *(float*)(uintptr_t(ent) + 0x20);
		const double reltime = ((spawn_time + 7.031) - g_csgo.m_globals->m_curtime);
		const double factor = reltime / 7.031;


		// pandorel
		if (dist <= 85) { // we render the circle like a boss
			float radius = 144.f;
			render::WorldCircleOutline(origin, radius, 1.f, moly_color);
		}

		//render::circle(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 120));
		//render::circle_outline(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 255));
		render::esp_small.string(screen.x - 9, screen.y - 23, { 255,255,255,255 }, "molotov", render::ALIGN_LEFT);


	}
	 // commented out due to a very odd crash that shouldn't appear.
	/*else if (ent->is(HASH("CSmokeGrenadeProjectile"))) {
		Weapon* pSmokeEffect = reinterpret_cast<Weapon*>(ent);
		const float spawn_time = game::TICKS_TO_TIME(pSmokeEffect->m_nSmokeEffectTickBegin());
		const double reltime = ((spawn_time + 17.441) - g_csgo.m_globals->m_curtime);
		const double factor = reltime / 17.441;



		if (spawn_time > 0.f) { // l3d saves me
			if (dist <= 85) { // we render the circle like a  boss
				float radius = 144.f;
				render::WorldCircleOutline(origin, radius, 1.f, smoke_color);
			}

			//render::circle(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 120));
			//render::circle_outline(screen.x, screen.y - 10, 20, 360, Color(0, 0, 0, 255));
			render::esp_small.string(screen.x - 5, screen.y - 23, { 255,255,255,255 }, "smoke", render::ALIGN_LEFT);
		}

	}*/
}
