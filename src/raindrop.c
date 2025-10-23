/*============================================================================
Copyright (c) 2024 Raspberry Pi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#include <locale.h>
#include <math.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include "raindrop.h"

extern wm_functions_t labwc_functions;
extern wm_functions_t openbox_functions;
extern wm_functions_t wayfire_functions;

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define SNAP_DISTANCE 200

#define TEXTURE_W 4096
#define TEXTURE_H 4096

#define SCALE(n) ((n) / scale)
#define UPSCALE(n) ((n) * scale)

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

static GtkBuilder *builder;
static GtkWidget *da, *main_dlg, *undo, *zin, *zout, *conf, *clbl, *cpb, *ident, *overlay, *zooms;
static GtkWidget *id[MAX_MONS], *lbl[MAX_MONS];

monitor_t mons[MAX_MONS];
static monitor_t bmons[MAX_MONS];

GList *touchscreens;

static int mousex, mousey, screenw, screenh, curmon, scale, rev_time, tid;
static gboolean pressed;
static double press_x, press_y;
static wm_type wm;

static wm_functions_t wm_fn;

/*----------------------------------------------------------------------------*/
/* Function prototypes */
/*----------------------------------------------------------------------------*/

static int screen_w (monitor_t mon);
static int screen_h (monitor_t mon);
static void copy_config (monitor_t *from, monitor_t *to);
static gboolean compare_config (monitor_t *from, monitor_t *to);
static void clear_config (gboolean first);
static gint mode_compare (gconstpointer a, gconstpointer b);
static void sort_modes (void);
static void draw (GtkDrawingArea *, cairo_t *cr, gpointer);
static void check_frequency (int mon);
static void set_resolution (GtkMenuItem *item, gpointer data);
static void add_resolution (GtkWidget *menu, long mon, int width, int height, gboolean inter);
static void set_frequency (GtkMenuItem *item, gpointer data);
static void add_frequency (GtkWidget *menu, long mon, float freq);
static void set_orientation (GtkMenuItem *item, gpointer data);
static void add_orientation (GtkWidget *menu, long mon, const char *orient, int rotation);
static void set_scaling (GtkMenuItem *item, gpointer data);
static void add_scaling (GtkWidget *menu, long mon, float scaling);
static void set_enable (GtkCheckMenuItem *item, gpointer data);
static void set_touchscreen (GtkMenuItem *item, gpointer data);
static void set_brightness (GtkMenuItem *item, gpointer data);
static void set_primary (GtkCheckMenuItem *item, gpointer data);
static void set_mode_emu (GtkCheckMenuItem *item, gpointer data);
static void set_mode_mt (GtkCheckMenuItem *item, gpointer data);
static GtkWidget *create_menu (long mon);
static GtkWidget *create_popup (void);
static void set_timer_msg (void);
static void handle_cancel (GtkButton *, gpointer);
static void handle_ok (GtkButton *, gpointer);
static gboolean revert_timeout (gpointer data);
static void show_confirm_dialog (void);
static void find_touchscreens (void);
static void find_backlights (void);
static int get_backlight (int mon);
static void set_backlight (int mon, int level);
static gboolean button_press_event (GtkWidget *, GdkEventButton *ev, gpointer);
static gboolean motion_notify_event (GtkWidget *da, GdkEventMotion *ev, gpointer);
static gboolean button_release_event (GtkWidget *, GdkEventButton *, gpointer);
static gboolean scroll (GtkWidget *, GdkEventScroll *ev, gpointer);
static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer);
static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer);
static void handle_apply (GtkButton *, gpointer);
static void handle_undo (GtkButton *, gpointer);
static void handle_zoom (GtkButton *, gpointer data);
static void handle_menu (GtkButton *btn, gpointer);
static gboolean hide_ids (gpointer);
static void identify_monitors (void);
static void handle_ident (GtkButton *, gpointer);
static void init_config (void);
static void load_scale (void);
static void save_scale (void);
#ifndef PLUGIN_NAME
static void handle_close (GtkButton *, gpointer);
static void close_prog (GtkWidget *, GdkEvent *, gpointer);
#endif

/*----------------------------------------------------------------------------*/
/* Helper functions */
/*----------------------------------------------------------------------------*/

static int screen_w (monitor_t mon)
{
    if (mon.rotation == 90 || mon.rotation == 270) return mon.height;
    else return mon.width;
}

static int screen_h (monitor_t mon)
{
    if (mon.rotation == 90 || mon.rotation == 270) return mon.width;
    else return mon.height;
}

static void copy_config (monitor_t *from, monitor_t *to)
{
    int m;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (from[m].modes == NULL) continue;
        to[m].enabled = from[m].enabled;
        to[m].width = from[m].width;
        to[m].height = from[m].height;
        to[m].x = from[m].x;
        to[m].y = from[m].y;
        to[m].rotation = from[m].rotation;
        to[m].freq = from[m].freq;
        to[m].interlaced = from[m].interlaced;
        to[m].primary = from[m].primary;
        to[m].scale = from[m].scale;
        if (to[m].touchscreen) g_free (to[m].touchscreen);
        if (from[m].touchscreen) to[m].touchscreen = g_strdup (from[m].touchscreen);
    }
}

static gboolean compare_config (monitor_t *from, monitor_t *to)
{
    int m;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (from[m].modes == NULL) continue;
        if (to[m].enabled != from[m].enabled) return FALSE;
        if (to[m].width != from[m].width) return FALSE;
        if (to[m].height != from[m].height) return FALSE;
        if (to[m].x != from[m].x) return FALSE;
        if (to[m].y != from[m].y) return FALSE;
        if (to[m].rotation != from[m].rotation) return FALSE;
        if (to[m].freq != from[m].freq) return FALSE;
        if (to[m].interlaced != from[m].interlaced) return FALSE;
        if (to[m].primary != from[m].primary) return FALSE;
        if (to[m].scale != from[m].scale) return FALSE;
        if (g_strcmp0 (to[m].touchscreen, from[m].touchscreen)) return FALSE;
    }
    return TRUE;
}

static void clear_config (gboolean first)
{
    int m;
    for (m = 0; m < MAX_MONS; m++)
    {
        mons[m].width = 0;
        mons[m].height = 0;
        mons[m].x = 0;
        mons[m].y = 0;
        mons[m].freq = 0.0;
        mons[m].rotation = 0;
        mons[m].interlaced = FALSE;
        mons[m].modes = NULL;
        mons[m].enabled = FALSE;
        mons[m].touchscreen = NULL;
        if (!first)
        {
            if (mons[m].name) g_free (mons[m].name);
            if (mons[m].touchscreen) g_free (mons[m].touchscreen);
            if (mons[m].backlight) g_free (mons[m].backlight);
        }
        mons[m].name = NULL;
        mons[m].touchscreen = NULL;
        mons[m].backlight = NULL;
        mons[m].primary = FALSE;
        mons[m].scale = 1.0;
        mons[m].tmode = MODE_NONE;
    }
}

static gint mode_compare (gconstpointer a, gconstpointer b)
{
    output_mode_t *moda = (output_mode_t *) a;
    output_mode_t *modb = (output_mode_t *) b;

    if (moda->width > modb->width) return -1;
    if (moda->width < modb->width) return 1;
    if (moda->height > modb->height) return -1;
    if (moda->height < modb->height) return 1;
    if (moda->interlaced != modb->interlaced) return moda->interlaced ? 1 : -1;
    if (moda->freq > modb->freq) return -1;
    if (moda->freq < modb->freq) return 1;
    return 0;
}

static void sort_modes (void)
{
    int m;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        mons[m].modes = g_list_sort (mons[m].modes, mode_compare);
    }
}

/*----------------------------------------------------------------------------*/
/* Drawing */
/*----------------------------------------------------------------------------*/

static void draw (GtkDrawingArea *da, cairo_t *cr, gpointer)
{
    PangoLayout *layout;
    PangoFontDescription *font;
    int m, w, h, charwid;
    char *buf;

    GdkRGBA bg = { 0.25, 0.25, 0.25, 1.0 };
    GdkRGBA fg = { 1.0, 1.0, 1.0, 0.75 };
    GdkRGBA bk = { 0.0, 0.0, 0.0, 1.0 };

    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (da));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (da));

    // window fill
    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_rectangle (cr, 0, 0, screenw, screenh);
    cairo_fill (cr);

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

        // background
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(screen_w (mons[m])), SCALE(screen_h (mons[m])));
        cairo_fill (cr);

        // border
        gdk_cairo_set_source_rgba (cr, &bk);
        cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(screen_w (mons[m])), SCALE(screen_h (mons[m])));
        cairo_stroke (cr);

        // text label
        cairo_save (cr);
        font = pango_font_description_from_string ("sans");
        charwid = SCALE (mons[m].width) / strlen (mons[m].name);

        pango_font_description_set_size (font, charwid * PANGO_SCALE);
        layout = pango_cairo_create_layout (cr);
        pango_layout_set_text (layout, mons[m].name, -1);
        pango_layout_set_font_description (layout, font);
        pango_layout_get_pixel_size (layout, &w, &h);
        cairo_move_to (cr, SCALE(mons[m].x + screen_w (mons[m]) / 2), SCALE(mons[m].y + screen_h (mons[m]) / 2));
        cairo_rotate (cr, mons[m].rotation * G_PI / 180.0);
        cairo_rel_move_to (cr, -w / 2, -h / 2);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);

        if (mons[m].scale != 1.0)
        {
            cairo_rel_move_to (cr, w / 2, h);
            pango_font_description_set_size (font, charwid * PANGO_SCALE / 3);
            layout = pango_cairo_create_layout (cr);
            buf = g_strdup_printf ("(x%0.1f)", mons[m].scale);
            pango_layout_set_text (layout, buf, -1);
            pango_layout_set_font_description (layout, font);
            pango_layout_get_pixel_size (layout, &w, &h);
            cairo_rel_move_to (cr, -w / 2, - h / 2);
            pango_cairo_show_layout (cr, layout);
            g_free (buf);
            g_object_unref (layout);
        }

        pango_font_description_free (font);
        cairo_restore (cr);
    }
}

/*----------------------------------------------------------------------------*/
/* Menu handlers */
/*----------------------------------------------------------------------------*/

static void check_frequency (int mon)
{
    GList *model;
    output_mode_t *mode;

    // set the highest frequency for this mode
    model = mons[mon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (mons[mon].width == mode->width && mons[mon].height == mode->height)
        {
            mons[mon].freq = mode->freq;
            return;
        }
        model = model->next;
    }
}

static void set_resolution (GtkMenuItem *item, gpointer data)
{
    int w, h;
    gboolean i;
    int mon = (long) data;

    sscanf (gtk_menu_item_get_label (item), "%dx%d", &w, &h);
    if (strstr (gtk_menu_item_get_label (item), "i")) i = TRUE;
    else i = FALSE;
    if (w != mons[mon].width || h != mons[mon].height || i != mons[mon].interlaced)
    {
        mons[mon].width = w;
        mons[mon].height = h;
        mons[mon].interlaced = i;
        check_frequency (mon);
        mons[mon].scale = 1.0;
    }
    gtk_widget_queue_draw (da);
}

static void add_resolution (GtkWidget *menu, long mon, int width, int height, gboolean inter)
{
    char *label = g_strdup_printf ("%dx%d%s", width, height, inter ? "i" : "");
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].width == width && mons[mon].height == height && mons[mon].interlaced == inter);
    g_signal_connect (item, "activate", G_CALLBACK (set_resolution), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void set_frequency (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    sscanf (gtk_menu_item_get_label (item), "%fHz", &(mons[mon].freq));
}

static void add_frequency (GtkWidget *menu, long mon, float freq)
{
    char *label = g_strdup_printf ("%.3fHz", freq);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].freq == freq);
    g_signal_connect (item, "activate", G_CALLBACK (set_frequency), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void set_orientation (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    sscanf (gtk_widget_get_name (GTK_WIDGET (item)), "%d", &(mons[mon].rotation));
    gtk_widget_queue_draw (da);
}

static void add_orientation (GtkWidget *menu, long mon, const char *orient, int rotation)
{
    GtkWidget *item = gtk_check_menu_item_new_with_label (orient);
    char *tag = g_strdup_printf ("%d", rotation);
    gtk_widget_set_name (item, tag);
    g_free (tag);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].rotation == rotation);
    g_signal_connect (item, "activate", G_CALLBACK (set_orientation), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void set_scaling (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    sscanf (gtk_widget_get_name (GTK_WIDGET (item)), "%f", &(mons[mon].scale));
    gtk_widget_queue_draw (da);
}

static void add_scaling (GtkWidget *menu, long mon, float scaling)
{
    float multiplier;
    int wtest, htest;

    char *tag = g_strdup_printf ("%0.1f", scaling);
    GtkWidget *item = gtk_check_menu_item_new_with_label (tag);
    gtk_widget_set_name (item, tag);
    g_free (tag);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].scale == scaling);

    multiplier = ceil (scaling) / scaling;
    wtest = mons[mon].width * multiplier;
    htest = mons[mon].height * multiplier;
    if (wtest > TEXTURE_W || htest > TEXTURE_H)
    {
        gtk_widget_set_sensitive (item, FALSE);
        gtk_widget_set_tooltip_text (item, _("Fractional scalings cannot be used at this resolution"));
    }
    else g_signal_connect (item, "activate", G_CALLBACK (set_scaling), (gpointer) mon);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void set_enable (GtkCheckMenuItem *item, gpointer data)
{
    int mon = (long) data;
    mons[mon].enabled = gtk_check_menu_item_get_active (item);
    gtk_widget_queue_draw (da);
}

static void set_touchscreen (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    const char *ts = gtk_menu_item_get_label (item);
    int m;

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (m == mon)
        {
            mons[m].touchscreen = g_strdup (ts);
            if (wm == WM_LABWC) mons[m].tmode = MODE_MOUSEEMU;
        }
        else if (!g_strcmp0 (mons[m].touchscreen, ts))
        {
            g_free (mons[m].touchscreen);
            mons[m].touchscreen = NULL;
            mons[m].tmode = MODE_NONE;
        }
    }
}

static void set_brightness (GtkMenuItem *item, gpointer data)
{
    int mon = (long) data;
    int val;

    sscanf (gtk_menu_item_get_label (item), "%d%%", &val);
    set_backlight (mon, val);
}

static void set_primary (GtkCheckMenuItem *item, gpointer data)
{
    int mon = (long) data;
    int m;

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (m == mon) mons[m].primary = TRUE;
        else mons[m].primary = FALSE;
    }
}

static void set_mode_emu (GtkCheckMenuItem *item, gpointer data)
{
    int mon = (long) data;
    int m;

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (m == mon) mons[m].tmode = MODE_MOUSEEMU;
    }
}

static void set_mode_mt (GtkCheckMenuItem *item, gpointer data)
{
    int mon = (long) data;
    int m;

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (m == mon) mons[m].tmode = MODE_MULTITOUCH;
    }
}

/*----------------------------------------------------------------------------*/
/* Context menu */
/*----------------------------------------------------------------------------*/

static GtkWidget *create_menu (long mon)
{
    GList *model;
    GtkWidget *item, *menu, *rmenu, *fmenu, *omenu, *tmenu, *tmmenu, *bmenu, *smenu;
    int lastw, lasth, level;
    float lastf;
    output_mode_t *mode;
    gboolean lasti, show_f = FALSE;
    char *ts;

    menu = gtk_menu_new ();

    item = gtk_check_menu_item_new_with_label (_("Active"));
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].enabled);
    g_signal_connect (item, "activate", G_CALLBACK (set_enable), (gpointer) mon);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    if (!mons[mon].enabled)
    {
        gtk_widget_show_all (menu);
        return menu;
    }

    if (wm == WM_OPENBOX)
    {
        item = gtk_check_menu_item_new_with_label (_("Primary"));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].primary);
        g_signal_connect (item, "activate", G_CALLBACK (set_primary), (gpointer) mon);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    // resolution and frequency menus from mode data for this monitor
    rmenu = gtk_menu_new ();
    fmenu = gtk_menu_new ();

    lastw = 0;
    lasth = 0;
    lastf = 0.0;
    lasti = FALSE;
    model = mons[mon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (lastw != mode->width || lasth != mode->height || lasti != mode->interlaced)
        {
            add_resolution (rmenu, mon, mode->width, mode->height, mode->interlaced);
            lastw = mode->width;
            lasth = mode->height;
            lasti = mode->interlaced;
        }
        if (mode->width == mons[mon].width && mode->height == mons[mon].height && mode->interlaced == mons[mon].interlaced && lastf != mode->freq && mode->freq > 1.0)
        {
            add_frequency (fmenu, mon, mode->freq);
            lastf = mode->freq;
            show_f = TRUE;
        }
        model = model->next;
    }

    item = gtk_menu_item_new_with_label (_("Resolution"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), rmenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    if (show_f)
    {
        item = gtk_menu_item_new_with_label (_("Frequency"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), fmenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    // orientation menu - generic
    omenu = gtk_menu_new ();
    add_orientation (omenu, mon, _("Normal"), 0);
    add_orientation (omenu, mon, _("Left"), 90);
    add_orientation (omenu, mon, _("Inverted"), 180);
    add_orientation (omenu, mon, _("Right"), 270);
    item = gtk_menu_item_new_with_label (_("Orientation"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), omenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    if (wm != WM_OPENBOX)
    {
        smenu = gtk_menu_new ();
        add_scaling (smenu, mon, 1.0);
        add_scaling (smenu, mon, 1.5);
        add_scaling (smenu, mon, 2.0);
        add_scaling (smenu, mon, 3.0);
        item = gtk_menu_item_new_with_label (_("Scaling"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), smenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    if (touchscreens)
    {
        tmenu = gtk_menu_new ();
        if (mons[mon].tmode != MODE_NONE)
        {
            tmmenu = gtk_menu_new ();

            item = gtk_check_menu_item_new_with_label (_("Mouse Emulation"));
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].tmode == MODE_MOUSEEMU);
            g_signal_connect (item, "activate", G_CALLBACK (set_mode_emu), (gpointer) mon);
            gtk_menu_shell_append (GTK_MENU_SHELL (tmmenu), item);

            item = gtk_check_menu_item_new_with_label (_("Multitouch"));
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].tmode == MODE_MULTITOUCH);
            g_signal_connect (item, "activate", G_CALLBACK (set_mode_mt), (gpointer) mon);
            gtk_menu_shell_append (GTK_MENU_SHELL (tmmenu), item);

            item = gtk_menu_item_new_with_label (_("Mode"));
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), tmmenu);
            gtk_menu_shell_append (GTK_MENU_SHELL (tmenu), item);

            item = gtk_separator_menu_item_new ();
            gtk_menu_shell_append (GTK_MENU_SHELL (tmenu), item);
        }
        model = touchscreens;
        while (model)
        {
            ts = (char *) model->data;
            item = gtk_check_menu_item_new_with_label (ts);
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), !g_strcmp0 (mons[mon].touchscreen, ts));
            g_signal_connect (item, "activate", G_CALLBACK (set_touchscreen), (gpointer) mon);
            gtk_menu_shell_append (GTK_MENU_SHELL (tmenu), item);
            model = model->next;
        }
        item = gtk_menu_item_new_with_label (_("Touchscreen"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), tmenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    if (mons[mon].backlight)
    {
        bmenu = gtk_menu_new ();
        level = 100;
        while (level > -10)
        {
            ts = g_strdup_printf ("%d%%", level);
            item = gtk_check_menu_item_new_with_label (ts);
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), get_backlight (mon) == level);
            g_signal_connect (item, "activate", G_CALLBACK (set_brightness), (gpointer) mon);
            gtk_menu_shell_append (GTK_MENU_SHELL (bmenu), item);
            level -= 10;
        }
        item = gtk_menu_item_new_with_label (_("Brightness"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), bmenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_widget_show_all (menu);
    return menu;
}

/*----------------------------------------------------------------------------*/
/* Pop-up menu */
/*----------------------------------------------------------------------------*/

static GtkWidget *create_popup (void)
{
    GtkWidget *item, *menu, *pmenu;
    GList *list, *l;
    int m, count;

    menu = gtk_menu_new ();

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        item = gtk_menu_item_new_with_label (mons[m].name);
        pmenu = create_menu (m);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), pmenu);

        count = 0;
        list = gtk_container_get_children (GTK_CONTAINER (menu));
        for (l = list; l; l = l->next)
        {
            if (g_strcmp0 (mons[m].name, gtk_menu_item_get_label (GTK_MENU_ITEM (l->data))) < 0) break;
            count++;
        }
        gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, count);
        g_list_free (list);
    }
    gtk_widget_show_all (menu);
    return menu;
}

/*----------------------------------------------------------------------------*/
/* Confirmation / reversion */
/*----------------------------------------------------------------------------*/

static void set_timer_msg (void)
{
    char *msg;
    msg = g_strdup_printf (_("Screen updated. Click 'OK' if is this is correct, or 'Cancel' to revert to previous setting.\n\nReverting in %d seconds..."), rev_time);
    gtk_label_set_text (GTK_LABEL (clbl), msg);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (cpb), (10.0 - (float) rev_time) / 10.0);
    g_free (msg);
}

static void handle_cancel (GtkButton *, gpointer)
{
    g_source_remove (tid);
    handle_undo (NULL, NULL);
    gtk_widget_destroy (conf);
}

static void handle_ok (GtkButton *, gpointer)
{
    g_source_remove (tid);
    gtk_widget_destroy (conf);
}

static gboolean revert_timeout (gpointer)
{
    rev_time--;
    if (rev_time > 0)
    {
        set_timer_msg ();
        return TRUE;
    }
    else
    {
        handle_cancel (NULL, NULL);
        return FALSE;
    }
}

static void show_confirm_dialog (void)
{
    GtkBuilder *builder;

    textdomain (GETTEXT_PACKAGE);
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    conf = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    gtk_window_set_transient_for (GTK_WINDOW (conf), GTK_WINDOW (main_dlg));
    clbl = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    cpb = (GtkWidget *) gtk_builder_get_object (builder, "modal_pb");
    g_signal_connect (gtk_builder_get_object (builder, "modal_ok"), "clicked", G_CALLBACK (handle_ok), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "modal_cancel"), "clicked", G_CALLBACK (handle_cancel), NULL);
    gtk_widget_show (conf);
    g_object_unref (builder);

    rev_time = 10;
    set_timer_msg ();
    tid = g_timeout_add (1000, (GSourceFunc) revert_timeout, NULL);
}

/*----------------------------------------------------------------------------*/
/* Touchscreens */
/*----------------------------------------------------------------------------*/

static void find_touchscreens (void)
{
    FILE *fp;
    char *line, *ts;
    size_t len;

    touchscreens = NULL;

    fp = popen ("libinput list-devices | tr \\\\n @ | sed 's/@@/\\\n/g' | grep \"Capabilities:.*touch\" | sed 's/Device:[ \\\t]*//' | cut -d @ -f 1", "r");
    if (fp)
    {
        line = NULL;
        len = 0;
        while (getline (&line, &len, fp) != -1)
        {
            ts = g_strdup (line);
            ts[strlen(ts) - 1] = 0;
            touchscreens = g_list_append (touchscreens, ts);
        }
        free (line);
        pclose (fp);
    }
}

/*----------------------------------------------------------------------------*/
/* Backlights */
/*----------------------------------------------------------------------------*/

static void find_backlights (void)
{
    DIR *dir;
    struct dirent *entry;
    FILE *fp;
    char *filename;
    char buffer[32];
    int m;

    if ((dir = opendir ("/sys/class/backlight")))
    {
        while ((entry = readdir (dir)))
        {
            if (entry->d_name[0] != '.')
            {
                filename = g_build_filename ("/sys/class/backlight", entry->d_name, "display_name", NULL);
                if ((fp = fopen (filename, "r")))
                {
                    if (fscanf (fp, "%s", buffer) == 1)
                    {
                        for (m = 0; m < MAX_MONS; m++)
                        {
                            if (mons[m].modes == NULL) continue;
                            if (!g_strcmp0 (mons[m].name, buffer))
                            {
                                mons[m].backlight = g_strdup (entry->d_name);
                            }
                        }
                    }
                    fclose (fp);
                }
                g_free (filename);
            }
        }
        closedir (dir);
    }
}

static int get_backlight (int mon)
{
    char *filename;
    int level, max;
    FILE *fp;

    filename = g_build_filename ("/sys/class/backlight", mons[mon].backlight, "brightness", NULL);
    if ((fp = fopen (filename, "r")))
    {
        if (fscanf (fp, "%d", &level) != 1) level = -1;
        fclose (fp);
    }
    g_free (filename);

    filename = g_build_filename ("/sys/class/backlight", mons[mon].backlight, "max_brightness", NULL);
    if ((fp = fopen (filename, "r")))
    {
        if (fscanf (fp, "%d", &max) != 1) max = -1;
        fclose (fp);
    }
    g_free (filename);

    if (level == -1 || max == -1) return -1;

    return 10 * ((level * 100 / max) / 10);
}

static void set_backlight (int mon, int level)
{
    char *filename;
    int max;
    FILE *fp;

    filename = g_build_filename ("/sys/class/backlight", mons[mon].backlight, "max_brightness", NULL);
    if ((fp = fopen (filename, "r")))
    {
        if (fscanf (fp, "%d", &max) != 1) max = -1;
        fclose (fp);
    }
    g_free (filename);

    filename = g_build_filename ("/sys/class/backlight", mons[mon].backlight, "brightness", NULL);
    if ((fp = fopen (filename, "w")))
    {
        fprintf (fp, "%d", ((level * max) + 50) / 100);
        fclose (fp);
    }
    g_free (filename);
}

/*----------------------------------------------------------------------------*/
/* Event handlers */
/*----------------------------------------------------------------------------*/

static gboolean button_press_event (GtkWidget *, GdkEventButton *ev, gpointer)
{
    GtkWidget *menu;
    int m;

    curmon = -1;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

        if (ev->x > SCALE(mons[m].x) && ev->x < SCALE(mons[m].x + screen_w (mons[m]))
            && ev->y > SCALE(mons[m].y) && ev->y < SCALE(mons[m].y + screen_h (mons[m])))
        {
            curmon = m;
            mousex = ev->x - SCALE(mons[m].x);
            mousey = ev->y - SCALE(mons[m].y);
        }
    }

    if (ev->button == 3 && curmon != -1)
    {
        menu = create_menu (curmon);
        curmon = -1;
        gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
    }
    return TRUE;
}

static gboolean motion_notify_event (GtkWidget *da, GdkEventMotion *ev, gpointer)
{
    int m, xs, ys;

    if (curmon != -1)
    {
        mons[curmon].x = UPSCALE(ev->x - mousex);
        mons[curmon].y = UPSCALE(ev->y - mousey);

        // constrain to screen
        if (mons[curmon].x < 0) mons[curmon].x = 0;
        if (mons[curmon].y < 0) mons[curmon].y = 0;

        // snap top and left to other windows bottom or right, or to 0,0
        for (m = 0; m < MAX_MONS; m++)
        {
            if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

            xs = m != curmon ? mons[m].x + screen_w (mons[m]) : 0;
            ys = m != curmon ? mons[m].y + screen_h (mons[m]) : 0;
            if (mons[curmon].x > xs - SNAP_DISTANCE && mons[curmon].x < xs + SNAP_DISTANCE) mons[curmon].x = xs;
            if (mons[curmon].y > ys - SNAP_DISTANCE && mons[curmon].y < ys + SNAP_DISTANCE) mons[curmon].y = ys;
        }

        gtk_widget_queue_draw (da);
    }
    return FALSE;
}

static gboolean button_release_event (GtkWidget *, GdkEventButton *, gpointer)
{
    curmon = -1;
    return TRUE;
}

static gboolean scroll (GtkWidget *, GdkEventScroll *ev, gpointer)
{
    if (ev->direction == 0) handle_zoom (NULL, (gpointer) 1);
    if (ev->direction == 1) handle_zoom (NULL, (gpointer) -1);
    return TRUE;
}

static void gesture_pressed (GtkGestureLongPress *, gdouble x, gdouble y, gpointer)
{
    pressed = TRUE;
    press_x = x;
    press_y = y;
}

static void gesture_end (GtkGestureLongPress *, GdkEventSequence *, gpointer)
{
    GtkWidget *menu;
    int m;

    if (pressed)
    {
        curmon = -1;
        for (m = 0; m < MAX_MONS; m++)
        {
            if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

            if (press_x > SCALE(mons[m].x) && press_x < SCALE(mons[m].x + screen_w (mons[m]))
                && press_y > SCALE(mons[m].y) && press_y < SCALE(mons[m].y + screen_h (mons[m])))
            {
                curmon = m;
                mousex = press_x - SCALE(mons[m].x);
                mousey = press_y - SCALE(mons[m].y);
            }
        }

        if (curmon != -1)
        {
            menu = create_menu (curmon);
            curmon = -1;
            GdkRectangle rect = {press_x, press_y, 0, 0};
            gtk_menu_popup_at_rect (GTK_MENU (menu), gtk_widget_get_window (da), &rect, GDK_GRAVITY_CENTER, GDK_GRAVITY_NORTH_WEST, NULL);
        }
    }
    pressed = FALSE;
}

static void handle_apply (GtkButton *, gpointer)
{
    if (compare_config (mons, bmons)) return;

    wm_fn.save_config ();
    wm_fn.save_touchscreens ();

    wm_fn.reload_config ();
    wm_fn.reload_touchscreens ();

    clear_config (FALSE);

    wm_fn.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    wm_fn.load_touchscreens ();

    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, TRUE);
    show_confirm_dialog ();
}

static void handle_undo (GtkButton *, gpointer)
{
    wm_fn.revert_config ();
    wm_fn.revert_touchscreens ();

    wm_fn.reload_config ();
    wm_fn.reload_touchscreens ();

    clear_config (FALSE);

    wm_fn.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    wm_fn.load_touchscreens ();

    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, FALSE);
}

static void handle_zoom (GtkButton *, gpointer data)
{
    if ((long) data == -1 && scale < 16) scale *= 2;
    if ((long) data == 1 && scale > 4) scale /= 2;
    gtk_widget_set_sensitive (zin, scale != 4);
    gtk_widget_set_sensitive (zout, scale != 16);
    gtk_widget_queue_draw (da);
}

static void handle_menu (GtkButton *btn, gpointer)
{
    gtk_menu_popup_at_widget (GTK_MENU (create_popup ()), GTK_WIDGET (btn),
        GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, NULL);
}

static gboolean hide_ids (gpointer)
{
    int m;
    gtk_widget_set_sensitive (ident, TRUE);
    for (m = 0; m < MAX_MONS; m++)
    {
        if (id[m]) gtk_widget_destroy (id[m]);
        id[m] = NULL;
    }
    return FALSE;
}

static void identify_monitors (void)
{
    GdkDisplay *disp = gdk_display_get_default ();
    GdkMonitor *mon;
    PangoFontDescription *fd;
    PangoAttribute *attr;
    PangoAttrList *attrs;
    int m, n;

    for (m = 0; m < MAX_MONS; m++)
    {
        id[m] = NULL;
        if (mons[m].modes == NULL) continue;
        if (mons[m].enabled)
        {
            id[m] = gtk_window_new (GTK_WINDOW_TOPLEVEL);
            lbl[m] = gtk_label_new (mons[m].name);
            gtk_window_set_decorated (GTK_WINDOW (id[m]), FALSE);
            gtk_window_set_skip_taskbar_hint (GTK_WINDOW (id[m]), TRUE);
            gtk_window_set_skip_pager_hint (GTK_WINDOW (id[m]), TRUE);

            if (gtk_layer_is_supported ()) gtk_layer_init_for_window (GTK_WINDOW (id[m]));

            for (n = 0; n < gdk_display_get_n_monitors (disp); n++)
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                char *buf = gdk_screen_get_monitor_plug_name (gdk_display_get_default_screen (disp), n);
#pragma GCC diagnostic pop
                if (!g_strcmp0 (mons[m].name, buf))
                {
                    GdkRectangle geom;
                    mon = gdk_display_get_monitor (disp, n);
                    gdk_monitor_get_geometry (mon, &geom);

                    fd = pango_font_description_from_string ("sans");
                    pango_font_description_set_size (fd, PANGO_SCALE * geom.width / 60);
                    attr = pango_attr_font_desc_new (fd);
                    attrs = pango_attr_list_new ();
                    pango_attr_list_insert (attrs, attr);
                    gtk_label_set_attributes (GTK_LABEL (lbl[m]), attrs);

                    gtk_container_add (GTK_CONTAINER (id[m]), lbl[m]);
                    gtk_widget_show_all (id[m]);

                    if (gtk_layer_is_supported ()) gtk_layer_set_monitor (GTK_WINDOW (id[m]), mon);
                    else
                    {
                        int w, h;

                        gtk_window_get_size (GTK_WINDOW (id[m]), &w, &h);
                        gtk_window_move (GTK_WINDOW (id[m]), geom.x + geom.width / 2 - w / 2, geom.y + geom.height / 2 - h / 2);
                    }
                    gtk_window_present (GTK_WINDOW (id[m]));

                    pango_attr_list_unref (attrs);
                    pango_font_description_free (fd);
                }
                g_free (buf);
            }
        }
    }

    gtk_widget_set_sensitive (ident, FALSE);
    g_timeout_add (1000, G_SOURCE_FUNC (hide_ids), NULL);
}

static void handle_ident (GtkButton *, gpointer)
{
    identify_monitors ();
}

/*----------------------------------------------------------------------------*/
/* Load / save config */
/*----------------------------------------------------------------------------*/

static void load_scale (void)
{
    char *conffile;
    GKeyFile *kf;
    GError *err;
    int val;

    scale = 8;

    conffile = g_build_filename (g_get_user_config_dir (), "rpcc", "config.ini", NULL);
    kf = g_key_file_new ();
    if (g_key_file_load_from_file (kf, conffile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
    {
        err = NULL;
        val = g_key_file_get_integer (kf, "raindrop", "scale", &err);
        if (err == NULL && (val == 4 || val == 16)) scale = val;
        else scale = 8;
    }

    g_key_file_free (kf);
    g_free (conffile);
}

static void save_scale (void)
{
    char *conffile, *str;
    GKeyFile *kf;
    gsize len;

    conffile = g_build_filename (g_get_user_config_dir (), "rpcc", "config.ini", NULL);

    str = g_path_get_dirname (conffile);
    g_mkdir_with_parents (str, S_IRUSR | S_IWUSR | S_IXUSR);
    g_free (str);

    kf = g_key_file_new ();
    g_key_file_load_from_file (kf, conffile, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

    g_key_file_set_integer (kf, "raindrop", "scale", scale);

    str = g_key_file_to_data (kf, &len, NULL);
    g_file_set_contents (conffile, str, len, NULL);
    g_free (str);

    g_key_file_free (kf);
    g_free (conffile);
}

/*----------------------------------------------------------------------------*/
/* Initial configuration                                                      */
/*----------------------------------------------------------------------------*/

static void init_config (void)
{
    clear_config (TRUE);

    find_touchscreens ();

    wm_fn.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    wm_fn.load_touchscreens ();

    // ensure the config file reflects the current state, or undo won't work...
    wm_fn.save_config ();

    curmon = -1;
    da = (GtkWidget *) gtk_builder_get_object (builder, "da");
    gtk_widget_set_events (da, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);
    g_signal_connect (da, "draw", G_CALLBACK (draw), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (button_press_event), NULL);
    g_signal_connect (da, "button-release-event", G_CALLBACK (button_release_event), NULL);
    g_signal_connect (da, "motion-notify-event", G_CALLBACK (motion_notify_event), NULL);
    g_signal_connect (da, "scroll-event", G_CALLBACK (scroll), NULL);

    undo = (GtkWidget *) gtk_builder_get_object (builder, "btn_undo");
    g_signal_connect (undo, "clicked", G_CALLBACK (handle_undo), NULL);
    gtk_widget_set_sensitive (undo, FALSE);

    zin = (GtkWidget *) gtk_builder_get_object (builder, "btn_zin");
    zout = (GtkWidget *) gtk_builder_get_object (builder, "btn_zout");
    overlay = (GtkWidget *) gtk_builder_get_object (builder, "overlay");
    zooms = (GtkWidget *) gtk_builder_get_object (builder, "zooms");
    g_signal_connect (zin, "clicked", G_CALLBACK (handle_zoom), (gpointer) 1);
    g_signal_connect (zout, "clicked", G_CALLBACK (handle_zoom), (gpointer) -1);
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), zooms);

    g_signal_connect (gtk_builder_get_object (builder, "btn_apply"), "clicked", G_CALLBACK (handle_apply), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_menu"), "clicked", G_CALLBACK (handle_menu), NULL);
    ident = (GtkWidget *) gtk_builder_get_object (builder, "btn_ident");
    g_signal_connect (ident, "clicked", G_CALLBACK (handle_ident), NULL);

    /* Set up long press */
    GtkGesture *gesture = gtk_gesture_long_press_new (da);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
    g_signal_connect (gesture, "pressed", G_CALLBACK (gesture_pressed), NULL);
    g_signal_connect (gesture, "end", G_CALLBACK (gesture_end), NULL);
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_TARGET);
    pressed = FALSE;
}

/*----------------------------------------------------------------------------*/
/* Plugin interface */
/*----------------------------------------------------------------------------*/

#ifdef PLUGIN_NAME

void init_plugin (GtkWidget *parent)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE"))
        {
            wm = WM_WAYFIRE;
            wm_fn = wayfire_functions;
        }
        else
        {
            wm = WM_LABWC;
            wm_fn = labwc_functions;
        }
    }
    else
    {
        wm = WM_OPENBOX;
        wm_fn = openbox_functions;
    }

    main_dlg = parent;
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    load_scale ();

    init_config ();

    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "btn_close")));
}

int plugin_tabs (void)
{
    return 1;
}

const char *tab_name (int tab)
{
    return C_("tab", "Screens");
}

const char *icon_name (int tab)
{
    return "video-display";
}

const char *tab_id (int tab)
{
    return NULL;
}

GtkWidget *get_tab (int tab)
{
    GtkWidget *window, *plugin;

    window = (GtkWidget *) gtk_builder_get_object (builder, "main_win");
    plugin = (GtkWidget *) gtk_builder_get_object (builder, "raindrop_page");

    gtk_container_remove (GTK_CONTAINER (window), plugin);

    return plugin;
}

gboolean reboot_needed (void)
{
    save_scale ();

    if (gtk_widget_get_sensitive (undo)) wm_fn.update_system_config ();
    // note - if you change a touchscreen under wayfire you do need to reboot, but ...
    return FALSE;
}

void free_plugin (void)
{
    g_object_unref (builder);
}

#else

/*----------------------------------------------------------------------------*/
/* Main window button handlers                                                */
/*----------------------------------------------------------------------------*/

static void handle_close (GtkButton *, gpointer)
{
    if (gtk_widget_get_sensitive (undo)) wm_fn.update_system_config ();
    gtk_main_quit ();
}

static void close_prog (GtkWidget *, GdkEvent *, gpointer)
{
    if (gtk_widget_get_sensitive (undo)) wm_fn.update_system_config ();
    gtk_main_quit ();
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        if (getenv ("WAYFIRE_CONFIG_FILE"))
        {
            wm = WM_WAYFIRE;
            wm_fn = wayfire_functions;
        }
        else
        {
            wm = WM_LABWC;
            wm_fn = labwc_functions;
        }
    }
    else
    {
        wm = WM_OPENBOX;
        wm_fn = openbox_functions;
    }

    gtk_init (&argc, &argv);

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    main_dlg = (GtkWidget *) gtk_builder_get_object (builder, "main_win");
    g_signal_connect (main_dlg, "delete-event", G_CALLBACK (close_prog), NULL);

    g_signal_connect (gtk_builder_get_object (builder, "btn_close"), "clicked", G_CALLBACK (handle_close), NULL);

    gtk_window_set_default_size (GTK_WINDOW (main_dlg), 500, 400);

    scale = 8;

    init_config ();

    g_object_unref (builder);

    gtk_widget_show_all (main_dlg);

    gtk_main ();

    return 0;
}

#endif

/* End of file */
/*============================================================================*/
