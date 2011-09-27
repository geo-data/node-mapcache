libs = "-lpng -ljpeg -L/usr/lib -lapr-1 -L/usr/lib -laprutil-1 -lpcre -lsqlite3 -lpixman-1   -L/usr/lib -lgdal -L/usr/lib -lgeos_c"
cflags = "-g -DLINUX=2 -D_REENTRANT -D_GNU_SOURCE -D_LARGEFILE64_SOURCE -pthread -O2 -Wall -fPIC -DHAVE_SYMLINK -DNDEBUG -I../include -DTHREADED_MPM=0 -I/usr/include -I/usr/include -I/usr/include/apr-1 -I/usr/include/apr-1 -I/usr/include -DUSE_PCRE -DUSE_OGR -DUSE_GEOS -DUSE_SQLITE -DUSE_PIXMAN -I/usr/include/pixman-1"

def set_options(opt):
  opt.tool_options("compiler_cxx")

def configure(conf):
  conf.check_tool("compiler_cxx")
  conf.check_tool("node_addon")
  conf.env.append_value('CXXFLAGS', cflags.split(' '))
  conf.env.append_value('LINKFLAGS', libs.split(' '))
  conf.env.CCDEFINES_GEOCACHE = ['GEOCACHE']
  #conf.env.CCFLAGS_GEOCACHE   = ['-O0']
  conf.env.LIB_GEOCACHE       = 'geocache'
  conf.env.LIBPATH_GEOCACHE   = ['/home/homme/checkouts/mod-geocache/src']
  #conf.env.LINKFLAGS_GEOCACHE = ['-g']

def build(bld):
  obj = bld.new_task_gen("cxx", "shlib", "node_addon")
  obj.cxxflags = ["-g", "-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE", "-Wall", "-I/home/homme/checkouts/mod-geocache/include"]
  obj.target = "bindings"
  obj.source = "bindings.cc"
  obj.uselib = ['GEOCACHE']
