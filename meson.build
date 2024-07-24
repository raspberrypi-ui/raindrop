project ('raindrop','c')

share_dir = join_paths(get_option('prefix'), 'share')
resource_dir = join_paths(share_dir, 'raindrop')
ui_dir = join_paths(resource_dir, 'ui')

add_project_arguments('-DPACKAGE_DATA_DIR="' + resource_dir + '"', language : [ 'c', 'cpp' ])

subdir('src')
subdir('data')
