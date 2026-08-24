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

#include <vector>

#include <ytkmm/button.h>
#include <ytkmm/progressbar.h>

#include "temporal/timeline.h"
#include "temporal/types.h"

#include "ardour/timefx_request.h"

#include "ardour_dialog.h"
#include "progress_reporter.h"

namespace ARDOUR {
	class Region;
	class Playlist;
}

/** One slice produced by the quantize split pass: either move it to
 *  target_position with no change to its content (ratio == 1/1), or
 *  time-stretch it so its content exactly fills the gap up to
 *  target_position of the next slice (ratio != 1/1).
 */
struct QuantizeSliceJob
{
	std::shared_ptr<ARDOUR::Region>   region;
	std::shared_ptr<ARDOUR::Playlist> playlist;
	Temporal::timepos_t               target_position;
	Temporal::ratio_t                 ratio;

	QuantizeSliceJob ()
		: target_position (Temporal::AudioTime)
		, ratio (1, 1)
	{}
};

/** Minimal progress dialog shown while a background thread runs the
 *  per-slice RBStretch/move pass of audio quantize. Unlike TimeFXDialog,
 *  this has no options to gather -- the per-slice ratios are already
 *  computed before this dialog is ever shown -- so it only ever displays
 *  progress and a cancel button.
 */
class QuantizeProgressDialog : public ArdourDialog, public ProgressReporter
{
public:
	QuantizeProgressDialog (Gtk::Window& parent);

	ARDOUR::TimeFXRequest           request;
	std::vector<QuantizeSliceJob>   jobs;

	void start_updates ();

	sigc::connection first_cancel;
	sigc::connection first_delete;
	void cancel_in_progress ();
	gint delete_in_progress (GdkEventAny*);

private:
	Gtk::ProgressBar progress_bar;
	Gtk::Button*      cancel_button;
	float             progress;
	sigc::connection  update_connection;

	void update_progress_gui (float);
	void timer_update ();
};
