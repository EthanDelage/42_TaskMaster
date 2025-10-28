NAME        := taskmaster
BUILD_DIR   := .build

.PHONY: all
all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

.PHONY: clean
clean:
	$(MAKE) -C $(BUILD_DIR) clean

.PHONY: fclean
fclean:
	rm -rf $(BUILD_DIR)

.PHONY: re
re: fclean all

.PHONY: debug
debug:
	cmake -S . -B $(BUILD_DIR) -DDEBUG=1
	cmake --build $(BUILD_DIR)

.PHONY: re_debug
re_debug: fclean debug

.PHONY: format
format:
	git ls-files "*.cpp" "*.hpp" | xargs clang-format -i

.PHONY: compile_commands
compile_commands:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=1

include test/test.mk
