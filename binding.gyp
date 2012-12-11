{
  "targets": [
    {
      "target_name": "bindings",
      "sources": [
        "src/node-mapcache.cpp",
        "src/mapcache.cpp",
        "src/asynclog.cpp"
      ],
      "include_dirs": [
        "<!@(python tools/config.py --include)"
      ],
      "conditions": [
        ['OS=="linux"', {
          'ldflags': [
            '-Wl,--no-as-needed,-lmapcache',
            '<!@(python tools/config.py --ldflags)'
          ],
          'libraries': [
            "<!@(python tools/config.py --libraries)"
          ],
          'cflags': [
            '<!@(python tools/config.py --cflags)',
            '-pedantic',
            '-Wall'
          ],
        }],
      ]
    }
  ]
}
