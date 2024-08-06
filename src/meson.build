sources = files (
    'raindrop.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
deps = [ gtk, xml ]

executable ('raindrop', sources, dependencies: deps, install: true)
