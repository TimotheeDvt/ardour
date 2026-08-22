/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/memento_command.h"

#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track_folder.h"
#include "ardour/track_folder_list.h"

#include "gtkmm2ext/colors.h"

#include "axis_view.h"
#include "mixer_folder_tabs.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

#define BASELINESTRETCH (1.25)

MixerFolderTabs::MixerFolderTabs (Mixer_UI* m)
	: _mixer (m)
{
	add_events (Gdk::BUTTON_PRESS_MASK);
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &MixerFolderTabs::queue_draw));
}

list<MixerFolderTabs::Tab>
MixerFolderTabs::compute_tabs () const
{
	list<Tab> tabs;

	if (!_session) {
		return tabs;
	}

	Tab tab;
	tab.from = 0;

	int32_t x = 0;
	TreeModel::Children rows = _mixer->track_model->children ();
	for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {

		AxisView* av = (*i)[_mixer->stripable_columns.strip];
		MixerStrip* s = dynamic_cast<MixerStrip*> (av);

		if (!s) {
			continue;
		}

		if (s->route ()->is_main_bus () || !s->marked_for_display ()) {
			continue;
		}

		if (_mixer->folded_under_collapsed_folder (s->route ())) {
			continue;
		}

		std::shared_ptr<TrackFolder> f = _session->track_folders ()->folder_for_route (s->route ());
		if (!f) {
			f = _session->track_folders ()->folder_for_bus (s->route ());
		}
		std::shared_ptr<TrackFolder> tab_folder = tab.folder.lock ();

		if (f != tab_folder) {
			if (tab_folder) {
				tab.to = x;
				tabs.push_back (tab);
			}

			tab.from = x;
			tab.folder = f;

			if (f) {
				tab.color = f->color ();
			}
		}

		int ww = 0, wh = 0;
		s->get_size_request (ww, wh); // widget may not be realized, get_width() is invalid.
		x += ww;
	}

	if (tab.folder.lock ()) {
		tab.to = x;
		tabs.push_back (tab);
	}

	return tabs;
}

MixerFolderTabs::Tab const*
MixerFolderTabs::click_to_tab (double x, list<Tab> const& tabs) const
{
	for (list<Tab>::const_iterator i = tabs.begin (); i != tabs.end (); ++i) {
		if (x >= i->from && x < i->to) {
			return &(*i);
		}
	}

	return 0;
}

void
MixerFolderTabs::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj ();

	uint32_t bg_color = UIConfiguration::instance ().color ("group tab base");
	Gtkmm2ext::set_source_rgb_a (cr, bg_color, 1.0);
	cairo_rectangle (cr, 0, 0, get_width (), get_height ());
	cairo_fill (cr);

	list<Tab> tabs = compute_tabs ();

	for (list<Tab>::const_iterator i = tabs.begin (); i != tabs.end (); ++i) {

		std::shared_ptr<TrackFolder> f = i->folder.lock ();
		if (!f) {
			continue;
		}

		double const arc_radius = get_height ();
		double r, g, b, a;
		Gtkmm2ext::color_to_rgba (i->color, r, g, b, a);
		a = 1.0;

		cairo_set_source_rgba (cr, r, g, b, a);
		cairo_arc (cr, i->from + arc_radius, get_height (), arc_radius, M_PI, 3 * M_PI / 2);
		cairo_line_to (cr, i->to - arc_radius, 0);
		cairo_arc (cr, i->to - arc_radius, get_height (), arc_radius, 3 * M_PI / 2, 2 * M_PI);
		cairo_line_to (cr, i->from, get_height ());
		cairo_fill (cr);

		if ((i->to - i->from) > arc_radius) {
			int text_width, text_height;

			Glib::RefPtr<Pango::Layout> layout;
			layout = Pango::Layout::create (get_pango_context ());
			layout->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);

			string label = (f->collapsed () ? S_("Folder|>") : S_("Folder|v")) + string (" ") + f->name ();
			layout->set_text (label);
			layout->set_width ((i->to - i->from - arc_radius) * PANGO_SCALE);
			layout->get_pixel_size (text_width, text_height);

			cairo_move_to (cr, i->from + (i->to - i->from - text_width) * .5, (get_height () - text_height) * .5);

			uint32_t text_color = Gtkmm2ext::contrasting_text_color (i->color);
			Gtkmm2ext::color_to_rgba (text_color, r, g, b, a);
			cairo_set_source_rgb (cr, r, g, b);

			pango_cairo_show_layout (cr, layout->gobj ());
		}
	}
}

void
MixerFolderTabs::on_size_request (Gtk::Requisition* req)
{
	Glib::RefPtr<Pango::Layout> layout;
	layout = Pango::Layout::create (get_pango_context ());
	layout->set_text (X_("Tab Text"));
	int tw, th;
	layout->get_pixel_size (tw, th);

	int size = (int) ceil (th * BASELINESTRETCH + 1.0);

	req->width = size;
	req->height = size;
}

bool
MixerFolderTabs::on_button_press_event (GdkEventButton* ev)
{
	if (!_session || ev->button != 1) {
		return false;
	}

	list<Tab> tabs = compute_tabs ();
	Tab const* t = click_to_tab (ev->x, tabs);

	if (!t) {
		return true;
	}

	std::shared_ptr<TrackFolder> f = t->folder.lock ();
	if (!f) {
		return true;
	}

	XMLNode& before (f->get_state ());
	f->set_collapsed (!f->collapsed ());
	XMLNode& after (f->get_state ());

	_session->begin_reversible_command (f->collapsed () ? _("Collapse Folder") : _("Expand Folder"));
	_session->commit_reversible_command (new MementoCommand<TrackFolder> (*f, &before, &after));

	return true;
}
