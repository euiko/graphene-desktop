# This file is part of graphene-desktop, the desktop environment of VeltOS.
# Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
# This file is licensed under the WTFPL.

add_custom_command(
  OUTPUT session-dbus-iface.c session-dbus-iface.h
  COMMAND gdbus-codegen --interface-prefix org.gnome --c-namespace DBus --generate-c-code session-dbus-iface ${CMAKE_CURRENT_SOURCE_DIR}/session-dbus-iface.xml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/session-dbus-iface.xml
)

add_custom_command(
  OUTPUT status-notifier-dbus-ifaces.c status-notifier-dbus-ifaces.h
  COMMAND gdbus-codegen --interface-prefix org --c-namespace DBus --generate-c-code status-notifier-dbus-ifaces ${CMAKE_CURRENT_SOURCE_DIR}/status-notifier-dbus-ifaces.xml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/status-notifier-dbus-ifaces.xml
)

add_custom_command(
  OUTPUT notifications-dbus-iface.c notifications-dbus-iface.h
  COMMAND gdbus-codegen --interface-prefix org.freedesktop --c-namespace DBus --generate-c-code notifications-dbus-iface ${CMAKE_CURRENT_SOURCE_DIR}/notifications-dbus-iface.xml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/notifications-dbus-iface.xml
)

pkg_check_modules(GIOUNIX2 REQUIRED gio-unix-2.0>=2.10)
pkg_check_modules(LIBMUTTER REQUIRED libmutter>=3.22)
pkg_check_modules(LIBPULSEGLIB REQUIRED libpulse-mainloop-glib>=8.0)
pkg_check_modules(POLKITAGENT REQUIRED polkit-agent-1)
pkg_check_modules(LIBRSVG REQUIRED librsvg-2.0)
pkg_check_modules(LIBGNOMEMENU REQUIRED libgnome-menu-3.0>=3.13)
pkg_check_modules(LIBACT REQUIRED accountsservice>=0.6)
link_directories(${LIBMUTTER_LIBRARY_DIRS})

add_executable(graphene-desktop
	main.c
	session-dbus-iface.c
	session.c
	client.c
	# status-notifier-watcher.c
	# ${PROJECT_SOURCE_DIR}/status-notifier-dbus-ifaces.c
	util.c
	wm.c
	percent-floater.c
	dialog.c
	background.c
	pkauthdialog.c
	cmk/button.c
	cmk/shadow.c
	cmk/cmk-widget.c
	cmk/cmk-icon-loader.c
	cmk/cmk-icon.c
	cmk/cmk-label.c
	# ${PROJECT_SOURCE_DIR}/common/dbusmonitor.c
	csk/audio.c
	status-icons.c
	panel.c
	panel-launcher.c
	panel-settings.c
	panel-clock.c
	settings-battery.c
	network.c
	notifications-dbus-iface.c
	notifications.c
)
target_link_libraries(graphene-desktop
	${GIOUNIX2_LIBRARIES}
	${LIBMUTTER_LIBRARIES}
	${LIBPULSEGLIB_LIBRARIES}
	${POLKITAGENT_LIBRARIES}
	${LIBRSVG_LIBRARIES}
	${LIBGNOMEMENU_LIBRARIES}
	${LIBACT_LIBRARIES}
	m
)
target_include_directories(graphene-desktop PRIVATE
	${CMAKE_CURRENT_BINARY_DIR} 
	${GIOUNIX2_INCLUDE_DIRS}
	${LIBMUTTER_INCLUDE_DIRS}
	${LIBPULSEGLIB_INCLUDE_DIRS}
	${POLKITAGENT_INCLUDE_DIRS}
	${LIBRSVG_INCLUDE_DIRS}
	${LIBGNOMEMENU_INCLUDE_DIRS}
	${LIBACT_INCLUDE_DIRS}
)

# Normally, the CMake install functionality removes the executable's rpath,
# which here includes /usr/lib/mutter. This keeps the rpath.
set_target_properties(graphene-desktop PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(TARGETS graphene-desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
