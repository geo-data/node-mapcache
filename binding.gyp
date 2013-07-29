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
      "configurations": {
        # This augments the `Debug` configuration used when calling `node-gyp`
        # with `--debug` to create code coverage files used by lcov
        "Debug": {
          "conditions": [
            ['OS=="linux"', {
              "cflags": ["--coverage"],
              "ldflags": ["--coverage"]
            }],
          ]
        },
      },
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
            '-Wall'
          ],
        }],
      ]
    }
  ]
}
