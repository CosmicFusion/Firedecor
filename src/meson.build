cosmodecor = shared_module(
	'cosmodecor', [ 'cosmodecor.cpp', 'cosmodecor-subsurface.cpp',
				   'cosmodecor-buttons.cpp', 'cosmodecor-layout.cpp',
			       'cosmodecor-theme.cpp' ],
    dependencies: [ wf_config, wlroots, rsvg , pixman, glib, gdk_pixbuf, cairo, pango,
					pangocairo],
    install: true, install_dir: wayfire.get_variable(pkgconfig: 'plugindir'))
