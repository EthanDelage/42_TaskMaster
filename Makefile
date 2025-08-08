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

.PHONY: format
format:
	git ls-files "*.c" "*.h" | xargs clang-format -i
