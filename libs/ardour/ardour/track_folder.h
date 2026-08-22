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

#include <memory>
#include <vector>

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/session_object.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> folder_collapsed;
}

class Route;
class Session;

/** A named, ordered collection of Routes that can be collapsed as a unit
 *  in the Editor.  Unlike RouteGroup, a TrackFolder implies no functional
 *  (gain/mute/solo) linking between its members -- it is purely a display
 *  grouping.  Membership order is significant, since it determines where
 *  the folder's header appears among its members in the Editor.
 */
class LIBARDOUR_API TrackFolder : public SessionObject, public std::enable_shared_from_this<TrackFolder>
{
public:
	static void make_property_quarks ();

	typedef std::vector<std::shared_ptr<Route> > RouteVector;

	TrackFolder (Session&, const std::string& name);
	~TrackFolder ();

	bool collapsed () const { return _collapsed.val (); }
	void set_collapsed (bool yn);

	uint32_t color () const { return _color.val (); }
	void set_color (uint32_t rgba);

	/** Add a route to the folder.  Adding a route already present is a no-op. */
	int add_route (std::shared_ptr<Route>);
	int remove_route (std::shared_ptr<Route>);
	void clear ();

	bool empty () const { return _routes.empty (); }
	size_t size () const { return _routes.size (); }
	bool contains (std::shared_ptr<Route>) const;

	RouteVector const& route_list () const { return _routes; }

	/** The member route currently sorted first by PresentationInfo order,
	 *  i.e. the route the folder header should be displayed directly above.
	 *  Null if the folder has no members.
	 */
	std::shared_ptr<Route> topmost_route () const;

	/** Emitted when a route has been added to this folder */
	PBD::Signal<void(std::shared_ptr<TrackFolder>, std::weak_ptr<ARDOUR::Route>)> RouteAdded;
	/** Emitted when a route has been removed from this folder */
	PBD::Signal<void(std::shared_ptr<TrackFolder>, std::weak_ptr<ARDOUR::Route>)> RouteRemoved;

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);

protected:
	friend class Session;

private:
	RouteVector             _routes;
	PBD::Property<bool>     _collapsed;
	PBD::Property<uint32_t> _color;

	void remove_when_going_away (std::weak_ptr<Route>);
};

} /* namespace */
