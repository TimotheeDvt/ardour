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
#include <string>

#include "pbd/signals.h"
#include "pbd/statefuldestructible.h"

#include "ardour/session_handle.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class Route;
class Session;
class TrackFolder;

/** The session-wide collection of TrackFolders.
 *
 *  Modeled on Locations rather than RouteGroupList: get_state()/set_state()
 *  serialize the *entire* membership and reconcile by ID on load (reuse an
 *  existing TrackFolder if its ID is still present, construct a new one for
 *  an unmatched ID, drop any TrackFolder no longer present). This is what
 *  lets a plain MementoCommand<TrackFolderList> undo/redo folder creation
 *  and deletion, not just a property change on an existing, still-live
 *  folder (which is instead handled by MementoCommand<TrackFolder>).
 */
class LIBARDOUR_API TrackFolderList : public SessionHandleRef, public PBD::StatefulDestructible
{
public:
	typedef std::list<std::shared_ptr<TrackFolder> > List;

	TrackFolderList (Session&);
	~TrackFolderList ();

	List const& list () const { return _folders; }

	std::shared_ptr<TrackFolder> new_folder (const std::string& name);
	void add (std::shared_ptr<TrackFolder>);
	void remove (std::shared_ptr<TrackFolder>);

	std::shared_ptr<TrackFolder> folder_for_route (std::shared_ptr<Route>) const;
	std::shared_ptr<TrackFolder> folder_by_name (std::string) const;

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);

	/** Emitted when a folder is added to the session (including on session load) */
	PBD::Signal<void(std::shared_ptr<TrackFolder>)> TrackFolderAdded;
	/** Emitted when a folder is removed from the session */
	PBD::Signal<void(std::shared_ptr<TrackFolder>)> TrackFolderRemoved;

private:
	List _folders;
};

} /* namespace */
