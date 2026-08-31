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

#include <map>
#include <memory>
#include <set>

#include <ydk/gdk.h>

#include "pbd/signals.h"

namespace ARDOUR {
	class Route;
	class TrackFolder;
}

class GhostRegion;
class PublicEditor;
class RegionView;
class TimeAxisView;

/** Shows "ghost" copies of a TrackFolder's member routes' regions on a
 *  single TimeAxisView row while the folder is collapsed.
 *
 *  Used by both FolderTimeAxisView (the synthetic header row for a
 *  bus-less folder) and, for a folder that has been given a submix bus,
 *  the bus's own RouteTimeAxisView -- in that case the bus row takes over
 *  as the folder's on-canvas identity, so it is the one that should show
 *  the other members' regions while collapsed.
 */
class FolderRegionGhosts : public sigc::trackable
{
public:
	/** @param target the TimeAxisView that ghost regions should be drawn on */
	FolderRegionGhosts (PublicEditor&, TimeAxisView& target);
	~FolderRegionGhosts ();

	/** Start tracking @p folder's membership/collapsed state, ghosting its
	 *  members' regions onto the target view as appropriate.  Pass a null
	 *  folder to stop and drop any existing ghosts.
	 */
	void set_folder (std::shared_ptr<ARDOUR::TrackFolder>);

private:
	PublicEditor&                        _editor;
	TimeAxisView&                        _target;
	std::shared_ptr<ARDOUR::TrackFolder> _folder;
	PBD::ScopedConnection                 _going_away_connection;
	PBD::ScopedConnectionList             _folder_connections;

	std::set<RegionView*>                                     _ghosted_regions;
	std::map<std::shared_ptr<ARDOUR::Route>, sigc::connection> _region_added_connections;

	void update ();
	void folder_property_changed (PBD::PropertyChange const&);
	void folder_membership_changed (std::shared_ptr<ARDOUR::TrackFolder>, std::weak_ptr<ARDOUR::Route>);
	void ghost_route_regions (std::shared_ptr<ARDOUR::Route>);
	void unghost_route_regions (std::shared_ptr<ARDOUR::Route>);
	void add_region_ghost (RegionView*);
	void remove_region_ghost (RegionView*);
	void style_ghost_region (GhostRegion*);
	void region_view_going_away (RegionView*);
	bool ghost_region_event (GdkEvent*, RegionView*);
};
