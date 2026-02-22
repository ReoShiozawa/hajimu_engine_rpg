BUILD_DIR := build

.PHONY: all clean install

all:
	@mkdir -p $(BUILD_DIR)
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -Wno-dev > /dev/null
	@echo "▶ ビルド中..."
	@cmake --build $(BUILD_DIR)
	@echo "✅ ビルド完了: $(BUILD_DIR)/engine_rpg.hjp"

install: all
	@mkdir -p ~/.hajimu/plugins/engine_rpg
	@cp $(BUILD_DIR)/engine_rpg.hjp ~/.hajimu/plugins/engine_rpg/
	@echo "✅ インストール完了: ~/.hajimu/plugins/engine_rpg"

clean:
	@rm -rf $(BUILD_DIR)
	@echo "🗑  クリーン完了"
