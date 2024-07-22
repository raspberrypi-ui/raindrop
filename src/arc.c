#include <gtk/gtk.h>

typedef struct {
    char *name;
    int width;
    int height;
    int x;
    int y;
    int rotation;
    float freq;
} monitor_t;

#define MAX_MONS 2

monitor_t mons[MAX_MONS];
int mousex, mousey;
int screenw, screenh;
int curmon;
int scalen = 1, scaled = 20;

GtkWidget *da;

#define SCALE(n) (((n)*scalen)/scaled)
#define UPSCALE(n) (((n)*scaled)/scalen)

void end_program (GtkWidget *, GdkEvent *, gpointer)
{
    gtk_main_quit ();
}

void draw_function (GtkDrawingArea *, cairo_t *cr, gpointer)
{
    GdkRGBA bg = { 0.25, 0.25, 0.25, 1.0 };
    GdkRGBA fg = { 1.0, 1.0, 1.0, 1.0 };
    GdkRGBA bk = { 0.0, 0.0, 0.0, 1.0 };

    gdk_cairo_set_source_rgba (cr, &bg);
    cairo_rectangle (cr, 0, 0, screenw, screenh);
    cairo_fill (cr);

    for (int m = 0; m < MAX_MONS; m++)
    {
        gdk_cairo_set_source_rgba (cr, &fg);
        if (mons[m].rotation == 90 || mons[m].rotation == 270)
            cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(mons[m].height), SCALE(mons[m].width));
        else
            cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(mons[m].width), SCALE(mons[m].height));
        cairo_fill (cr);

        gdk_cairo_set_source_rgba (cr, &bk);
        if (mons[m].rotation == 90 || mons[m].rotation == 270)
            cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(mons[m].height), SCALE(mons[m].width));
        else
            cairo_rectangle (cr, SCALE(mons[m].x), SCALE(mons[m].y), SCALE(mons[m].width), SCALE(mons[m].height));
        cairo_stroke (cr);
    }
}

gboolean rotated (int rot)
{
    if (rot == 90 || rot == 270) return TRUE;
    return FALSE;
}

void drag_motion (GtkWidget *da, GdkDragContext *, gint x, gint y, guint time)
{
    int w, h;
    if (curmon != -1)
    {
        mons[curmon].x = UPSCALE(x - mousex);
        mons[curmon].y = UPSCALE(y - mousey);

        if (mons[curmon].x < 0) mons[curmon].x = 0;
        if (mons[curmon].y < 0) mons[curmon].y = 0;
        w = rotated (mons[curmon].rotation) ? mons[curmon].height : mons[curmon].width;
        h = rotated (mons[curmon].rotation) ? mons[curmon].width : mons[curmon].height;
        if (SCALE(mons[curmon].x + w) > screenw) mons[curmon].x = UPSCALE(screenw - SCALE(w));
        if (SCALE(mons[curmon].y + h) > screenh) mons[curmon].y = UPSCALE(screenh - SCALE(h));

        gtk_widget_queue_draw (da);
    }
}

void drag_end (GtkWidget *, GdkDragContext *, gpointer)
{
    curmon = -1;
}

void set_resolution (GtkMenuItem *item, gpointer)
{
    sscanf (gtk_menu_item_get_label (item), "%dx%d", &(mons[curmon].width), &(mons[curmon].height));
    gtk_widget_queue_draw (da);
}

void add_resolution (GtkWidget *menu, int width, int height)
{
    char *label = g_strdup_printf ("%dx%d", width, height);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[curmon].width == width && mons[curmon].height == height);
    g_signal_connect (item, "activate", G_CALLBACK (set_resolution), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void set_orientation (GtkMenuItem *item, gpointer)
{
    sscanf (gtk_menu_item_get_label (item), "%d", &(mons[curmon].rotation));
    gtk_widget_queue_draw (da);
}

void add_orientation (GtkWidget *menu, int rotation)
{
    char *label = g_strdup_printf ("%d", rotation);
    GtkWidget *item = gtk_check_menu_item_new_with_label (label);
    g_free (label);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), mons[curmon].rotation == rotation);
    g_signal_connect (item, "activate", G_CALLBACK (set_orientation), NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void show_menu (void)
{
    GtkWidget *item;

    GtkWidget *menu = gtk_menu_new ();

    GtkWidget *rmenu = gtk_menu_new ();
    add_resolution (rmenu, 3840, 2160);
    add_resolution (rmenu, 1920, 1080);
    add_resolution (rmenu, 800, 600);
    item = gtk_menu_item_new_with_label ("Resolution");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), rmenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    GtkWidget *omenu = gtk_menu_new ();
    add_orientation (omenu, 0);
    add_orientation (omenu, 90);
    add_orientation (omenu, 180);
    add_orientation (omenu, 270);
    item = gtk_menu_item_new_with_label ("Orientation");
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), omenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
}

void click (GtkWidget *, GdkEventButton ev, gpointer)
{
    int w, h;
    curmon = -1;
    for (int m = 0; m < MAX_MONS; m++)
    {
        w = rotated (mons[m].rotation) ? mons[m].height : mons[m].width;
        h = rotated (mons[m].rotation) ? mons[m].width : mons[m].height;

        if (ev.x > SCALE(mons[m].x) && ev.x < SCALE(mons[m].x + w)
            && ev.y > SCALE(mons[m].y) && ev.y < SCALE(mons[m].y + h))
        {
            curmon = m;
            switch (ev.button)
            {
                case 1 :    mousex = ev.x - SCALE(mons[m].x);
                            mousey = ev.y - SCALE(mons[m].y);
                            break;

                case 3 :    show_menu ();
                            break;
            }
        }
    }
}

int main (int argc, char *argv[])
{
    mons[0].x = 0;
    mons[0].y = 0;
    mons[0].width = 3840;
    mons[0].height = 2160;
    mons[0].name = g_strdup ("HDMI-A-1");

    mons[1].x = 3840;
    mons[1].y = 0;
    mons[1].width = 1920;
    mons[1].height = 1080;
    mons[1].name = g_strdup ("HDMI-A-2");

    gtk_init (&argc, &argv);
    GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (win, "delete-event", G_CALLBACK (end_program), NULL);

    da = gtk_drawing_area_new ();
    gtk_drag_source_set (da, GDK_BUTTON1_MASK, NULL, 0, 0);
    gtk_drag_dest_set (da, 0, NULL, 0, 0);
    g_signal_connect (da, "draw", G_CALLBACK (draw_function), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (click), NULL);
    g_signal_connect (da, "drag-motion", G_CALLBACK (drag_motion), NULL);
    g_signal_connect (da, "drag-end", G_CALLBACK (drag_end), NULL);
    gtk_widget_set_size_request (da, 500, 400);
    gtk_container_add (GTK_CONTAINER (win), da);

    gtk_widget_show_all (win);
    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (da));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (da));
    
    gtk_main ();
    return 0;
}
