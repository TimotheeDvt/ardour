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

#include <algorithm>
#include <sstream>

#include "pbd/strsplit.h"
#include "pbd/types_convert.h"

#include "ardour/presentation_info.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/track_folder.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

namespace ARDOUR {
	namespace Properties {
		PropertyDescriptor<bool> folder_collapsed;
	}
}

void
TrackFolder::make_property_quarks ()
{
	Properties::folder_collapsed.property_id = g_quark_from_static_string (X_("collapsed"));
}

TrackFolder::TrackFolder (Session& s, const string& n)
	: SessionObject (s, n)
	, _collapsed (Properties::folder_collapsed, false)
	, _color (Properties::color, 0x707070ff)
{
	_xml_node_name = X_("TrackFolder");
	add_property (_collapsed);
	add_property (_color);
}

TrackFolder::~TrackFolder ()
{
	_routes.clear ();
}

int
TrackFolder::add_route (std::shared_ptr<Route> r)
{
	if (!r || contains (r)) {
		return 0;
	}

	_routes.push_back (r);
	r->DropReferences.connect_same_thread (*this, std::bind (&TrackFolder::remove_when_going_away, this, std::weak_ptr<Route> (r)));

	_session.set_dirty ();
	RouteAdded (shared_from_this (), std::weak_ptr<Route> (r)); /* EMIT SIGNAL */
	return 0;
}

int
TrackFolder::remove_route (std::shared_ptr<Route> r)
{
	RouteVector::iterator i = find (_routes.begin (), _routes.end (), r);

	if (i == _routes.end ()) {
		return -1;
	}

	_routes.erase (i);
	_session.set_dirty ();
	RouteRemoved (shared_from_this (), std::weak_ptr<Route> (r)); /* EMIT SIGNAL */
	return 0;
}

void
TrackFolder::remove_when_going_away (std::weak_ptr<Route> wr)
{
	std::shared_ptr<Route> r (wr.lock ());

	if (r) {
		remove_route (r);
	}
}

void
TrackFolder::clear ()
{
	RouteVector copy (_routes);

	for (auto& r : copy) {
		remove_route (r);
	}
}

bool
TrackFolder::contains (std::shared_ptr<Route> r) const
{
	return find (_routes.begin (), _routes.end (), r) != _routes.end ();
}

std::shared_ptr<Route>
TrackFolder::topmost_route () const
{
	if (_routes.empty ()) {
		return std::shared_ptr<Route> ();
	}

	std::shared_ptr<Route> top = _routes.front ();

	for (auto const& r : _routes) {
		if (r->presentation_info ().order () < top->presentation_info ().order ()) {
			top = r;
		}
	}

	return top;
}

void
TrackFolder::set_collapsed (bool yn)
{
	if (collapsed () == yn) {
		return;
	}

	_collapsed = yn;
	send_change (PropertyChange (Properties::folder_collapsed));
	_session.set_dirty ();
}

void
TrackFolder::set_color (uint32_t rgba)
{
	if (color () == rgba) {
		return;
	}

	_color = rgba;
	send_change (PropertyChange (Properties::color));
	_session.set_dirty ();
}

XMLNode&
TrackFolder::get_state () const
{
	XMLNode* node = new XMLNode (X_("TrackFolder"));

	node->set_property ("id", id ());
	add_properties (*node);

	if (!_routes.empty ()) {
		stringstream str;

		for (auto const& r : _routes) {
			str << r->id () << ' ';
		}

		node->set_property ("routes", str.str ());
	}

	return *node;
}

int
TrackFolder::set_state (const XMLNode& node, int version)
{
	set_id (node);
	set_values (node);

	/* set_state() is invoked repeatedly across undo/redo of membership
	 * changes, so we must clear existing membership before repopulating
	 * from the XML rather than assuming this is a one-time load (unlike
	 * RouteGroup::set_state(), which only ever runs once at session load).
	 */
	clear ();

	std::string routes;
	if (node.get_property ("routes", routes)) {
		stringstream str (routes);
		vector<string> ids;
		split (str.str (), ids, ' ');

		for (auto const& i : ids) {
			if (i.empty ()) {
				continue;
			}
			PBD::ID id (i);
			std::shared_ptr<Route> r = _session.route_by_id (id);

			if (r) {
				add_route (r);
			}
		}
	}

	return 0;
}
