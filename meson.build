project('kaleidoscope', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3',
                     'cpp_std=c++17'])

linenoise_subproject = subproject('linenoise')
linenoise_dep = linenoise_subproject.get_variable('linenoise_dep')

exe = executable('kaleidoscope', 'kaleidoscope.cpp',
  dependencies: [linenoise_dep],
  install : true)

test('basic', exe)
