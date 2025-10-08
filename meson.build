project('wawaworld', 'c', 'cpp', default_options: ['cpp_std=c++20'])

rdm4001_proj = subproject('rdm4001')
rdm4001_dep = rdm4001_proj.get_variable('rdm4001_dep')

compiler = meson.get_compiler('cpp')

qrencode = dependency('libqrencode')

b3geometry_dep = declare_dependency(link_args : ['-lBullet3Geometry', '-lBullet3Common'] ) 
executable('wawaworld', 
	'gstate.cpp',
	'gstate.hpp',
	'main.cpp',
	'map.cpp',
	'weapon.cpp',
	'weapon.hpp',

	'weapons/magnum.cpp',
	'weapons/magnum.hpp',
	'weapons/sniper.cpp',
	'weapons/sniper.hpp',

        'planetmap.cpp',
        'planetmap.hpp',
	'wgame.cpp',
	'wgame.hpp',
	'worldspawn.cpp',
	'worldspawn.hpp',
	'wplayer.cpp',
	'wplayer.hpp', dependencies: [rdm4001_dep, b3geometry_dep, qrencode])
