import re, os, os.path

def _get_mapcache_dirs(conf):
  """Get the location of the mapcache directory

  This is retrieved from the environment variables set by npm. This is
  only necessary as mapcache doesn't install headers or support
  pkgconfig"""

  try:
    build_dir = os.environ['npm_config_mapcache_build_dir']
  except KeyError:
    return conf.fatal('Mapcache build directory not found: set using `npm config set mapcache:build_dir DIR`')

  try:
    lib_dir = os.environ['npm_config_mapcache_lib_dir']
  except KeyError:
    return conf.fatal('Mapcache lib directory not found: set using `npm config set mapcache:lib_dir DIR`')
  
  if not os.path.isdir(build_dir):
    return conf.fatal('Mapcache build directory is not accessible: %s' % build_dir)

  if not os.path.isdir(lib_dir):
    return conf.fatal('Mapcache lib directory is not accessible: %s' % lib_dir)

  return build_dir, lib_dir

def _configure_mapcache_includes(conf, build_dir):
  """Add mapcache includes from the Mapcache build directory"""

  makefile_inc = os.path.join(build_dir, 'Makefile.inc')
  
  # add includes from the Makefile
  p = re.compile('^[A-Z]+_INC *= *(.+)$') # match an include header
  with open(makefile_inc, 'r') as f:
    for line in f:
      match = p.match(line)
      if match:
        arg = match.groups()[0].strip()
        if arg:
          conf.env.append_value('CXXFLAGS_LIBMAPCACHE', arg)

  # add the include for the local mapcache headers
  include = '-I%s' % os.path.join(build_dir, 'include')
  conf.env.append_value('CXXFLAGS_LIBMAPCACHE', include)

  # add debugging flags and defines
  if os.environ.get('npm_config_mapcache_debug'):
    conf.env.CXXDEFINES_LIBMAPCACHE = ['DEBUG']
    conf.env.CXXFLAGS += ["-ggdb"]

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  # This will tell the compiler to link our extension with the geocache library.
  build_dir, lib_dir = _get_mapcache_dirs(conf)
  _configure_mapcache_includes(conf, build_dir)
  conf.check_cxx(lib='mapcache', libpath=lib_dir, uselib_store='LIBMAPCACHE')

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall", "-pedantic"]
  obj.target = "bindings"
  obj.source = "bindings.cc"
  obj.uselib = ['LIBMAPCACHE']
