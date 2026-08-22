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

#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track_folder.h"
#include "ardour/track_folder_list.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

TrackFolderList::TrackFolderList (Session& s)
	: SessionHandleRef (s)
{
}

TrackFolderList::~TrackFolderList ()
{
	_folders.clear ();
}

std::shared_ptr<TrackFolder>
TrackFolderList::new_folder (const string& name)
{
	return std::shared_ptr<TrackFolder> (new TrackFolder (_session, name));
}

void
TrackFolderList::add (std::shared_ptr<TrackFolder> f)
{
	if (!f) {
		return;
	}

	if (find (_folders.begin (), _folders.end (), f) != _folders.end ()) {
		return;
	}

	_folders.push_back (f);
	_session.set_dirty ();
	TrackFolderAdded (f); /* EMIT SIGNAL */
}

void
TrackFolderList::remove (std::shared_ptr<TrackFolder> f)
{
	List::iterator i = find (_folders.begin (), _folders.end (), f);

	if (i == _folders.end ()) {
		return;
	}

	_folders.erase (i);
	_session.set_dirty ();
	TrackFolderRemoved (f); /* EMIT SIGNAL */
}

std::shared_ptr<TrackFolder>
TrackFolderList::folder_for_route (std::shared_ptr<Route> r) const
{
	if (!r) {
		return std::shared_ptr<TrackFolder> ();
	}

	for (auto const& f : _folders) {
		if (f->contains (r)) {
			return f;
		}
	}

	return std::shared_ptr<TrackFolder> ();
}

std::shared_ptr<TrackFolder>
TrackFolderList::folder_for_bus (std::shared_ptr<Route> r) const
{
	if (!r) {
		return std::shared_ptr<TrackFolder> ();
	}

	for (auto const& f : _folders) {
		if (f->has_bus () && f->bus () == r) {
			return f;
		}
	}

	return std::shared_ptr<TrackFolder> ();
}

std::shared_ptr<TrackFolder>
TrackFolderList::folder_by_name (string name) const
{
	for (auto const& f : _folders) {
		if (f->name () == name) {
			return f;
		}
	}

	return std::shared_ptr<TrackFolder> ();
}

XMLNode&
TrackFolderList::get_state () const
{
	XMLNode* node = new XMLNode (X_("TrackFolders"));

	for (auto const& f : _folders) {
		node->add_child_nocopy (f->get_state ());
	}

	return *node;
}

int
TrackFolderList::set_state (const XMLNode& node, int version)
{
	if (node.name () != X_("TrackFolders")) {
		error << _("incorrect XML node passed to TrackFolderList::set_state") << endmsg;
		return -1;
	}

	List new_folders;
	List newly_added;

	for (auto const& n : node.children ()) {
		if (n->name () != X_("TrackFolder")) {
			continue;
		}

		XMLProperty const* prop_id = n->property ("id");
		if (!prop_id) {
			continue;
		}
		PBD::ID id (prop_id->value ());

		List::const_iterator i = _folders.begin ();
		while (i != _folders.end () && (*i)->id () != id) {
			++i;
		}

		std::shared_ptr<TrackFolder> f;

		if (i != _folders.end ()) {
			/* re-use the existing object so any live references (e.g. from
			 * an in-flight MementoCommand<TrackFolder>, or a GUI TimeAxisView)
			 * remain valid.
			 */
			f = *i;
		} else {
			f = std::shared_ptr<TrackFolder> (new TrackFolder (_session, ""));
			newly_added.push_back (f);
		}

		f->set_state (*n, version);
		new_folders.push_back (f);
	}

	/* anything present before, but not in the new state, was removed */
	for (auto const& old_f : _folders) {
		bool found = false;
		for (auto const& new_f : new_folders) {
			if (new_f->id () == old_f->id ()) {
				found = true;
				break;
			}
		}
		if (!found) {
			TrackFolderRemoved (old_f); /* EMIT SIGNAL */
		}
	}

	_folders = new_folders;

	for (auto const& f : newly_added) {
		TrackFolderAdded (f); /* EMIT SIGNAL */
	}

	return 0;
}
