PWD = "$(shell pwd)"
SRC = "$(PWD)/src"
BUILD = "$(PWD)/build"

.PHONY: all clean

all: 
	@echo "Compiling source..."
	@node-gyp configure && node-gyp build
	@echo "Compilation is complited"

clean:
	@rm -rf $(BUILD) *.o
	@echo "Clean $(BUILD) directory..."




















