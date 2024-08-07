/*============================================================================
Copyright (c) 2024 Raspberry Pi Holdings Ltd.
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "raindrop.h"

extern wm_functions_t labwc_functions;
extern wm_functions_t openbox_functions;

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

#define SNAP_DISTANCE 200

#define SCALE(n) ((n) / scale)
#define UPSCALE(n) ((n) * scale)

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

monitor_t mons[MAX_MONS], bmons[MAX_MONS];
int mousex, mousey;
int screenw, screenh;
int curmon;
int scale = 8;
int rev_time;
int tid;
GtkWidget *da, *win, *undo, *zin, *zout, *conf, *clbl, *cpb;
GList *touchscreens;
gboolean use_x;
wm_functions_t touse;

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
static void set_enable (GtkCheckMenuItem *item, gpointer data);
static void set_touchscreen (GtkMenuItem *item, gpointer data);
static void set_brightness (GtkMenuItem *item, gpointer data);
static void set_primary (GtkCheckMenuItem *item, gpointer data);
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
static void button_press_event (GtkWidget *, GdkEventButton ev, gpointer);
static gboolean motion_notify_event (GtkWidget *da, GdkEventMotion ev, gpointer);
static void button_release_event (GtkWidget *, GdkEventButton, gpointer);
static void handle_close (GtkButton *, gpointer);
static void handle_apply (GtkButton *, gpointer);
static void handle_undo (GtkButton *, gpointer);
static void handle_zoom (GtkButton *, gpointer data);
static void handle_menu (GtkButton *btn, gpointer);
static void end_program (GtkWidget *, GdkEvent *, gpointer);

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

static void draw (GtkDrawingArea *, cairo_t *cr, gpointer)
{
    PangoLayout *layout;
    PangoFontDescription *font;
    int m, w, h, charwid;

    GdkRGBA bg = { 0.25, 0.25, 0.25, 1.0 };
    GdkRGBA fg = { 1.0, 1.0, 1.0, 0.75 };
    GdkRGBA bk = { 0.0, 0.0, 0.0, 1.0 };

    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (win));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (win));

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
        if (m == mon) mons[m].touchscreen = g_strdup (ts);
        else if (!g_strcmp0 (mons[m].touchscreen, ts))
        {
            g_free (mons[m].touchscreen);
            mons[m].touchscreen = NULL;
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

/*----------------------------------------------------------------------------*/
/* Context menu */
/*----------------------------------------------------------------------------*/

static GtkWidget *create_menu (long mon)
{
    GList *model;
    GtkWidget *item, *menu, *rmenu, *fmenu, *omenu, *tmenu, *bmenu;
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

    if (use_x)
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

    if (touchscreens)
    {
        tmenu = gtk_menu_new ();
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
        tid = 0;
        handle_cancel (NULL, NULL);
        return FALSE;
    }
}

static void show_confirm_dialog (void)
{
    GtkBuilder *builder;

    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    conf = (GtkWidget *) gtk_builder_get_object (builder, "modal");
    gtk_window_set_transient_for (GTK_WINDOW (conf), GTK_WINDOW (win));
    clbl = (GtkWidget *) gtk_builder_get_object (builder, "modal_msg");
    cpb = (GtkWidget *) gtk_builder_get_object (builder, "modal_pb");
    g_signal_connect (gtk_builder_get_object (builder, "modal_ok"), "clicked", G_CALLBACK (handle_ok), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "modal_cancel"), "clicked", G_CALLBACK (handle_cancel), NULL);
    gtk_widget_show_all (conf);
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

static void button_press_event (GtkWidget *, GdkEventButton ev, gpointer)
{
    GtkWidget *menu;
    int m;

    curmon = -1;
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL || mons[m].enabled == FALSE) continue;

        if (ev.x > SCALE(mons[m].x) && ev.x < SCALE(mons[m].x + screen_w (mons[m]))
            && ev.y > SCALE(mons[m].y) && ev.y < SCALE(mons[m].y + screen_h (mons[m])))
        {
            curmon = m;
            mousex = ev.x - SCALE(mons[m].x);
            mousey = ev.y - SCALE(mons[m].y);
        }
    }

    if (ev.button == 3)
    {
        menu = create_menu (curmon);
        curmon = -1;
        gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
    }
}

static gboolean motion_notify_event (GtkWidget *da, GdkEventMotion ev, gpointer)
{
    int m, xs, ys;

    if (curmon != -1)
    {
        mons[curmon].x = UPSCALE(ev.x - mousex);
        mons[curmon].y = UPSCALE(ev.y - mousey);

        // constrain to screen
        if (mons[curmon].x < 0) mons[curmon].x = 0;
        if (mons[curmon].y < 0) mons[curmon].y = 0;

        // snap top and left to other windows bottom or right, or to 0,0
        for (m = 0; m < MAX_MONS; m++)
        {
            if (mons[m].modes == NULL) continue;

            xs = m != curmon ? mons[m].x + screen_w (mons[m]) : 0;
            ys = m != curmon ? mons[m].y + screen_h (mons[m]) : 0;
            if (mons[curmon].x > xs - SNAP_DISTANCE && mons[curmon].x < xs + SNAP_DISTANCE) mons[curmon].x = xs;
            if (mons[curmon].y > ys - SNAP_DISTANCE && mons[curmon].y < ys + SNAP_DISTANCE) mons[curmon].y = ys;
        }

        gtk_widget_queue_draw (da);
    }
    return FALSE;
}

static void button_release_event (GtkWidget *, GdkEventButton, gpointer)
{
    curmon = -1;
}

static void handle_close (GtkButton *, gpointer)
{
    if (gtk_widget_get_sensitive (undo)) touse.update_system_config ();
    gtk_main_quit ();
}

static void handle_apply (GtkButton *, gpointer)
{
    if (compare_config (mons, bmons)) return;

    touse.save_config ();
    touse.save_touchscreens ();

    touse.reload_config ();
    touse.reload_touchscreens ();

    clear_config (FALSE);

    touse.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    touse.load_touchscreens ();

    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, TRUE);
    show_confirm_dialog ();
}

static void handle_undo (GtkButton *, gpointer)
{
    touse.revert_config ();
    touse.revert_touchscreens ();

    touse.reload_config ();
    touse.reload_touchscreens ();

    clear_config (FALSE);

    touse.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    touse.load_touchscreens ();

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

static void end_program (GtkWidget *, GdkEvent *, gpointer)
{
    if (gtk_widget_get_sensitive (undo)) touse.update_system_config ();
    gtk_main_quit ();
}

/*----------------------------------------------------------------------------*/
/* Main function */
/*----------------------------------------------------------------------------*/

int main (int argc, char *argv[])
{
    GtkBuilder *builder;

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    if (getenv ("WAYLAND_DISPLAY"))
    {
        use_x = FALSE;
        touse = labwc_functions;
    }
    else
    {
        use_x = TRUE;
        touse = openbox_functions;
    }

    clear_config (TRUE);

    find_touchscreens ();

    touse.load_config ();

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);

    touse.load_touchscreens ();

    // ensure the config file reflects the current state, or undo won't work...
    touse.save_config ();

    gtk_init (&argc, &argv);

    // build the UI
    builder = gtk_builder_new_from_file (PACKAGE_DATA_DIR "/ui/raindrop.ui");

    win = (GtkWidget *) gtk_builder_get_object (builder, "main_win");
    g_signal_connect (win, "delete-event", G_CALLBACK (end_program), NULL);

    curmon = -1;
    da = (GtkWidget *) gtk_builder_get_object (builder, "da");
    gtk_widget_set_events (da, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    g_signal_connect (da, "draw", G_CALLBACK (draw), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (button_press_event), NULL);
    g_signal_connect (da, "button-release-event", G_CALLBACK (button_release_event), NULL);
    g_signal_connect (da, "motion-notify-event", G_CALLBACK (motion_notify_event), NULL);
    gtk_window_set_default_size (GTK_WINDOW (win), 500, 400);

    undo = (GtkWidget *) gtk_builder_get_object (builder, "btn_undo");
    g_signal_connect (undo, "clicked", G_CALLBACK (handle_undo), NULL);
    gtk_widget_set_sensitive (undo, FALSE);

    zin = (GtkWidget *) gtk_builder_get_object (builder, "btn_zin");
    zout = (GtkWidget *) gtk_builder_get_object (builder, "btn_zout");
    g_signal_connect (zin, "clicked", G_CALLBACK (handle_zoom), (gpointer) 1);
    g_signal_connect (zout, "clicked", G_CALLBACK (handle_zoom), (gpointer) -1);

    g_signal_connect (gtk_builder_get_object (builder, "btn_close"), "clicked", G_CALLBACK (handle_close), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_apply"), "clicked", G_CALLBACK (handle_apply), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_menu"), "clicked", G_CALLBACK (handle_menu), NULL);

    gtk_widget_show_all (win);

    gtk_main ();
    return 0;
}

/* End of file */
/*============================================================================*/
