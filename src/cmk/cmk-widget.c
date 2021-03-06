/*
 * This file is part of graphene-desktop, the desktop environment of VeltOS.
 * Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
 * Licensed under the Apache License 2 <www.apache.org/licenses/LICENSE-2.0>.
 */

#include "cmk-widget.h"

typedef struct _CmkWidgetPrivate CmkWidgetPrivate;
struct _CmkWidgetPrivate
{
	CmkWidget *styleParent; // Not ref'd
	CmkWidget *actualStyleParent; // styleParent, real parent, or NULL (not ref'd)
	GHashTable *colors;
	float bevelRadius;
	float padding;
	float scaleFactor;

	gchar *backgroundColorName;
	gboolean drawBackground;

	gboolean emittingStyleChanged;
	gboolean disposed; // Various callbacks (ex clutter canvas draws) like to emit even after their actor has been disposed. This is checked to avoid runtime "CRITICAL" messages on disposed objects (and unnecessary processing)
};

enum
{
	// TODO: Make all style properties into GObject properties
	PROP_BACKGROUND_COLOR_NAME = 1,
	PROP_LAST
};

enum
{
	SIGNAL_STYLE_CHANGED = 1,
	SIGNAL_BACKGROUND_CHANGED,
	SIGNAL_LAST
};

static GParamSpec *properties[PROP_LAST];
static guint signals[SIGNAL_LAST];

static void cmk_widget_constructed(GObject *self_);
static void cmk_widget_dispose(GObject *self_);
static void cmk_widget_set_property(GObject *self_, guint propertyId, const GValue *value, GParamSpec *pspec);
static void cmk_widget_get_property(GObject *self_, guint propertyId, GValue *value, GParamSpec *pspec);
static void emit_style_changed(CmkWidget *self);
static void on_parent_changed(ClutterActor *self_, ClutterActor *prevParent);
static void set_actual_style_parent(CmkWidget *self, CmkWidget *parent);
static void update_actual_style_parent(CmkWidget *self);
static void on_style_changed(CmkWidget *self);
static void on_background_changed(CmkWidget *self);
static void update_named_background_color(CmkWidget *self);

G_DEFINE_TYPE_WITH_PRIVATE(CmkWidget, cmk_widget, CLUTTER_TYPE_ACTOR);
#define PRIVATE(widget) ((CmkWidgetPrivate *)cmk_widget_get_instance_private(widget))

CmkWidget * cmk_widget_new(void)
{
	return CMK_WIDGET(g_object_new(CMK_TYPE_WIDGET, NULL));
}

static CmkWidget *styleDefault = NULL;

CmkWidget * cmk_widget_get_style_default(void)
{
	if(CMK_IS_WIDGET(styleDefault))
		return g_object_ref(styleDefault);
	styleDefault = cmk_widget_new();
	return styleDefault;
}

static void cmk_widget_class_init(CmkWidgetClass *class)
{
	GObjectClass *base = G_OBJECT_CLASS(class);
	base->constructed = cmk_widget_constructed;
	base->dispose = cmk_widget_dispose;
	base->get_property = cmk_widget_get_property;
	base->set_property = cmk_widget_set_property;
	
	CLUTTER_ACTOR_CLASS(class)->parent_set = on_parent_changed;

	class->style_changed = on_style_changed;
	class->background_changed = on_background_changed;

	properties[PROP_BACKGROUND_COLOR_NAME] = g_param_spec_string("background-color-name", "background-color-name", "Named background color using CmkStyle", NULL, G_PARAM_READWRITE);

	g_object_class_install_properties(base, PROP_LAST, properties);
	
	signals[SIGNAL_STYLE_CHANGED] = g_signal_new("style-changed", G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET(CmkWidgetClass, style_changed), NULL, NULL, NULL, G_TYPE_NONE, 0);

	signals[SIGNAL_BACKGROUND_CHANGED] = g_signal_new("background-changed", G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET(CmkWidgetClass, background_changed), NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void cmk_widget_init(CmkWidget *self)
{
	CmkWidgetPrivate *private = PRIVATE(self);
	private->colors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)clutter_color_free);
	private->padding = -1;
	private->bevelRadius = -1;
	private->scaleFactor = -1;
	private->emittingStyleChanged = TRUE;
	set_actual_style_parent(self, styleDefault);
	private->emittingStyleChanged = FALSE;
}

static void cmk_widget_constructed(GObject *self_)
{
	emit_style_changed(CMK_WIDGET(self_));
}

static void cmk_widget_dispose(GObject *self_)
{
	CmkWidgetPrivate *private = PRIVATE(CMK_WIDGET(self_));
	private->styleParent = NULL;
	set_actual_style_parent(CMK_WIDGET(self_), NULL);
	g_clear_pointer(&private->colors, g_hash_table_unref);
	g_clear_pointer(&private->backgroundColorName, g_free);
	private->disposed = TRUE;
	G_OBJECT_CLASS(cmk_widget_parent_class)->dispose(self_);
}

static void cmk_widget_set_property(GObject *self_, guint propertyId, const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(CMK_IS_WIDGET(self_));
	CmkWidget *self = CMK_WIDGET(self_);
	
	switch(propertyId)
	{
	case PROP_BACKGROUND_COLOR_NAME:
		cmk_widget_set_background_color_name(self, g_value_get_string(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(self, propertyId, pspec);
		break;
	}
}

static void cmk_widget_get_property(GObject *self_, guint propertyId, GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(CMK_IS_WIDGET(self_));
	CmkWidget *self = CMK_WIDGET(self_);
	
	switch(propertyId)
	{
	case PROP_BACKGROUND_COLOR_NAME:
		g_value_set_string(value, cmk_widget_get_background_color_name(self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(self, propertyId, pspec);
		break;
	}
}

gboolean cmk_widget_destroy(CmkWidget *widget)
{
	clutter_actor_destroy(CLUTTER_ACTOR(widget));
	return G_SOURCE_REMOVE;
}

static void emit_style_changed(CmkWidget *self)
{
	CmkWidgetPrivate *private = PRIVATE(self);
	if(private->emittingStyleChanged)
		return;
	// Set to FALSE in on_style_changed, the top-level object signal handler for the style changed signal
	// This helps with signal recursion when settings styles in the on_style_changed callback (which is called both when the parent style changes or when the current style changes). The style set handlers check for this anyway, but this removes a little extra work.
	private->emittingStyleChanged = TRUE;
	g_signal_emit(self, signals[SIGNAL_STYLE_CHANGED], 0);
}

const ClutterColor * cmk_widget_style_get_color(CmkWidget *self, const gchar *name)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), NULL);
	CmkWidgetPrivate *private = PRIVATE(self);
	if(G_UNLIKELY(!name || private->disposed))
		return NULL;
	const ClutterColor *ret = NULL;
	if(g_hash_table_size(private->colors) > 0)
		ret = g_hash_table_lookup(private->colors, name);
	if(ret)
		return ret;
	if(private->actualStyleParent)
		return cmk_widget_style_get_color(private->actualStyleParent, name);
	return NULL;
}

void cmk_widget_style_set_color(CmkWidget *self, const gchar *name, const ClutterColor *color)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	ClutterColor *c = clutter_color_copy(color);
	g_hash_table_insert(PRIVATE(self)->colors, g_strdup(name), c);
	emit_style_changed(self);
	g_signal_emit(self, signals[SIGNAL_BACKGROUND_CHANGED], 0);
}

void cmk_widget_style_set_bevel_radius(CmkWidget *self, float radius)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	radius = MAX(radius, 0.0f);
	gboolean diff = (radius != PRIVATE(self)->bevelRadius);
	PRIVATE(self)->bevelRadius = radius;
	if(diff)
		emit_style_changed(self);
}

float cmk_widget_style_get_bevel_radius(CmkWidget *self)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), 0);
	CmkWidgetPrivate *private = PRIVATE(self);
	if(G_UNLIKELY(private->disposed))
		return 0;
	if(private->bevelRadius >= 0)
		return private->bevelRadius; // TODO: Scale factor
	if(private->actualStyleParent)
		return cmk_widget_style_get_bevel_radius(private->actualStyleParent);
	return 0;
}

void cmk_widget_style_set_padding(CmkWidget *self, float padding)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	padding = MAX(padding, 0.0f);
	gboolean diff = (padding != PRIVATE(self)->padding);
	PRIVATE(self)->padding = padding;
	if(diff)
		emit_style_changed(self);
}

static float _cmk_widget_style_get_padding_internal(CmkWidget *self, CmkWidget *origin)
{
	CmkWidgetPrivate *private = PRIVATE(self);
	if(G_UNLIKELY(private->disposed))
		return 0;
	if(private->padding >= 0)
		return private->padding * cmk_widget_style_get_scale_factor(origin);
	if(private->actualStyleParent)
		return _cmk_widget_style_get_padding_internal(private->actualStyleParent, origin);
	return 0;
}

float cmk_widget_style_get_padding(CmkWidget *self)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), 0);
	return _cmk_widget_style_get_padding_internal(self, self);
}

void cmk_widget_style_set_scale_factor(CmkWidget *self, float scale)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	scale = MIN(MAX(scale, 0.1f), 10.0f);
	gboolean diff = (scale != PRIVATE(self)->scaleFactor);
	PRIVATE(self)->scaleFactor = scale;
	if(diff)
		emit_style_changed(self);
}

float cmk_widget_style_get_scale_factor(CmkWidget *self)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), 0);
	CmkWidgetPrivate *private = PRIVATE(self);
	if(G_UNLIKELY(private->disposed))
		return 1;
	if(private->scaleFactor >= 0)
		return private->scaleFactor;
	if(private->actualStyleParent)
		return cmk_widget_style_get_scale_factor(private->actualStyleParent);
	return 1;
}

const ClutterColor * cmk_widget_get_foreground_color(CmkWidget *self)
{
	const ClutterColor *black = clutter_color_get_static(CLUTTER_COLOR_BLACK);
	g_return_val_if_fail(CMK_IS_WIDGET(self), black);
	if(G_UNLIKELY(PRIVATE(self)->disposed))
		return black;

	const gchar *bgColor = cmk_widget_get_background_color_name(self);
	const ClutterColor *color = NULL;
	if(bgColor)
	{
		gchar *name = g_strdup_printf("%s-foreground", bgColor);
		color = cmk_widget_style_get_color(self, name);
		g_free(name);
		if(color)
			return color;
	}
	color = cmk_widget_style_get_color(self, "foreground");
	if(color)
		return color;
	return black;
} 

static void on_style_parent_style_changed(CmkWidget *self)
{
	// Relay signal
	emit_style_changed(self);
	g_signal_emit(self, signals[SIGNAL_BACKGROUND_CHANGED], 0);
}

static void on_style_parent_background_changed(CmkWidget *self)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	// Only say we've changed colors if we're not drawing our own background
	if(!PRIVATE(self)->backgroundColorName)
		g_signal_emit(self, signals[SIGNAL_BACKGROUND_CHANGED], 0);
}

static void on_style_parent_destroy(CmkWidget *self, CmkWidget *styleParent)
{
	if(PRIVATE(self)->styleParent == styleParent)
		PRIVATE(self)->styleParent = NULL;
	update_actual_style_parent(self);
}

static void on_parent_changed(ClutterActor *self_, ClutterActor *prevParent)
{
	g_return_if_fail(CMK_IS_WIDGET(self_));
	update_actual_style_parent(CMK_WIDGET(self_));
}

static void set_actual_style_parent(CmkWidget *self, CmkWidget *parent)
{
	CmkWidgetPrivate *private = PRIVATE(self);
	if(private->actualStyleParent == parent)
		return;
	if(private->actualStyleParent)
	{
		g_signal_handlers_disconnect_by_func(private->actualStyleParent, G_CALLBACK(on_style_parent_style_changed), self);
		g_signal_handlers_disconnect_by_func(private->actualStyleParent, G_CALLBACK(on_style_parent_background_changed), self);
		g_signal_handlers_disconnect_by_func(private->actualStyleParent, G_CALLBACK(on_style_parent_destroy), self);
	}
	
	private->actualStyleParent = parent;
	if(parent)
	{
		g_signal_connect_swapped(private->actualStyleParent, "style-changed", G_CALLBACK(on_style_parent_style_changed), self);
		g_signal_connect_swapped(private->actualStyleParent, "background-changed", G_CALLBACK(on_style_parent_background_changed), self);
		g_signal_connect_swapped(private->actualStyleParent, "destroy", G_CALLBACK(on_style_parent_destroy), self);
	}

	emit_style_changed(self);
	g_signal_emit(self, signals[SIGNAL_BACKGROUND_CHANGED], 0);
}

static void update_actual_style_parent(CmkWidget *self)
{
	CmkWidgetPrivate *private = PRIVATE(self);
	if(private->disposed)
		return;	
	ClutterActor *parent = clutter_actor_get_parent(CLUTTER_ACTOR(self));
	
	if(private->styleParent)
		set_actual_style_parent(self, private->styleParent);	
	else if(CMK_IS_WIDGET(parent))
		set_actual_style_parent(self, CMK_WIDGET(parent));
	else if(styleDefault)
		set_actual_style_parent(self, styleDefault);
	else
		set_actual_style_parent(self, NULL);
}

void cmk_widget_set_style_parent(CmkWidget *self, CmkWidget *parent)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	g_return_if_fail(parent == NULL || CMK_IS_WIDGET(parent));
	if(PRIVATE(self)->styleParent == parent)
		return;
	PRIVATE(self)->styleParent = parent;
	update_actual_style_parent(self);
}

CmkWidget * cmk_widget_get_style_parent(CmkWidget *self)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), NULL);
	return PRIVATE(self)->actualStyleParent;
}

static void on_style_changed(CmkWidget *self)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	update_named_background_color(self);
	PRIVATE(self)->emittingStyleChanged = FALSE; // see emit_style_changed
}

static void on_background_changed(CmkWidget *self)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	update_named_background_color(self);
}

void cmk_widget_set_background_color_name(CmkWidget *self, const gchar *namedColor)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	if(g_strcmp0(PRIVATE(self)->backgroundColorName, namedColor) == 0)
		return;
	g_clear_pointer(&(PRIVATE(self)->backgroundColorName), g_free);
	PRIVATE(self)->backgroundColorName = g_strdup(namedColor);
	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_BACKGROUND_COLOR_NAME]);
	g_signal_emit(self, signals[SIGNAL_BACKGROUND_CHANGED], 0);
}

/*
 * If this is set to not draw, then the widget should be drawing its own
 * background with the set color. So this shouldn't emit a background color
 * changed signal.
 */
void cmk_widget_set_draw_background_color(CmkWidget *self, gboolean draw)
{
	g_return_if_fail(CMK_IS_WIDGET(self));
	if(PRIVATE(self)->drawBackground == draw)
		return;

	PRIVATE(self)->drawBackground = draw;
	update_named_background_color(self);
	
	// If changing from drawing background to not, clear the background color
	if(!draw)
		clutter_actor_set_background_color(CLUTTER_ACTOR(self), NULL);
}

const gchar * cmk_widget_get_background_color_name(CmkWidget *self)
{
	g_return_val_if_fail(CMK_IS_WIDGET(self), NULL);
	CmkWidgetPrivate *private = PRIVATE(self);
	if(G_UNLIKELY(private->disposed))
		return NULL;
	if(private->backgroundColorName)
		return private->backgroundColorName;
	if(private->actualStyleParent)
		return cmk_widget_get_background_color_name(private->actualStyleParent);
	return NULL;
}

const ClutterColor * cmk_widget_get_background_color(CmkWidget *self)
{
	const gchar *name = cmk_widget_get_background_color_name(self);
	if(name)
	{
		const ClutterColor *color = cmk_widget_style_get_color(self, name);
		if(color)
			return color;
	}
	return clutter_color_get_static(CLUTTER_COLOR_WHITE);
}

static void update_named_background_color(CmkWidget *self)
{
	if(!PRIVATE(self)->drawBackground)
		return;
	const ClutterColor *color = NULL;
	if(PRIVATE(self)->backgroundColorName)
		color = cmk_widget_get_background_color(self);
	clutter_actor_set_background_color(CLUTTER_ACTOR(self), color);
}

void cairo_set_source_clutter_color(cairo_t *cr, const ClutterColor *color)
{
	if(!color) return; // Don't produce warnings because this happens naturally sometimes during object destruction
	cairo_set_source_rgba(cr, color->red/255.0, color->green/255.0, color->blue/255.0, color->alpha/255.0);
}

void cmk_scale_actor_box(ClutterActorBox *b, gfloat scale, gboolean move)
{
	if(!b)
		return;
	gfloat width = b->x2 - b->x1;
	gfloat height = b->y2 - b->y1;

	if(move)
	{
		b->x1 *= scale;
		b->y1 *= scale;
	}

	b->x2 = b->x1 + width*scale;
	b->y2 = b->y1 + height*scale;
}
