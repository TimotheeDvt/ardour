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

#include "widgets/ardour_button.h"

#include "time_axis_view.h"

namespace ArdourCanvas {
	class Canvas;
}

namespace ARDOUR {
	class Route;
	class Session;
	class TrackFolder;
}

/** A TimeAxisView representing a collapsible group ("folder") of Routes in
 *  the Editor.
 *
 *  Unlike RouteTimeAxisView / VCATimeAxisView, this is *not* backed by a
 *  Stripable -- it is a purely Editor-local display grouping (the
 *  underlying data is ARDOUR::TrackFolder, which implies no functional
 *  linking between members, and is not part of Session::get_stripables()).
 *  stripable() always returns null; any code that walks
 *  Editor::track_views must not assume every entry is Stripable-backed.
 */
class FolderTimeAxisView : public TimeAxisView
{
public:
	FolderTimeAxisView (PublicEditor&, ARDOUR::Session*, ArdourCanvas::Canvas&, std::shared_ptr<ARDOUR::TrackFolder>);
	~FolderTimeAxisView ();

	std::string name () const;
	Gdk::Color color () const;
	std::shared_ptr<ARDOUR::Stripable> stripable () const { return std::shared_ptr<ARDOUR::Stripable> (); }
	std::string state_id () const;

	std::shared_ptr<ARDOUR::TrackFolder> folder () const { return _folder; }

	/** The member route this folder's header should be displayed directly
	 *  above (TrackFolder::topmost_route()). Null if the folder is empty.
	 */
	std::shared_ptr<ARDOUR::Route> anchor_route () const;

	void toggle_collapsed ();

protected:
	void build_display_menu ();
	bool name_entry_changed (std::string const&);

private:
	std::shared_ptr<ARDOUR::TrackFolder> _folder;
	ArdourWidgets::ArdourButton          _collapse_button;
	PBD::ScopedConnectionList             _folder_connections;

	bool collapse_button_release (GdkEventButton*);
	void folder_property_changed (PBD::PropertyChange const&);
	void folder_membership_changed (std::shared_ptr<ARDOUR::TrackFolder>, std::weak_ptr<ARDOUR::Route>);
	void update_collapse_button ();
	void update_member_count_tooltip ();
	void update_color ();
	void ungroup ();
	void choose_color ();
};
