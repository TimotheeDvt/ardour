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

bool
TrackFolder::check_bus_compat (DataType& dt, uint32_t& nin) const
{
	if (_routes.empty ()) {
		return false;
	}

	bool midi_only = true;

	for (auto const& r : _routes) {
#ifdef MIXBUS
		if (r->mixbus ()) {
			return false;
		}
#endif
		ChanCount cc (r->output ()->n_ports ());
		if (cc.n_audio () > 0) {
			midi_only = false;
		}
	}

	dt  = midi_only ? DataType::MIDI : DataType::AUDIO;
	nin = 0;

	bool have_one = false;

	for (auto const& r : _routes) {
		ChanCount cc (r->output ()->n_ports ());
		if (have_one && nin != cc.get (dt)) {
			/* member routes must have matching channel counts: this is a
			 * hard subgroup (direct 1:1 bundle connection), not an aux-send
			 * subgroup, so there is no per-route gain stage to absorb a
			 * mismatch.
			 */
			return false;
		}
		nin = max (nin, cc.get (dt));
		have_one = true;
	}

	return have_one && nin > 0;
}

bool
TrackFolder::can_make_bus () const
{
	if (_bus) {
		return false;
	}

	DataType dt (DataType::NIL);
	uint32_t nin;
	return check_bus_compat (dt, nin);
}

void
TrackFolder::make_bus ()
{
	DataType dt (DataType::NIL);
	uint32_t nin;

	if (_bus || !check_bus_compat (dt, nin)) {
		return;
	}

	RouteList rl;

	try {
		if (dt == DataType::MIDI) {
			rl = _session.new_midi_route (0, 1, string (), true, std::shared_ptr<PluginInfo> (), 0, PresentationInfo::MidiBus, PresentationInfo::max_order);
		} else {
			uint32_t nout = nin;
			if (_session.master_out ()) {
				nout = std::max (nout, _session.master_out ()->n_inputs ().n_audio ());
			}
			rl = _session.new_audio_route (nin, nout, 0, 1, string (), PresentationInfo::AudioBus, PresentationInfo::max_order);
		}
	} catch (...) {
		return;
	}

	if (rl.empty ()) {
		return;
	}

	_bus = rl.front ();
	/* the bus takes over as the folder's on-canvas identity: give it the
	 * folder's current (pre-merge) name/color directly, not a derived name.
	 */
	_bus->set_name (SessionObject::name ());
	_bus->presentation_info ().set_color (_color.val ());
	_bus->DropReferences.connect_same_thread (*this, std::bind (&TrackFolder::unset_bus, this));

	std::shared_ptr<Bundle> bundle = _bus->input ()->bundle ();

	for (auto const& r : _routes) {
		r->output ()->disconnect ();
		r->output ()->connect_ports_to_bundle (bundle, false, true);
	}

	reposition_bus_above_members ();

	_session.set_dirty ();
	BusChanged (); /* EMIT SIGNAL */
}

void
TrackFolder::reposition_bus_above_members ()
{
	std::shared_ptr<Route> top = topmost_route ();

	if (!_bus || !top) {
		return;
	}
	PresentationInfo::ChangeSuspender cs;

	PresentationInfo::order_t const target = top->presentation_info ().order ();

	StripableList sl;
	_session.get_stripables (sl);

	for (auto const& s : sl) {
		if (s == _bus) {
			continue;
		}
		if (s->presentation_info ().order () >= target) {
			s->set_presentation_order (s->presentation_info ().order () + 1);
		}
	}

	_bus->set_presentation_order (target);
}

void
TrackFolder::remove_bus ()
{
	if (!_bus) {
		return;
	}

	for (auto const& r : _routes) {
		r->output ()->disconnect ();
		/* XXX find a new bundle to connect to, e.g. master -- same
		 * limitation as RouteGroup::destroy_subgroup().
		 */
	}

	SessionObject::set_name (_("Folder"));
	_color = 0x707070ff;

	PropertyChange change;
	change.add (Properties::name);
	change.add (Properties::color);
	send_change (change);

	_session.set_dirty ();

	_session.remove_route (_bus);
}

void
TrackFolder::unset_bus ()
{
	if (_session.deletion_in_progress ()) {
		return;
	}
	_bus.reset ();
	BusChanged (); /* EMIT SIGNAL */
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
	if (_session.deletion_in_progress ()) {
		return;
	}

	std::shared_ptr<Route> r (wr.lock ());

	if (!r) {
		return;
	}

	remove_route (r);

	if (_routes.empty ()) {
		if (_bus) {
			/* a bus with no member tracks feeding it is useless */
			remove_bus ();
		}
		Emptied (shared_from_this ()); /* EMIT SIGNAL */
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

std::string
TrackFolder::name () const
{
	if (_bus) {
		return _bus->name ();
	}
	return SessionObject::name ();
}

bool
TrackFolder::set_name (const std::string& str)
{
	if (_bus) {
		return _bus->set_name (str);
	}
	return SessionObject::set_name (str);
}

uint32_t
TrackFolder::color () const
{
	if (_bus) {
		return _bus->presentation_info ().color ();
	}
	return _color.val ();
}

void
TrackFolder::set_color (uint32_t rgba)
{
	if (_bus) {
		_bus->presentation_info ().set_color (rgba);
		return;
	}

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

	if (_bus) {
		node->set_property ("bus", _bus->id ());
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

	_bus.reset ();
	PBD::ID bus_id (0);
	if (node.get_property ("bus", bus_id)) {
		std::shared_ptr<Route> r = _session.route_by_id (bus_id);
		if (r) {
			_bus = r;
			_bus->DropReferences.connect_same_thread (*this, std::bind (&TrackFolder::unset_bus, this));
		}
	}

	return 0;
}
