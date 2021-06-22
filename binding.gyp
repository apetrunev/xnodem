{
  'targets': [
    {
      'target_name': 'mumps',
      'type': 'loadable_module',
      'sources': [
        'src/mumps.cc',
	'src/iconvm.cc'
      ],
      'cflags': [
	'-Wall',
	'-ansi',
        '-pedantic',
	'-std=c++11',
	'-Wno-write-strings'
      ],
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'gtm_dist%': '/usr/gtm'
          }
        }, {
          'variables': {
            'gtm_dist%': '/usr/gtm'
          }
        }]
      ],
      'include_dirs': [
        '<(gtm_dist)'
      ],
      'libraries': [
        '-L<(gtm_dist)',
        '-lgtmshr'
      ],
      'defines': [
        'GTM_VERSION=61'
      ],
      'ldflags': [
        '-Wl,-rpath,<(gtm_dist),--enable-new-dtags',
      ]
    }
  ]
}
