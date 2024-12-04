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

shared_module(meson.project_name(), sources,
        dependencies: deps,
        install: true,
        install_dir: get_option('libdir') / 'rpcc',
        c_args : '-DPLUGIN_NAME="' + meson.project_name() + '"'
)
