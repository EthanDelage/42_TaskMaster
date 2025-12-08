NAME        	:= taskmaster
BUILD_DIR   	:= .build
DEBUG_BUILD_DIR	:=	.debug

.PHONY: all
all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

.PHONY: clean
clean:
	$(MAKE) -C $(BUILD_DIR) clean
	$(MAKE) -C $(DEBUG_BUILD_DIR) clean

.PHONY: fclean
fclean:
	rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR)

.PHONY: re
re: fclean
	$(MAKE) all

.PHONY: debug
debug:
	cmake -S . -B $(DEBUG_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DDISABLE_DAEMON=$(DISABLE_DAEMON)
	cmake --build $(DEBUG_BUILD_DIR)

.PHONY: re_debug
re_debug: fclean
	$(MAKE) debug

.PHONY: format
format:
	git ls-files "*.cpp" "*.hpp" | xargs clang-format -i

.PHONY: compile_commands
compile_commands:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=1

include test/test.mk
