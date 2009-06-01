
#include "program.h"
#include "pipe/p_format.h"

static gboolean main_idle(gpointer data)
{
	struct program *p = (struct program *)data;

	if (!ask_connect(p))
		main_quit(p);

	return false;
}

int main(int argc, char *argv[])
{
	struct program *p = g_malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));

	gtk_init(&argc, &argv);
	gtk_gl_init(&argc, &argv);

	p->draw.config = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
	                                           GDK_GL_MODE_ALPHA |
	                                           GDK_GL_MODE_DEPTH |
	                                           GDK_GL_MODE_DOUBLE);

	/* connect to first non gnome argument */
	if (argc > 1) {
		int len = strlen(argv[1]) + 1;
		p->ask.host = g_malloc(len);
		memcpy(p->ask.host, argv[1], len);
		p->ask.port = 13370;
		gtk_idle_add(main_idle, p);
	} else {
		ask_window_create(p);
	}

	gtk_main();

	g_free(p);

	return 0;
}

static void destroy(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	(void)widget;

	main_quit(p);
}

static void foreach(GtkTreeModel *model,
                    GtkTreePath *path,
                    GtkTreeIter *iter,
                    gpointer data)
{
	struct program *p = (struct program *)data;
	GtkTreeIter parent;
	GValue id;
	GValue type;
	GValue typename;
	(void)path;
	(void)data;

	memset(&id, 0, sizeof(id));
	memset(&type, 0, sizeof(type));
	memset(&typename, 0, sizeof(typename));

	gtk_tree_model_get_value(model, iter, COLUMN_ID, &id);
	gtk_tree_model_get_value(model, iter, COLUMN_TYPE, &type);
	gtk_tree_model_get_value(model, iter, COLUMN_TYPENAME, &typename);

	g_assert(G_VALUE_HOLDS_UINT64(&id));
	g_assert(G_VALUE_HOLDS_INT(&type));
	g_assert(G_VALUE_HOLDS_STRING(&typename));

	p->selected.iter = *iter;
	p->selected.id = g_value_get_uint64(&id);
	p->selected.type = g_value_get_int(&type);

	if (gtk_tree_model_iter_parent(model, &parent, iter)) {
		g_value_unset(&id);
		gtk_tree_model_get_value(model, &parent, COLUMN_ID, &id);
		g_assert(G_VALUE_HOLDS_UINT64(&id));

		p->selected.parent = g_value_get_uint64(&id);
	}
}

static void changed(GtkTreeSelection *s, gpointer data)
{
	struct program *p = (struct program *)data;
	enum types old_type;
	uint64_t old_id;
	(void)s;
	(void)p;

	old_id = p->selected.id;
	old_type = p->selected.type;

	/* reset selected data */
	memset(&p->selected, 0, sizeof(p->selected));

	gtk_tree_selection_selected_foreach(s, foreach, p);

	if (p->selected.id != old_id ||
	    p->selected.type != old_type) {
		if (old_id) {
			if (old_type == TYPE_SHADER)
				shader_unselected(p);
			else if (old_type == TYPE_TEXTURE)
				texture_unselected(p);
		}
		if (p->selected.id) {
			if (p->selected.type == TYPE_SHADER)
				shader_selected(p);
			else if (p->selected.type == TYPE_TEXTURE)
				texture_selected(p);
		}
	}
}

static void refresh(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	GtkTreeStore *store = p->main.treestore;
	(void)widget;

	if (p->selected.id != 0) {
		if (p->selected.type == TYPE_TEXTURE)
			texture_refresh(p);

		return;
	}

	gtk_tree_store_clear(p->main.treestore);

	gtk_tree_store_insert_with_values(store, &p->main.top, NULL, -1,
	                                  COLUMN_ID, 0,
	                                  COLUMN_TYPE, TYPE_SCREEN,
	                                  COLUMN_TYPENAME, "screen",
	                                  COLUMN_PIXBUF, icon_get("screen", p),
	                                  -1);

	/* contexts */
	context_list(store, &p->main.top, p);

	/* textures */
	texture_list(store, &p->main.top, p);

	/* expend all rows */
	gtk_tree_view_expand_all(p->main.treeview);
}

static void setup_cols(GtkBuilder *builder, GtkTreeView *view, struct program *p)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	(void)view;
	(void)p;

	/* column id */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_id"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_ID);

	/* column format */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_string"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_TYPENAME);

	/* column format */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_icon"));
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "pixbuf", COLUMN_PIXBUF);

	g_object_set(G_OBJECT(renderer), "xalign", (gfloat)0.0f, NULL);
}

static void icon_setup(struct program *p)
{
	p->icon.hash = g_hash_table_new(g_str_hash, g_str_equal);

	icon_add("res/rgba.png", "rgba", p);
	icon_add("res/rgbx.png", "rgbx", p);
	icon_add("res/argb.png", "argb", p);
	icon_add("res/xrgb.png", "xrgb", p);

	icon_add("res/s8z24.png", "s8z24", p);
	icon_add("res/x8z24.png", "x8z24", p);
	icon_add("res/z24s8.png", "z24s8", p);
	icon_add("res/z24x8.png", "z24x8", p);

	icon_add("res/rgb.png", "rgb", p);
	icon_add("res/rgb.png", "bgr", p);

	icon_add("res/screen.png", "screen", p);

	icon_add("res/shader_on_normal.png", "shader_on_normal", p);
	icon_add("res/shader_on_replaced.png", "shader_on_replaced", p);
	icon_add("res/shader_off_normal.png", "shader_off_normal", p);
	icon_add("res/shader_off_replaced.png", "shader_off_replaced", p);
}

void main_window_create(struct program *p)
{
	GtkBuilder *builder;

	GtkWidget *window;
	GObject *selection;
	GtkDrawingArea *draw;
	GtkTextView *textview;
	GtkWidget *textview_scrolled;
	GtkTreeView *treeview;
	GtkTreeStore *treestore;

	GObject *tool_quit;
	GObject *tool_refresh;

	GObject *tool_back;
	GObject *tool_forward;
	GObject *tool_background;
	GObject *tool_alpha;
	GObject *tool_automatic;

	GObject *tool_disable;
	GObject *tool_enable;
	GObject *tool_save;
	GObject *tool_revert;

	builder = gtk_builder_new();

	gtk_builder_add_from_file(builder, "res/main.xml", NULL);
	gtk_builder_connect_signals(builder, NULL);

	draw = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "draw"));
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	textview = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "textview"));
	treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview"));
	treestore = GTK_TREE_STORE(gtk_builder_get_object(builder, "treestore"));
	selection = G_OBJECT(gtk_tree_view_get_selection(treeview));
	textview_scrolled = GTK_WIDGET(gtk_builder_get_object(builder, "textview_scrolled"));


	tool_quit = gtk_builder_get_object(builder, "tool_quit");
	tool_refresh = gtk_builder_get_object(builder, "tool_refresh");

	tool_back = gtk_builder_get_object(builder, "tool_back");
	tool_forward = gtk_builder_get_object(builder, "tool_forward");
	tool_background = gtk_builder_get_object(builder, "tool_background");
	tool_alpha = gtk_builder_get_object(builder, "tool_alpha");
	tool_automatic = gtk_builder_get_object(builder, "tool_auto");

	tool_disable = gtk_builder_get_object(builder, "tool_disable");
	tool_enable = gtk_builder_get_object(builder, "tool_enable");
	tool_save = gtk_builder_get_object(builder, "tool_save");
	tool_revert = gtk_builder_get_object(builder, "tool_revert");

	setup_cols(builder, treeview, p);

	/* manualy set up signals */
	g_signal_connect(selection, "changed", G_CALLBACK(changed), p);
	g_signal_connect(tool_quit, "clicked", G_CALLBACK(destroy), p);
	g_signal_connect(tool_refresh, "clicked", G_CALLBACK(refresh), p);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), p);

	p->main.draw = draw;
	p->main.window = window;
	p->main.textview = textview;
	p->main.treeview = treeview;
	p->main.treestore = treestore;
	p->main.textview_scrolled = textview_scrolled;

	p->tool.back = GTK_WIDGET(tool_back);
	p->tool.forward = GTK_WIDGET(tool_forward);
	p->tool.background = GTK_WIDGET(tool_background);
	p->tool.alpha = GTK_WIDGET(tool_alpha);
	p->tool.automatic = GTK_WIDGET(tool_automatic);

	p->tool.disable = GTK_WIDGET(tool_disable);
	p->tool.enable = GTK_WIDGET(tool_enable);
	p->tool.save = GTK_WIDGET(tool_save);
	p->tool.revert = GTK_WIDGET(tool_revert);

	draw_setup(draw, p);

	gtk_widget_hide(p->tool.back);
	gtk_widget_hide(p->tool.forward);
	gtk_widget_hide(p->tool.background);
	gtk_widget_hide(p->tool.alpha);

	gtk_widget_hide(p->tool.disable);
	gtk_widget_hide(p->tool.enable);
	gtk_widget_hide(p->tool.save);
	gtk_widget_hide(p->tool.revert);

	gtk_widget_hide(p->main.textview_scrolled);
	gtk_widget_hide(GTK_WIDGET(p->main.textview));
	gtk_widget_hide(GTK_WIDGET(p->main.draw));

	gtk_widget_show(window);

	icon_setup(p);

	/* do a refresh */
	refresh(GTK_WIDGET(tool_refresh), p);
}

void main_quit(struct program *p)
{
	if (p->rbug.con) {
		rbug_disconnect(p->rbug.con);
		g_io_channel_unref(p->rbug.channel);
		g_source_remove(p->rbug.event);
		g_hash_table_unref(p->rbug.hash);
	}

	g_free(p->ask.host);

	gtk_main_quit();
}

void icon_add(const char *filename, const char *name, struct program *p)
{
	GdkPixbuf *icon = gdk_pixbuf_new_from_file(filename, NULL);

	if (!icon)
		return;

	g_hash_table_insert(p->icon.hash, (char*)name, icon);
}

GdkPixbuf * icon_get(const char *name, struct program *p)
{
	return (GdkPixbuf *)g_hash_table_lookup(p->icon.hash, name);
}