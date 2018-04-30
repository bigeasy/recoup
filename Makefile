make: make/CMakeLists.txt

make/CMakeLists.txt: CMakeLists.make.txt
	@mkdir -p make
	@cp $< $@
	@cd make && cmake -DCMAKE_BUILD_TYPE=Debug -G 'Unix Makefiles'

xcode: xcode/CMakeLists.txt

xcode/CMakeLists.txt: CMakeLists.xcode.txt
	@mkdir -p xcode
	@cp $< $@
	@echo 'Now you should:'
	@echo 'cd xcode && cmake -G Xcode'

node_modules/.bin/proof:
	mkdir node_modules
	npm install proof

test: node_modules/.bin/proof
	node_modules/.bin/proof test make/build/heap_test
