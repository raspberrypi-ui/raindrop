#include <gtk/gtk.h>

#define MAX_MONS 2

GdkRectangle mons[MAX_MONS];
int mousex, mousey;
int screenw, screenh;
int curmon;

void end_program (GtkWidget *, GdkEvent *, gpointer)
{
    gtk_main_quit ();
}

void draw_function (GtkDrawingArea *da, cairo_t *cr, gpointer)
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
        cairo_rectangle (cr, mons[m].x, mons[m].y, mons[m].width, mons[m].height);
        cairo_fill (cr);

        gdk_cairo_set_source_rgba (cr, &bk);
        cairo_rectangle (cr, mons[m].x, mons[m].y, mons[m].width, mons[m].height);
        cairo_stroke (cr);
    }
}

void drag_motion (GtkWidget *da, GdkDragContext *, gint x, gint y, guint time)
{
    if (curmon != -1)
    {
        mons[curmon].x = x - mousex;
        mons[curmon].y = y - mousey;

        if (mons[curmon].x < 0) mons[curmon].x = 0;
        if (mons[curmon].y < 0) mons[curmon].y = 0;
        if (mons[curmon].x + mons[curmon].width > screenw) mons[curmon].x = screenw - mons[curmon].width;
        if (mons[curmon].y + mons[curmon].height > screenh) mons[curmon].y = screenh - mons[curmon].height;

        gtk_widget_queue_draw (da);
    }
}

void drag_end (GtkWidget *, GdkDragContext *, gpointer)
{
    curmon = -1;
}

void show_menu (void)
{
    char *label = g_strdup_printf ("Monitor %d", curmon);
    GtkWidget *item = gtk_menu_item_new_with_label (label);
    g_free (label);
    GtkWidget *menu = gtk_menu_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show_all (menu);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), gtk_get_current_event ());
}

void click (GtkWidget *, GdkEventButton ev, gpointer)
{
    curmon = -1;
    for (int m = 0; m < MAX_MONS; m++)
    {
        if (ev.x > mons[m].x && ev.x < mons[m].x + mons[m].width
            && ev.y > mons[m].y && ev.y < mons[m].y + mons[m].height)
        {
            curmon = m;
            switch (ev.button)
            {
                case 1 :    mousex = ev.x - mons[m].x;
                            mousey = ev.y - mons[m].y;
                            break;

                case 3 :    show_menu ();
                            break;
            }
        }
    }
}

int main (int argc, char *argv[])
{
    mons[0].x = 50;
    mons[0].y = 50;
    mons[0].width = 50;
    mons[0].height = 50;

    mons[1].x = 200;
    mons[1].y = 150;
    mons[1].width = 75;
    mons[1].height = 50;

    gtk_init (&argc, &argv);
    GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (win, "delete-event", G_CALLBACK (end_program), NULL);

    GtkWidget *da = gtk_drawing_area_new ();
    gtk_drag_source_set (da, GDK_BUTTON1_MASK, NULL, 0, 0);
    gtk_drag_dest_set (da, 0, NULL, 0, 0);
    g_signal_connect (da, "draw", G_CALLBACK (draw_function), NULL);
    g_signal_connect (da, "button-press-event", G_CALLBACK (click), NULL);
    g_signal_connect (da, "drag-motion", G_CALLBACK (drag_motion), NULL);
    g_signal_connect (da, "drag-end", G_CALLBACK (drag_end), NULL);
    gtk_widget_set_size_request (da, 300, 200);
    gtk_container_add (GTK_CONTAINER (win), da);

    gtk_widget_show_all (win);
    screenw = gtk_widget_get_allocated_width (GTK_WIDGET (da));
    screenh = gtk_widget_get_allocated_height (GTK_WIDGET (da));
    
    gtk_main ();
    return 0;
}
