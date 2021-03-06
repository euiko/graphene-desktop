/*
 * This file is part of graphene-desktop, the desktop environment of VeltOS.
 * Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * wm.h/.c
 * Graphene's window manager (a Mutter plugin)
 */

#ifndef __GRAPHENE_WM_H__
#define __GRAPHENE_WM_H__

#include <meta/main.h>
#include <meta/meta-plugin.h>
#include <meta/meta-background-group.h>
#include "percent-floater.h"
#include "csk/audio.h"
#include "panel.h"
#include "notifications.h"

G_BEGIN_DECLS

#define GRAPHENE_TYPE_WM (graphene_wm_get_type())
extern void glib_autoptr_cleanup_MetaPlugin(MetaPlugin **_ptr); // Fix implicit function declaration warning. Weird GLib stuff.
G_DECLARE_FINAL_TYPE(GrapheneWM, graphene_wm, GRAPHENE, WM, MetaPlugin);

struct _GrapheneWM {
	MetaPlugin parent;
	ClutterActor *stage;
	MetaBackgroundGroup *backgroundGroup;
	GraphenePercentFloater *percentBar;
	CskAudioDeviceManager *audioManager;
	ClutterActor *coverGroup;
	ClutterActor *dialog;
	GraphenePanel *panel;
	GrapheneNotificationBox *notificationBox;
	gint modalCount;
	
	// For fixing an input issue with the X backend
	// See xfixes_calculate_input_region (wm.c) for more details
	GList *xInputActors; // List of ClutterActors
	XserverRegion xInputRegion;
};

void graphene_wm_show_dialog(GrapheneWM *wm, ClutterActor *actor);

const MetaPluginInfo * graphene_wm_plugin_info(MetaPlugin *plugin);
void graphene_wm_start(MetaPlugin *plugin);
void graphene_wm_minimize(MetaPlugin *plugin, MetaWindowActor *windowActor);
void graphene_wm_unminimize(MetaPlugin *plugin, MetaWindowActor *windowActor);
void graphene_wm_destroy(MetaPlugin *plugin, MetaWindowActor *windowActor);
void graphene_wm_map(MetaPlugin *plugin, MetaWindowActor *windowActor);

G_END_DECLS

#endif /* __GRAPHENE_WM_H__ */
