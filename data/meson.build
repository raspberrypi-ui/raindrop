if build_standalone
  install_data('raindrop.ui', install_dir: ui_dir)
  i18n.merge_file(input: 'raindrop.desktop.in',
    output: 'raindrop.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: desktop_dir
  )
  install_data('pwdraindrop.sh', install_dir: resource_dir)
endif

if build_plugin
  install_data('raindrop.ui', install_dir: pui_dir)
  install_data('pwdraindrop.sh', install_dir: presource_dir)
endif
