if build_standalone
  install_data('raindrop.ui', install_dir: ui_dir)
  install_data('plus.png', install_dir: ui_dir)
  install_data('minus.png', install_dir: ui_dir)
  i18n.merge_file(input: 'raindrop.desktop.in',
    output: 'raindrop.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: desktop_dir
  )
endif

if build_plugin
  install_data('raindrop.ui', install_dir: pui_dir)
  install_data('plus.png', install_dir: pui_dir)
  install_data('minus.png', install_dir: pui_dir)
endif
