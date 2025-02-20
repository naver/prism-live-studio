/**
 * Copyright Off World Live Ltd (https://offworld.live), 2019-2021
 *
 * and licenced under the GPL v2 (https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
 *
 * Many thanks to authors of https://github.com/baffler/OBS-OpenVR-Input-Plugin which
 * was used as guidance to working with the OBS Studio APIs
 */

#ifndef WINSPOUTOUTSETTINGS_H
#define WINSPOUTOUTSETTINGS_H

#include <QDialog>
#include "ui_win-spout-output-settings.h"

//PRISM/fanzirong/20240401/4932/Modify the UI style and unify it with PRISM
#include <PLSDialogView.h>

class win_spout_output_settings : public PLSDialogView {
	Q_OBJECT

public:
	explicit win_spout_output_settings(QWidget* parent = 0);
	~win_spout_output_settings();
	void set_started_button_state(bool started);
	void close_event(QCloseEvent* event);
	void toggle_show_hide();

	//PRISM/FanZirong/20240802/PRISM_PC-755/save setting in shutdown
	void save_settings();

private Q_SLOTS:
	void on_start();
	void on_stop();

private:
	Ui::win_spout_output_settings* ui;
	//PRISM/FanZirong/20240802/PRISM_PC-755/save setting in shutdown
	//void save_settings();
};

#endif // WINSPOUTOUTSETTINGS_H
