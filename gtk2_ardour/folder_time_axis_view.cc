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

#include <ytkmm/menu.h>

#include "pbd/memento_command.h"
#include "pbd/whitespace.h"

#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/track_folder.h"
#include "ardour/track_folder_list.h"

#include "widgets/tooltips.h"

#include "folder_time_axis_view.h"
#include "gui_thread.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace PBD;
using namespace std;

FolderTimeAxisView::FolderTimeAxisView (PublicEditor& ed, Session* s, ArdourCanvas::Canvas& canvas, std::shared_ptr<TrackFolder> folder)
	: SessionHandlePtr (s)
	, TimeAxisView (s, ed, 0, canvas)
	, _folder (folder)
{
	controls_base_selected_name = X_("ControlMasterBaseSelected");
	controls_base_unselected_name = X_("ControlMasterBaseUnselected");

	_collapse_button.set_name ("route button");
	_collapse_button.set_text (S_("Folder|>"));
	set_tooltip (_collapse_button, _("Collapse/expand this folder"));
	_collapse_button.signal_button_release_event ().connect (sigc::mem_fun (*this, &FolderTimeAxisView::collapse_button_release), false);
	_collapse_button.set_can_focus (false);
	_collapse_button.set_tweaks (ArdourButton::TrackHeader);

	_member_count_button.set_name ("route button");
	_member_count_button.set_can_focus (false);
	_member_count_button.set_sensitive (false);
	_member_count_button.set_tweaks (ArdourButton::TrackHeader);

	controls_table.attach (_collapse_button, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);
	controls_table.attach (_member_count_button, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK, 0, 0);

	_collapse_button.show ();
	_member_count_button.show ();

	controls_ebox.set_name (controls_base_unselected_name);
	time_axis_frame.set_name (controls_base_unselected_name);

	name_label.set_text (_folder->name ());

	_folder->PropertyChanged.connect (_folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::folder_property_changed, this, _1), gui_context ());
	_folder->RouteAdded.connect (_folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::folder_membership_changed, this, _1, _2), gui_context ());
	_folder->RouteRemoved.connect (_folder_connections, invalidator (*this), std::bind (&FolderTimeAxisView::folder_membership_changed, this, _1, _2), gui_context ());

	update_collapse_button ();
	update_member_count ();

	set_height (preset_height (HeightSmall));
}

FolderTimeAxisView::~FolderTimeAxisView ()
{
	CatchDeletion (this);
}

std::string
FolderTimeAxisView::name () const
{
	return _folder->name ();
}

Gdk::Color
FolderTimeAxisView::color () const
{
	Gdk::Color c;
	c.set_rgb_p (0.5, 0.5, 0.55);
	return c;
}

std::string
FolderTimeAxisView::state_id () const
{
	return string_compose ("folder %1", _folder->id ().to_s ());
}

std::shared_ptr<Route>
FolderTimeAxisView::anchor_route () const
{
	return _folder->topmost_route ();
}

void
FolderTimeAxisView::update_collapse_button ()
{
	_collapse_button.set_text (_folder->collapsed () ? S_("Folder|>") : S_("Folder|v"));
}

void
FolderTimeAxisView::update_member_count ()
{
	_member_count_button.set_text (string_compose (P_("%1 track", "%1 tracks", _folder->size ()), _folder->size ()));
}

bool
FolderTimeAxisView::collapse_button_release (GdkEventButton*)
{
	toggle_collapsed ();
	return true;
}

void
FolderTimeAxisView::toggle_collapsed ()
{
	XMLNode& before (_folder->get_state ());
	_folder->set_collapsed (!_folder->collapsed ());
	XMLNode& after (_folder->get_state ());

	_session->begin_reversible_command (_folder->collapsed () ? _("Collapse Folder") : _("Expand Folder"));
	_session->commit_reversible_command (new MementoCommand<TrackFolder> (*_folder, &before, &after));
}

void
FolderTimeAxisView::folder_property_changed (PBD::PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::folder_collapsed)) {
		update_collapse_button ();
		_editor.queue_redisplay_track_views ();
	}
	if (what_changed.contains (Properties::name)) {
		name_label.set_text (_folder->name ());
	}
}

void
FolderTimeAxisView::folder_membership_changed (std::shared_ptr<TrackFolder>, std::weak_ptr<Route>)
{
	update_member_count ();
	_editor.queue_redisplay_track_views ();
}

bool
FolderTimeAxisView::name_entry_changed (string const& str)
{
	if (str == _folder->name ()) {
		return true;
	}

	string x = str;
	strip_whitespace_edges (x);

	if (x.empty ()) {
		return false;
	}

	XMLNode& before (_folder->get_state ());
	_folder->set_name (x);
	XMLNode& after (_folder->get_state ());

	_session->begin_reversible_command (_("Rename Folder"));
	_session->commit_reversible_command (new MementoCommand<TrackFolder> (*_folder, &before, &after));

	return true;
}

void
FolderTimeAxisView::ungroup ()
{
	std::shared_ptr<TrackFolder> folder = _folder;
	Session* s = _session;

	XMLNode& before (s->track_folders ()->get_state ());
	s->track_folders ()->remove (folder);
	XMLNode& after (s->track_folders ()->get_state ());

	s->begin_reversible_command (_("Remove Folder"));
	s->commit_reversible_command (new MementoCommand<TrackFolderList> (*(s->track_folders ()), &before, &after));
}

void
FolderTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	TimeAxisView::build_display_menu ();

	MenuList& items = display_menu->items ();

	items.push_back (MenuElem (_folder->collapsed () ? _("Expand Folder") : _("Collapse Folder"), sigc::mem_fun (*this, &FolderTimeAxisView::toggle_collapsed)));
	items.push_back (SeparatorElem ());
	items.push_back (MenuElem (_("Remove Folder (keeps tracks)"), sigc::mem_fun (*this, &FolderTimeAxisView::ungroup)));
}
