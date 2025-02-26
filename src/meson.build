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

if build_plugin
  shared_module(plugin_name, sources, dependencies: deps, install: true,
    install_dir: get_option('libdir') / 'rpcc',
    c_args : [ '-DPACKAGE_DATA_DIR="' + presource_dir + '"', '-DPLUGIN_NAME="' + plugin_name + '"' ],
    link_args : '-Wl,-z,defs'
  )
endif

if build_standalone
  executable (meson.project_name(), sources, dependencies: deps, install: true,
    c_args : '-DPACKAGE_DATA_DIR="' + resource_dir + '"'
  )
endif
