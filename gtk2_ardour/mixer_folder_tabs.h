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

#pragma once

#include <list>
#include <memory>

#include "gtkmm2ext/cairo_widget.h"

#include "ardour/session_handle.h"

namespace ARDOUR {
	class TrackFolder;
}

class Mixer_UI;

/** A thin strip of colored, labeled tabs drawn above the Mixer's strip
 *  packer, one per bus-less TrackFolder, spanning that folder's consecutive
 *  visible member strips. Clicking a tab toggles that folder's collapsed
 *  state (shared with the Editor -- TrackFolder::collapsed is one flag,
 *  not a per-window one).
 *
 *  Modeled on MixerGroupTabs/GroupTabs (which do the same thing for
 *  RouteGroups) but standalone rather than sharing that base class: GroupTabs
 *  is tightly coupled to RouteGroup throughout (Tab::group is a RouteGroup
 *  weak_ptr, and most of its virtual surface -- dragging to create new
 *  groups, VCA/master assignment, etc. -- has no TrackFolder equivalent).
 *
 *  Folders that already have a bus are NOT shown here: their collapse
 *  control lives directly on the bus's own MixerStrip instead (see
 *  MixerStrip::set_folder()), since the bus is a real, already-visible
 *  strip.
 */
class MixerFolderTabs : public CairoWidget, public ARDOUR::SessionHandlePtr
{
public:
	MixerFolderTabs (Mixer_UI*);

private:
	struct Tab {
		Tab () : from (0), to (0), color (0) {}

		double from;
		double to;
		uint32_t color;
		std::weak_ptr<ARDOUR::TrackFolder> folder;
	};

	Mixer_UI* _mixer;

	std::list<Tab> compute_tabs () const;
	Tab const* click_to_tab (double x, std::list<Tab> const&) const;

	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void on_size_request (Gtk::Requisition*);
	bool on_button_press_event (GdkEventButton*);
};
