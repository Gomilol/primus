#pragma once
#include "helpers.h"

// modelled after the original valve 'developer 1' debug console
// https://github.com/LestaD/SourceEngine2007/blob/master/se2007/engine/console.cpp

class NotifyText {
public:
	std::string m_text;
	Color		m_color;
	float		m_time;

public:
	__forceinline NotifyText(const std::string& text, Color color, float time) : m_text{ text }, m_color{ color }, m_time{ time } {}
};

class Notify {
private:
	std::vector< std::shared_ptr< NotifyText > > m_notify_text;

public:
	__forceinline Notify() : m_notify_text{} {}

	__forceinline void add(const std::string& text, Color color = colors::white, float time = 8.f, bool console = true) {
		// modelled after 'CConPanel::AddToNotify'
		m_notify_text.push_back(std::make_shared< NotifyText >(text, color, time));

		if (console)
			g_cl.print(text);
	}

	// modelled after 'CConPanel::DrawNotify' and 'CConPanel::ShouldDraw'
	void think() {
		int		x{ 8 }, y{ 5 }, size{ render::menu_shade.m_size.m_height + 1 };
		Color	color;
		float	left, delta;

		// update lifetimes.
		for (size_t i{}; i < m_notify_text.size(); ++i) {
			auto notify = m_notify_text[i];

			notify->m_time -= g_csgo.m_globals->m_frametime;

			if (notify->m_time <= 0.f) {
				m_notify_text.erase(m_notify_text.begin() + i);
				continue;
			}
		}

		// we have nothing to draw.
		if (m_notify_text.empty())
			return;

		// iterate entries.
		for (size_t i{}; i < m_notify_text.size(); ++i) {
			auto notify = m_notify_text[i];

			/* erease unstable caracthers */
			if (notify->m_text.find(XOR("%%%%")) != std::string::npos)
				notify->m_text.erase(notify->m_text.find(XOR("%")), 3);

			left = notify->m_time;
			color = notify->m_color;
			delta = fabs(g_csgo.m_globals->m_frametime - left);

			render::FontSize_t text_size = render::menu_shade.size(notify->m_text);
			render::FontSize_t prefix_size = render::menu_shade.size("[ supremacy ] ");

			if (left < .5f) {
				float f = left;
				math::clamp(f, 0.f, .5f);

				f /= .5f;

				color.a() = (int)(f * 255.f);

				if (i == 0 && f < 0.2f)
					y -= size * (1.f - f / 0.2f);
			}
			else {
				color.a() = 255;

				if (delta > 7.65f) {
					x = 8 + (((left - (7.65f)) / 0.25f) * static_cast<float>(-(text_size.m_width + prefix_size.m_width)));
				}
			}

			render::menu_shade.string(x, y, c_help::notify.alpha(color.a()), "[ supremacy ] ");
			render::menu_shade.string(x + prefix_size.m_width, y, color, notify->m_text);
			y += size;
		}
	}
};

extern Notify g_notify;