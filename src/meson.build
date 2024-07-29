sources = files (
    'raindrop.c'
)

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
deps = [ gtk, xml ]

executable ('raindrop', sources, dependencies: deps, install: true)
