install_data('raindrop.ui', install_dir: ui_dir)
if not meson.project_name().contains('rpcc')
  i18n.merge_file(input: 'raindrop.desktop.in',
      output: 'raindrop.desktop',
      type: 'desktop',
      po_dir: '../po',
      install: true,
      install_dir: desktop_dir
    )
endif
install_data('pwdraindrop.sh', install_dir: resource_dir)
