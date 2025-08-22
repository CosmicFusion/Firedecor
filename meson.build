project(
	'cosmodecor',
	'c',
	'cpp',
	version: '0.1',
	license: 'MIT',
	meson_version: '>=0.53.0',
	default_options: [
		'cpp_std=c++20',
        'c_std=c11',
		'warning_level=2',
		'werror=false',
	],
)

wayfire    = dependency('wayfire')
wf_config  = dependency('wf-config')
wlroots    = dependency('wlroots-0.18')
rsvg       = dependency('librsvg-2.0')
pixman     = dependency('pixman-1')
cairo      = dependency('cairo')
pango      = dependency('pango')
pangocairo = dependency('pangocairo')
glib       = dependency('glib-2.0')
gdk_pixbuf = dependency('gdk-pixbuf-2.0')
boost      = dependency('boost')

add_project_arguments(['-DWLR_USE_UNSTABLE'], language: ['cpp', 'c'])
add_project_arguments(['-DWAYFIRE_PLUGIN'], language: ['cpp', 'c'])
add_project_link_arguments(['-rdynamic'], language:'cpp')

install_data('share/executable.svg', install_dir: join_paths(get_option('datadir'), 'cosmodecor'))
install_data(['share/button-styles/reform/close.png', 'share/button-styles/reform/minimize.png', 'share/button-styles/reform/toggle-maximize.png', 'share/button-styles/reform/close-hovered.png', 'share/button-styles/reform/minimize-hovered.png', 'share/button-styles/reform/toggle-maximize-hovered.png', 'share/button-styles/reform/close-pressed.png', 'share/button-styles/reform/minimize-pressed.png', 'share/button-styles/reform/toggle-maximize-pressed.png'], install_dir: join_paths(get_option('datadir'), 'cosmodecor/button-styles/reform'))

subdir('src')
subdir('metadata')

summary = [
	'',
	'----------------',
	'cosmodecor @0@'.format(meson.project_version()),
	'----------------',
	''
]
message('\n'.join(summary))
