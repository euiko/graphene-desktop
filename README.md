Graphene Desktop (VDE)
==========

The desktop environment of VeltOS, an Arch Linux-based operating system
built entirely upon community input. Check out [velt.io](https://www.velt.io) more info.

Building
--------

graphene-desktop requires the following libraries:

    - accountsservice
    - gobject-2.0
    - gobject-introspection-1.0
    - gtk+-3.0
    - libpeas-1.0
    - libpulse
    - libwnck
    - mutter

And maybe some other ones too. Most of the GNOME stuff, really.

It will also be necessary to install xorg, as well as a way to start
it (xorg-xinit or a login manager such as LightDM). To run manually,
execute 'graphene-session' from a xorg terminal after installing.

Download/clone this repo and

```bash

    cd graphene-desktop
    sudo make
    sudo make install
```

graphene-desktop's data files (including panel applets) are stored in /usr/share/graphene.
All files here are customizable, however know that **running "sudo make install"
will overwrite any custom changes made to that directory.**

Eventually, this single-file make will be replaced with Autotools. Once I can
figure out how to use Autotools.