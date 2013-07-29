##
# Node-mapcache development targets
#
# This Makefile contains the following primary targets intended to facilitate
# Linux based development:
#
#  - `make build`: create a Debug build of the module
#  - `make test`: run the tests
#  - `make cover`: perform the code coverage analysis
#  - `make valgrind`: run the test suite under valgrind
#  - `make doc`: create the doxygen documentation
#  - `make clean`: remove generated files
#

# The location of the mapcache build directory
ifeq ($(strip $(npm_config_mapcache_build_dir)),)
	npm_config_mapcache_build_dir = $(shell npm config get mapcache:build_dir)
endif

# The location of the `vows` test command
VOWS = ./node_modules/.bin/vows

# The location of the `node-gyp` module builder. Try and get a globally
# installed version, falling back to a local install.
NODE_GYP = $(shell which node-gyp)
ifeq ($(NODE_GYP),)
	NODE_GYP = ./node_modules/.bin/node-gyp
endif

# The location of the `istanbul` JS code coverage framework. Try and get a
# globally installed version, falling back to a local install.
ISTANBUL=$(shell which istanbul)
ifeq ($(ISTANBUL),)
	ISTANBUL = ./node_modules/.bin/istanbul
endif

# Dependencies for the test target
test_deps=build \
./test/*.js \
./test/*.xml \
lib/*.js \
$(VOWS)


# Build the module
all: build
build: build/Debug/bindings.node
build/Debug/bindings.node: $(NODE_GYP) src/*.hpp src/*.cpp
	npm_config_mapcache_build_dir=$(npm_config_mapcache_build_dir) $(NODE_GYP) -v --debug configure build

# Test the module
test: $(test_deps)
	$(VOWS) --spec ./test/mapcache-test.js

# Run the test suite under valgrind
valgrind: $(test_deps) $(ISTANBUL)
	valgrind --leak-check=full --show-reachable=yes --track-origins=yes \
	node --nouse_idle_notification --expose-gc \
	$(VOWS) test/mapcache-test.js

# Perform the code coverage
cover: coverage/index.html
coverage/index.html: coverage/node-mapcache.info
	genhtml --output-directory coverage coverage/node-mapcache.info
	@echo "\033[0;32mPoint your browser at \`coverage/index.html\`\033[m\017"
coverage/node-mapcache.info: coverage/bindings.info
	lcov --test-name node-mapcache \
	--add-tracefile coverage/lcov.info \
	--add-tracefile coverage/bindings.info \
	--output-file coverage/node-mapcache.info
coverage/bindings.info: coverage/addon.info
	lcov --extract coverage/addon.info '*node-mapcache/src/*' --output-file coverage/bindings.info
coverage/addon.info: coverage/lcov.info
	lcov --capture --base-directory src/ --directory . --output-file coverage/addon.info
# This generates the JS lcov info as well as gcov `*.gcda` files:
coverage/lcov.info: $(test_deps) $(ISTANBUL)
	node --nouse_idle_notification --expose-gc \
	$(ISTANBUL) cover --report lcovonly \
	$(VOWS) -- test/mapcache-test.js

# Install required node modules
$(NODE_GYP):
	npm install node-gyp

$(VOWS): package.json
	npm install vows
	@touch $(VOWS)

$(ISTANBUL): package.json
	npm install istanbul
	@touch $(ISTANBUL)

# Generate the Doxygen documentation
doc: doc/.made
doc/.made: doc/Doxyfile
	chdir ./doc/ && doxygen
	@echo "\033[0;32mPoint your browser at \`doc/html/index.html\`\033[m\017"
	@touch doc/.made

# Clean up any generated files
clean: $(NODE_GYP)
	$(NODE_GYP) clean
	rm -rf coverage \
	doc/html \
	doc/latex

.PHONY: test
