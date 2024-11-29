sources = files (
    'raindrop.c',
    'labwc.c',
    'openbox.c',
    'wayfire.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
layershell = dependency('gtk-layer-shell-0')
deps = [ gtk, xml, layershell ]

executable ('raindrop', sources, dependencies: deps, install: true)
