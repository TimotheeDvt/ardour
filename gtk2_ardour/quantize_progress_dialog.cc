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

#include <ytkmm/box.h>
#include <ytkmm/label.h>
#include <ytkmm/stock.h>

#include "quantize_progress_dialog.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace Gtk;

QuantizeProgressDialog::QuantizeProgressDialog (Gtk::Window& parent)
	: ArdourDialog (X_("quantize progress dialog"))
	, progress (0.0f)
{
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_name (N_("QuantizeProgressDialog"));
	set_transient_for (parent);
	set_title (_("Quantizing Audio"));

	cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);

	VBox* vbox = manage (new VBox);
	vbox->set_spacing (6);
	vbox->set_border_width (12);

	Label* l = manage (new Label (_("<b>Progress</b>"), Gtk::ALIGN_START, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();

	vbox->pack_start (*l, false, false);
	vbox->pack_start (progress_bar, false, true);

	get_vbox()->pack_start (*vbox, false, false);

	first_cancel = cancel_button->signal_clicked().connect (sigc::mem_fun (*this, &QuantizeProgressDialog::cancel_in_progress));
	first_delete = signal_delete_event().connect (sigc::mem_fun (*this, &QuantizeProgressDialog::delete_in_progress));

	show_all_children ();
}

void
QuantizeProgressDialog::start_updates ()
{
	update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &QuantizeProgressDialog::timer_update));
}

void
QuantizeProgressDialog::update_progress_gui (float p)
{
	/* called from the worker thread; the timer-driven callback below
	 * picks this up and updates the widget on the GUI thread.
	 */
	progress = p;
}

void
QuantizeProgressDialog::timer_update ()
{
	progress_bar.set_fraction (progress);

	if (request.done || request.cancel) {
		update_connection.disconnect ();
	}
}

void
QuantizeProgressDialog::cancel_in_progress ()
{
	request.cancel = true;
	first_cancel.disconnect ();
}

gint
QuantizeProgressDialog::delete_in_progress (GdkEventAny*)
{
	request.cancel = true;
	first_delete.disconnect ();
	return TRUE;
}
