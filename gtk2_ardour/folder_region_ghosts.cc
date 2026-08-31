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

#include "ardour/route.h"
#include "ardour/track_folder.h"

#include "folder_region_ghosts.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "public_editor.h"
#include "region_view.h"
#include "route_time_axis.h"
#include "selection.h"
#include "streamview.h"
#include "time_axis_view.h"

#include "canvas/rectangle.h"

using namespace ARDOUR;
using namespace std;

FolderRegionGhosts::FolderRegionGhosts (PublicEditor& ed, TimeAxisView& target)
	: _editor (ed)
	, _target (target)
{
	RegionView::RegionViewGoingAway.connect (_going_away_connection, invalidator (*this), std::bind (&FolderRegionGhosts::region_view_going_away, this, _1), gui_context ());
}

FolderRegionGhosts::~FolderRegionGhosts ()
{
	set_folder (std::shared_ptr<TrackFolder> ());
}

void
FolderRegionGhosts::set_folder (std::shared_ptr<TrackFolder> folder)
{
	if (_folder == folder) {
		return;
	}

	_folder_connections.drop_connections ();
	_folder = folder;

	if (!_folder) {
		update ();
		return;
	}

	_folder->PropertyChanged.connect (_folder_connections, invalidator (*this), std::bind (&FolderRegionGhosts::folder_property_changed, this, _1), gui_context ());
	_folder->RouteAdded.connect (_folder_connections, invalidator (*this), std::bind (&FolderRegionGhosts::folder_membership_changed, this, _1, _2), gui_context ());
	_folder->RouteRemoved.connect (_folder_connections, invalidator (*this), std::bind (&FolderRegionGhosts::folder_membership_changed, this, _1, _2), gui_context ());

	update ();
}

void
FolderRegionGhosts::update ()
{
	if (_folder && _folder->collapsed ()) {
		TrackFolder::RouteVector const& routes (_folder->route_list ());
		for (TrackFolder::RouteVector::const_iterator i = routes.begin(); i != routes.end(); ++i) {
			ghost_route_regions (*i);
		}
		return;
	}

	for (std::map<std::shared_ptr<Route>, sigc::connection>::iterator i = _region_added_connections.begin(); i != _region_added_connections.end(); ++i) {
		i->second.disconnect ();
	}
	_region_added_connections.clear ();

	for (std::set<RegionView*>::iterator i = _ghosted_regions.begin(); i != _ghosted_regions.end(); ++i) {
		(*i)->remove_ghost_in (_target);
	}
	_ghosted_regions.clear ();
}

void
FolderRegionGhosts::folder_property_changed (PBD::PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::folder_collapsed)) {
		update ();
	}
}

void
FolderRegionGhosts::folder_membership_changed (std::shared_ptr<TrackFolder>, std::weak_ptr<Route> wr)
{
	if (!_folder || !_folder->collapsed ()) {
		return;
	}

	std::shared_ptr<Route> r = wr.lock ();
	if (!r) {
		return;
	}

	if (_folder->contains (r)) {
		ghost_route_regions (r);
	} else {
		unghost_route_regions (r);
	}
}

void
FolderRegionGhosts::ghost_route_regions (std::shared_ptr<Route> route)
{
	if (!route || _region_added_connections.find (route) != _region_added_connections.end ()) {
		/* already wired up for this route */
		return;
	}

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (_editor.time_axis_view_from_stripable (route));
	if (!rtv || !rtv->view ()) {
		return;
	}

	rtv->view ()->foreach_regionview (sigc::mem_fun (*this, &FolderRegionGhosts::add_region_ghost));
	_region_added_connections[route] = rtv->view ()->RegionViewAdded.connect (sigc::mem_fun (*this, &FolderRegionGhosts::add_region_ghost));
}

void
FolderRegionGhosts::unghost_route_regions (std::shared_ptr<Route> route)
{
	if (!route) {
		return;
	}

	std::map<std::shared_ptr<Route>, sigc::connection>::iterator ci = _region_added_connections.find (route);
	if (ci == _region_added_connections.end ()) {
		/* not currently ghosted */
		return;
	}

	ci->second.disconnect ();
	_region_added_connections.erase (ci);

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (_editor.time_axis_view_from_stripable (route));
	if (!rtv || !rtv->view ()) {
		return;
	}

	for (std::set<RegionView*>::iterator i = _ghosted_regions.begin(); i != _ghosted_regions.end(); ) {
		RegionView* rv = *i;
		++i;
		if (&rv->get_time_axis_view () == rtv) {
			remove_region_ghost (rv);
		}
	}
}

void
FolderRegionGhosts::add_region_ghost (RegionView* rv)
{
	if (!rv || _ghosted_regions.find (rv) != _ghosted_regions.end ()) {
		return;
	}

	GhostRegion* gr = rv->add_ghost (_target);
	if (!gr) {
		return;
	}

	style_ghost_region (gr);
	_ghosted_regions.insert (rv);

	gr->base_rect->set_data ("regionview", rv);
	gr->base_rect->Event.connect (sigc::bind (sigc::mem_fun (*this, &FolderRegionGhosts::ghost_region_event), rv));
}

void
FolderRegionGhosts::remove_region_ghost (RegionView* rv)
{
	if (!rv) {
		return;
	}
	rv->remove_ghost_in (_target);
	_ghosted_regions.erase (rv);
}

void
FolderRegionGhosts::style_ghost_region (GhostRegion* gr)
{
	gr->base_rect->set_x0 (0);
	gr->base_rect->set_y0 (1.0);
	gr->base_rect->set_y1 (_target.current_height ());
	gr->base_rect->set_outline (false);
	gr->base_rect->set_fill_color (gr->source_track_color (120));
}

void
FolderRegionGhosts::region_view_going_away (RegionView* rv)
{
	/* the ghost itself is already gone (RegionView's destructor deletes
	 * all of its ghosts); just drop our now-dangling reference.
	 */
	_ghosted_regions.erase (rv);
}

bool
FolderRegionGhosts::ghost_region_event (GdkEvent* ev, RegionView* rv)
{
	if (ev->type != GDK_BUTTON_PRESS || ev->button.button != 1) {
		return false;
	}

	Selection& s = _editor.get_selection ();

	if (ev->button.state & GDK_CONTROL_MASK) {
		s.toggle (rv);
	} else if (ev->button.state & GDK_SHIFT_MASK) {
		s.add (rv);
	} else {
		s.set (rv);
	}

	return true;
}
