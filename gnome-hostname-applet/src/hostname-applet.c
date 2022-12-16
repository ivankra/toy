#include <gtk/gtk.h>
#include <panel-applet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct State {
  char hostname[HOST_NAME_MAX + 1];
  char last_hostname[HOST_NAME_MAX + 1];
  char text[HOST_NAME_MAX + 10];
  GtkWidget* label;
};

static void hostname_applet_update(struct State* state) {
  if (gethostname(state->hostname, sizeof(state->hostname)) != 0) return;
  state->hostname[sizeof(state->hostname) - 1] = 0;

  if (strcmp(state->hostname, state->last_hostname) == 0) return;
  strcpy(state->last_hostname, state->hostname);

  snprintf(state->text, sizeof(state->text), "@%s", state->hostname);
  if (state->label != NULL) {
    gtk_label_set_text(GTK_LABEL(state->label), state->text);
  }
}

static gboolean hostname_applet_on_timeout(gpointer ptr) {
  struct State* state = (struct State*)ptr;
  hostname_applet_update(state);
  return TRUE;
}

static gboolean hostname_applet_start(PanelApplet* applet) {
  struct State* state = (struct State*)malloc(sizeof(struct State));
  memset(state, 0, sizeof(struct State));

  hostname_applet_update(state);

  state->label = gtk_label_new(state->text);
  gtk_container_add(GTK_CONTAINER(applet), state->label);
  gtk_widget_show_all(GTK_WIDGET(applet));

  g_timeout_add(10000, hostname_applet_on_timeout, state);  /* every 10s */

  return TRUE;
}

static gboolean hostname_applet_factory_callback(PanelApplet* applet,
                                                 const gchar* iid,
                                                 gpointer data) {
  gboolean retval = FALSE;

  if (g_strcmp0(iid, "HostnameApplet") == 0) {
    retval = hostname_applet_start(applet);
  }

  return retval;
}

PANEL_APPLET_IN_PROCESS_FACTORY("HostnameAppletFactory",
                                PANEL_TYPE_APPLET,
                                hostname_applet_factory_callback,
                                NULL)
