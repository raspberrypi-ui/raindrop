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

/****************************************************************************
 * TODO
 * X support
 ****************************************************************************/

#include <locale.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libxml/xpath.h>

/*----------------------------------------------------------------------------*/
/* Typedefs and macros */
/*----------------------------------------------------------------------------*/

typedef struct {
    int width;
    int height;
    float freq;
} output_mode_t;

typedef struct {
    char *name;
    gboolean enabled;
    int width;
    int height;
    int x;
    int y;
    int rotation;
    float freq;
    GList *modes;
    char *touchscreen;
    char *backlight;
} monitor_t;

#define MAX_MONS 10

#define SNAP_DISTANCE 200

#define SCALE(n) ((n) / scale)
#define UPSCALE(n) ((n) * scale)

#define SUDO_PREFIX "env SUDO_ASKPASS=/usr/share/raindrop/pwdraindrop.sh sudo -A "

#define load_config() {if (use_x) load_openbox_config (); else load_labwc_config ();}
#define load_touchscreens() {if (use_x) load_openbox_touchscreens (); else load_labwc_touchscreens ();}
#define save_config() {if (use_x) save_openbox_config (); else save_labwc_config ();}
#define save_touchscreens() {if (!use_x) save_labwc_touchscreens ();}
#define reload_config() {if (use_x) reload_openbox_config (); else reload_labwc_config ();}
#define reload_touchscreens() {if (!use_x) reload_labwc_touchscreens ();}
#define revert_config() {if (use_x) revert_openbox_config (); else revert_labwc_config ();}
#define revert_touchscreens() {if (!use_x) revert_labwc_touchscreens ();}
#define update_system_config() {if (use_x) update_openbox_system_config (); else update_labwc_system_config ();}

/*----------------------------------------------------------------------------*/
/* Global data */
/*----------------------------------------------------------------------------*/

const char *orients[4] = { "normal", "90", "180", "270" };
const char *xorients[4] = { "normal", "left", "inverted", "right" };

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
static void update_labwc_system_config (void);
static void update_openbox_system_config (void);
static void draw (GtkDrawingArea *, cairo_t *cr, gpointer);
static void check_frequency (int mon);
static void set_resolution (GtkMenuItem *item, gpointer data);
static void add_resolution (GtkWidget *menu, long mon, int width, int height);
static void set_frequency (GtkMenuItem *item, gpointer data);
static void add_frequency (GtkWidget *menu, long mon, float freq);
static void set_orientation (GtkMenuItem *item, gpointer data);
static void add_orientation (GtkWidget *menu, long mon, const char *orient, int rotation);
static void set_enable (GtkCheckMenuItem *item, gpointer data);
static void set_touchscreen (GtkMenuItem *item, gpointer data);
static void set_brightness (GtkMenuItem *item, gpointer data);
static GtkWidget *create_menu (long mon);
static GtkWidget *create_popup (void);
static void add_mode (int monitor, int w, int h, float f);
static void load_labwc_config (void);
static void load_openbox_config (void);
static gboolean copy_profile (FILE *fp, FILE *foutp, int nmons);
static int write_config (FILE *fp);
static void merge_configs (const char *infile, const char *outfile);
static void save_labwc_config (void);
static void write_dispsetup (const char *infile);
static void save_openbox_config (void);
static void reload_labwc_config (void);
static void reload_openbox_config (void);
static void revert_labwc_config (void);
static void revert_openbox_config (void);
static void set_timer_msg (void);
static void handle_cancel (GtkButton *, gpointer);
static void handle_ok (GtkButton *, gpointer);
static gboolean revert_timeout (gpointer data);
static void show_confirm_dialog (void);
static void find_touchscreens (void);
static void load_labwc_touchscreens (void);
static void load_openbox_touchscreens (void);
static void write_touchscreens (char *filename);
static void save_labwc_touchscreens (void);
static void reload_labwc_touchscreens (void);
static void revert_labwc_touchscreens (void);
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
    }
}

static gint mode_compare (gconstpointer a, gconstpointer b)
{
    output_mode_t *moda = (output_mode_t *) a;
    output_mode_t *modb = (output_mode_t *) b;

    if (moda->width > modb->width) return -1;
    if (moda->width < modb->width) return 1;
    if (moda->height > modb->height) return-1;
    if (moda->height < modb->height) return 1;
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

static void update_labwc_system_config (void)
{
    char *cmd;

    system (SUDO_PREFIX "mkdir -p /usr/share/labwc/");

    cmd = g_strdup_printf (SUDO_PREFIX "cp %s/kanshi/config /usr/share/labwc/config.kanshi",
        g_get_user_config_dir ());
    system (cmd);
    g_free (cmd);

    cmd = g_strdup_printf (SUDO_PREFIX "cp %s/labwc/rcgreeter.xml /usr/share/labwc/rc.xml",
        g_get_user_config_dir ());
    system (cmd);
    g_free (cmd);
}

static void update_openbox_system_config (void)
{
    char *cmd;

    cmd = g_strdup_printf (SUDO_PREFIX "cp /var/tmp/dispsetup.sh /usr/share/dispsetup.sh");
    system (cmd);
    g_free (cmd);
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
    int mon = (long) data;

    sscanf (gtk_menu_item_get_label (item), "%dx%d", &w, &h);
    if (w != mons[mon].width || h != mons[mon].height)
    {
        mons[mon].width = w;
        mons[mon].height = h;
        check_frequency (mon);
    }
    gtk_widget_queue_draw (da);
}

static void add_resolution (GtkWidget *menu, long mon, int width, int height)
{
    char *label = g_strdup_printf ("%dx%d", width, height);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[mon].width == width && mons[mon].height == height);
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
        if (m == mon) mons[m].touchscreen = g_strdup_printf (ts);
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
    gboolean show_f = FALSE;
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

    // resolution and frequency menus from mode data for this monitor
    rmenu = gtk_menu_new ();
    fmenu = gtk_menu_new ();

    lastw = 0;
    lasth = 0;
    lastf = 0.0;
    model = mons[mon].modes;
    while (model)
    {
        mode = (output_mode_t *) model->data;
        if (lastw != mode->width || lasth != mode->height)
        {
            add_resolution (rmenu, mon, mode->width, mode->height);
            lastw = mode->width;
            lasth = mode->height;
        }
        if (mode->width == mons[mon].width && mode->height == mons[mon].height && lastf != mode->freq && mode->freq > 1.0)
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
/* Loading initial config */
/*----------------------------------------------------------------------------*/

static void add_mode (int monitor, int w, int h, float f)
{
    output_mode_t *mod;
    mod = g_new0 (output_mode_t, 1);
    mod->width = w;
    mod->height = h;
    mod->freq = f;
    mons[monitor].modes = g_list_append (mons[monitor].modes, mod);
}

static void load_labwc_config (void)
{
    FILE *fp;
    char *line, *cptr;
    size_t len;
    int mon, w, h, i;
    float f;

    clear_config (FALSE);

    mon = -1;

    fp = popen ("wlr-randr", "r");
    if (fp)
    {
        line = NULL;
        len = 0;
        while (getline (&line, &len, fp) != -1)
        {
            if (line[0] != ' ')
            {
                mon++;
                cptr = line;
                while (*cptr != ' ') cptr++;
                *cptr = 0;
                mons[mon].name = g_strdup (line);
                if (strstr (line, "NOOP"))
                {
                    // add virtual modes for VNC display
                    add_mode (mon, 640, 480, 0);
                    add_mode (mon, 720, 480, 0);
                    add_mode (mon, 800, 600, 0);
                    add_mode (mon, 1024, 768, 0);
                    add_mode (mon, 1280, 720, 0);
                    add_mode (mon, 1280, 1024, 0);
                    add_mode (mon, 1600, 1200, 0);
                    add_mode (mon, 1920, 1080, 0);
                    add_mode (mon, 2048, 1080, 0);
                    add_mode (mon, 2560, 1440, 0);
                    add_mode (mon, 3200, 1800, 0);
                    add_mode (mon, 3840, 2160, 0);
                }
            }
            else if (line[2] != ' ')
            {
                if (strstr (line, "Position"))
                {
                    sscanf (line, "  Position: %d,%d", &w, &h);
                    mons[mon].x = w;
                    mons[mon].y = h;
                }
                else if (strstr (line, "Transform"))
                {
                    for (i = 0; i < 4; i++)
                        if (strstr (line, orients[i])) mons[mon].rotation = i * 90;
                }
                else if (strstr (line, "Enabled"))
                {
                    if (strstr (line, "no")) mons[mon].enabled = FALSE;
                    else mons[mon].enabled = TRUE;
                }
            }
            else if (line[4] != ' ')
            {
                sscanf (line, "    %dx%d px, %f Hz", &w, &h, &f);
                add_mode (mon, w, h, f);
                if ((mons[mon].enabled && strstr (line, "current"))
                    || (! mons[mon].enabled && strstr (line, "preferred")))
                {
                    mons[mon].width = w;
                    mons[mon].height = h;
                    mons[mon].freq = f;
                }
            }
        }
        free (line);
        pclose (fp);
    }

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);
}

static void load_openbox_config (void)
{
    FILE *fp;
    char *line, *cptr;
    size_t len;
    int mon, w, h, i;
    float f;

    clear_config (FALSE);

    mon = -1;

    fp = popen ("xrandr", "r");
    if (fp)
    {
        line = NULL;
        len = 0;
        while (getline (&line, &len, fp) != -1)
        {
            if (line[0] != ' ')
            {
                if (strstr (line, "Screen")) continue;
                if (!strstr (line, " connected")) continue;
                mon++;
                cptr = strtok (line, " ");
                mons[mon].name = g_strdup (cptr);
                while (cptr[0] != '(')
                {
                    if (cptr[0] >= '1' && cptr[0] <= '9')
                    {
                        sscanf (cptr, "%dx%d+%d+%d", &mons[mon].width, &mons[mon].height, &mons[mon].x, &mons[mon].y);
                        mons[mon].enabled = TRUE;
                    }
                    for (i = 0; i < 4; i++)
                        if (strstr (cptr, xorients[i])) mons[mon].rotation = i * 90;
                    cptr = strtok (NULL, " ");
                }
                if (mons[mon].rotation == 90 || mons[mon].rotation == 270)
                {
                    i = mons[mon].width;
                    mons[mon].width = mons[mon].height;
                    mons[mon].height = i;
                }
            }
            else if (line[4] != ' ')
            {
                cptr = strtok (line, " ");
                while (cptr)
                {
                    if (strstr (cptr, "x")) sscanf (cptr, "%dx%d", &w, &h);
                    if (strstr (cptr, "."))
                    {
                        sscanf (cptr, "%f", &f);
                        add_mode (mon, w, h, f);
                    }
                    if (mons[mon].enabled && strstr (cptr, "*")) mons[mon].freq = f;
                    if (!mons[mon].enabled && *cptr == '+')
                    {
                        mons[mon].width = w;
                        mons[mon].height = h;
                        mons[mon].freq = f;
                    }
                    cptr = strtok (NULL, " ");
                }
            }
        }
        free (line);
        pclose (fp);
    }

    find_backlights ();
    sort_modes ();
    copy_config (mons, bmons);
}

/*----------------------------------------------------------------------------*/
/* Writing config */
/*----------------------------------------------------------------------------*/

static gboolean copy_profile (FILE *fp, FILE *foutp, int nmons)
{
    char *line;
    size_t len;
    gboolean valid = FALSE;
    char *buf, *tmp;
    int m;

    line = NULL;
    len = 0;
    while (getline (&line, &len, fp) != -1)
    {
        if (strstr (line, "profile"))
        {
            valid = TRUE;
            buf = g_strdup (line);
        }
        else if (valid)
        {
            if (strstr (line, "}"))
            {
                tmp = g_strdup_printf ("%s}\n", buf);
                g_free (buf);
                buf = tmp;

                if (nmons) fprintf (foutp, "%s\n", buf);
                g_free (buf);
                return TRUE;
            }
            else if (strstr (line, "output"))
            {
                tmp = g_strdup_printf ("%s%s", buf, line);
                g_free (buf);
                buf = tmp;

                for (m = 0; m < MAX_MONS; m++)
                {
                    if (mons[m].modes == NULL) continue;
                    if (strstr (line, mons[m].name)) nmons--;
                }
            }
        }
    }
    free (line);
    return FALSE;
}

static int write_config (FILE *fp)
{
    int m, nmons = 0;

    fprintf (fp, "profile {\n");
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        nmons++;
        if (mons[m].enabled == FALSE)
        {
            fprintf (fp, "\t\toutput %s disable\n", mons[m].name);
        }
        else if (mons[m].freq == 0.0)
        {
            fprintf (fp, "\t\toutput %s enable mode --custom %dx%d position %d,%d transform %s\n",
                mons[m].name, mons[m].width, mons[m].height,
                mons[m].x, mons[m].y, orients[mons[m].rotation / 90]);
        }
        else
        {
            fprintf (fp, "\t\toutput %s enable mode %dx%d@%.3f position %d,%d transform %s\n",
                mons[m].name, mons[m].width, mons[m].height, mons[m].freq,
                mons[m].x, mons[m].y, orients[mons[m].rotation / 90]);
        }
    }
    fprintf (fp, "}\n\n");

    return nmons;
}

static void merge_configs (const char *infile, const char *outfile)
{
    FILE *finp = fopen (infile, "r");
    FILE *foutp = fopen (outfile, "w");

    // write the profile for this config
    int nmons = write_config (foutp);

    // copy any other profiles
    while (copy_profile (finp, foutp, nmons));

    fclose (finp);
    fclose (foutp);
}

static void save_labwc_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    merge_configs (infile, outfile);
    g_free (infile);
    g_free (outfile);
}

static void write_dispsetup (const char *infile)
{
    char *cmd, *mstr, *tmp;
    int m;
    FILE *fp;

    cmd = g_strdup ("xrandr");
    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].enabled)
            mstr = g_strdup_printf ("--output %s --mode %dx%d --rate %0.3f --pos %dx%d --rotate %s", mons[m].name,
                mons[m].width, mons[m].height, mons[m].freq, mons[m].x, mons[m].y, xorients[mons[m].rotation / 90]);
        else
            mstr = g_strdup_printf ("--output %s --off", mons[m].name);
        tmp = g_strdup_printf ("%s %s", cmd, mstr);
        g_free (cmd);
        g_free (mstr);
        cmd = tmp;
    }

    fp = fopen (infile, "wb");
    fprintf (fp, "#!/bin/sh\nif %s --dryrun; then\n\t%s\nfi\n", cmd, cmd);
    g_free (cmd);

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].touchscreen == NULL) continue;
        cmd = g_strdup_printf ("xinput --map-to-output \"%s\" %s", mons[m].touchscreen, mons[m].name);
        fprintf (fp, "if xinput | grep -q \"%s\" ; then\n\t%s\nfi\n", mons[m].touchscreen, cmd);
        g_free (cmd);
    }

    fprintf (fp, "if [ -e /usr/share/ovscsetup.sh ] ; then\n\t/usr/share/ovscsetup.sh\nfi\nexit 0");
    fclose (fp);
}

static void save_openbox_config (void)
{
    const char *infile = "/var/tmp/dispsetup.bak";
    const char *outfile = "/var/tmp/dispsetup.sh";
    char *cmd;

    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    write_dispsetup (outfile);
}

static void reload_labwc_config (void)
{
    system ("pkill --signal SIGHUP kanshi");
}

static void reload_openbox_config (void)
{
    system ("/bin/bash /var/tmp/dispsetup.sh > /dev/null");
}

static void revert_labwc_config (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "kanshi/config.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "kanshi/config", NULL);
    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
    g_free (infile);
    g_free (outfile);
}

static void revert_openbox_config (void)
{
    const char *infile = "/var/tmp/dispsetup.bak";
    const char *outfile = "/var/tmp/dispsetup.sh";
    char *cmd;

    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
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

static void load_labwc_touchscreens (void)
{
    xmlDocPtr xDoc;
    xmlNode *root_node, *child_node;
    xmlAttr *attr;
    char *infile, *dev, *mon;
    int m;

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    if (!g_file_test (infile, G_FILE_TEST_IS_REGULAR))
    {
        g_free (infile);
        return;
    }

    xmlInitParser ();
    LIBXML_TEST_VERSION
    xDoc = xmlParseFile (infile);
    if (xDoc == NULL)
    {
        g_free (infile);
        return;
    }

    root_node = xmlDocGetRootElement (xDoc);
    for (child_node = root_node->children; child_node; child_node = child_node->next)
    {
        if (child_node->type != XML_ELEMENT_NODE) continue;
        if (!g_strcmp0 ((char *) child_node->name, "touch"))
        {
            dev = NULL;
            mon = NULL;
            for (attr = child_node->properties; attr; attr = attr->next)
            {
                if (!g_strcmp0 ((char *) attr->name, "deviceName"))
                    dev = g_strdup ((char *) attr->children->content);
                if (!g_strcmp0 ((char *) attr->name, "mapToOutput"))
                    mon = g_strdup ((char *) attr->children->content);
            }
            if (dev && mon)
            {
                for (m = 0; m < MAX_MONS; m++)
                {
                    if (mons[m].modes == NULL) continue;
                    if (!g_strcmp0 (mons[m].name, mon))
                    {
                        mons[m].touchscreen = g_strdup (dev);
                    }
                }
            }
            g_free (dev);
            g_free (mon);
        }
    }

    xmlFreeDoc (xDoc);
    xmlCleanupParser ();
    g_free (infile);
}

static void load_openbox_touchscreens (void)
{
    FILE *fp;
    GList *ts;
    int sw, sh, tw, th, tx, ty, m;
    char *cmd;
    float matrix[6];

    // get the screen size
    fp = popen ("xrandr | grep current  | cut -d \" \" -f 8,10", "r");
    if (fp)
    {
        if (fscanf (fp, "%d %d,", &sw, &sh) != 2)
        {
            sw = -1;
            sh = -1;
        }
        pclose (fp);
    }
    if (sw == -1 || sh == -1) return;

    // get the coord transform matrix for each touch device and calculate coords of touch device
    ts = touchscreens;
    while (ts)
    {
        cmd = g_strdup_printf ("xinput --list-props \"%s\" | grep Coordinate | cut -d : -f 2", (char *) ts->data);
        fp = popen (cmd, "r");
        if (fp)
        {
            if (fscanf (fp, "%f, %f, %f, %f, %f, %f,", matrix, matrix + 1, matrix + 2, matrix + 3, matrix + 4, matrix + 5) == 6)
            {
                if (matrix[0] != 1.0 || matrix[1] != 0.0 || matrix[2] != 0.0 || matrix[3] != 0.0 || matrix[4] != 1.0 || matrix[5] != 0.0)
                {
                    tw = ((float) sw + 0.5) * (matrix[0] + matrix[1]);
                    th = ((float) sh + 0.5) * (matrix[3] + matrix[4]);
                    tx = ((float) sw + 0.5) * matrix[2];
                    ty = ((float) sh + 0.5) * matrix[5];
                    if (tw < 0) tx += tw;
                    if (th < 0) ty += th;
                    if (tw * th < 0)
                    {
                        m = tw;
                        tw = th;
                        th = m;
                    }
                    if (tw < 0) tw *= -1;
                    if (th < 0) th *= -1;

                    for (m = 0; m < MAX_MONS; m++)
                    {
                        if (mons[m].modes == NULL) continue;
                        if (mons[m].width == tw && mons[m].height == th && mons[m].x == tx && mons[m].y == ty)
                        {
                            mons[m].touchscreen = g_strdup ((char *) ts->data);
                        }
                    }
                }
            }
            pclose (fp);
        }
        g_free (cmd);
        ts = ts->next;
    }
}

static void write_touchscreens (char *filename)
{
    xmlDocPtr xDoc;
    xmlNode *root_node, *child_node, *next;
    int m;

    xmlInitParser ();
    LIBXML_TEST_VERSION
    if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    {
        xDoc = xmlParseFile (filename);
        if (!xDoc) xDoc = xmlNewDoc ((xmlChar *) "1.0");
    }
    else xDoc = xmlNewDoc ((xmlChar *) "1.0");

    root_node = xmlDocGetRootElement (xDoc);
    if (root_node == NULL)
    {
        root_node = xmlNewNode (NULL, (xmlChar *) "openbox_config");
        xmlDocSetRootElement (xDoc, root_node);
        xmlNewNs (root_node, (xmlChar *) "http://openbox.org/3.4/rc", NULL);
    }

    child_node = root_node->children;
    while (child_node)
    {
        next = child_node->next;
        if (child_node->type == XML_ELEMENT_NODE)
        {
            if (!g_strcmp0 ((char *) child_node->name, "touch"))
            {
                xmlUnlinkNode (child_node);
                xmlFreeNode (child_node);
            }
        }
        child_node = next;
    }

    for (m = 0; m < MAX_MONS; m++)
    {
        if (mons[m].modes == NULL) continue;
        if (mons[m].touchscreen == NULL) continue;
        child_node = xmlNewNode (NULL, (xmlChar *) "touch");
        xmlSetProp (child_node, (xmlChar *) "deviceName", (xmlChar *) mons[m].touchscreen);
        xmlSetProp (child_node, (xmlChar *) "mapToOutput", (xmlChar *) mons[m].name);
        xmlAddChild (root_node, child_node);
    }

    xmlSaveFile (filename, xDoc);
    xmlFreeDoc (xDoc);
    xmlCleanupParser ();
}

static void save_labwc_touchscreens (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    cmd = g_strdup_printf ("cp %s %s", outfile, infile);
    system (cmd);
    g_free (cmd);
    write_touchscreens (outfile);
    g_free (infile);
    g_free (outfile);

    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rcgreeter.xml", NULL);
    cmd = g_strdup_printf ("cp /usr/share/labwc/rc.xml %s", outfile);
    system (cmd);
    g_free (cmd);
    write_touchscreens (outfile);
    g_free (outfile);
}

static void reload_labwc_touchscreens (void)
{
    system ("labwc --reconfigure");
}

static void revert_labwc_touchscreens (void)
{
    char *infile, *outfile, *cmd;

    infile = g_build_filename (g_get_user_config_dir (), "labwc/rc.bak", NULL);
    outfile = g_build_filename (g_get_user_config_dir (), "labwc/rc.xml", NULL);
    cmd = g_strdup_printf ("cp %s %s", infile, outfile);
    system (cmd);
    g_free (cmd);
    g_free (infile);
    g_free (outfile);
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
    if (gtk_widget_get_sensitive (undo)) update_system_config ();
    gtk_main_quit ();
}

static void handle_apply (GtkButton *, gpointer)
{
    if (compare_config (mons, bmons)) return;

    save_config ();
    save_touchscreens ();

    reload_config ();
    reload_touchscreens ();

    load_config ();
    load_touchscreens ();

    gtk_widget_queue_draw (da);
    gtk_widget_set_sensitive (undo, TRUE);
    show_confirm_dialog ();
}

static void handle_undo (GtkButton *, gpointer)
{
    revert_config ();
    revert_touchscreens ();

    reload_config ();
    reload_touchscreens ();

    load_config ();
    load_touchscreens ();

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
    if (gtk_widget_get_sensitive (undo)) update_system_config ();
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

    if (getenv ("WAYLAND_DISPLAY")) use_x = FALSE;
    else use_x = TRUE;

    clear_config (TRUE);

    find_touchscreens ();

    load_config ();
    load_touchscreens ();

    // ensure the config file reflects the current state, or undo won't work...
    save_config ();

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
