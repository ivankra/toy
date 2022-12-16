# Build deps: build-essential libpanel-applet-dev libgtk-3-dev pkg-config equivs

all: deb

libhostname-applet.so: hostname-applet.c
	gcc -o libhostname-applet.so -shared -fPIC -Wall -DGTK_DISABLE_SINGLE_INCLUDES -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE `pkg-config --cflags --libs gtk+-3.0 libpanel-applet` hostname-applet.c

install: libhostname-applet.so hostname.panel-applet
	mkdir -p -m 0755 $(PREFIX)/usr/lib/gnome-applets/ $(PREFIX)/usr/share/gnome-panel/applets/
	cp -f libhostname-applet.so $(PREFIX)/usr/lib/gnome-applets/
	chmod 0755 $(PREFIX)/usr/lib/gnome-applets/libhostname-applet.so
	cp -f hostname.panel-applet $(PREFIX)/usr/share/gnome-panel/applets/
	chmod 0644 $(PREFIX)/usr/share/gnome-panel/applets/hostname.panel-applet

deb: libhostname-applet.so hostname.panel-applet control
	equivs-build ./control

clean:
	rm -f libhostname-applet.so gnome-hostname-applet_*.deb
