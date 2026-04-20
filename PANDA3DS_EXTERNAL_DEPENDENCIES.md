# Panda3DS (reference) の外部依存（内蔵を除外）

対象: `src/core/mikage/reference/Panda3DS`

この一覧は、Panda3DS 参照実装内の `#include` と利用箇所を根拠に、**Panda3DS ディレクトリ内に同梱されていない依存**を抽出したもの。

## 外部依存

### コア/ライブラリ
- **Dynarmic**（ARM JIT）
  - 根拠: `dynarmic/interface/...` を直接 include。
- **Oaknut**（ARM64 コード生成）
  - 根拠: `oaknut/code_block.hpp`, `oaknut/oaknut.hpp` を include。
- **Xbyak**（x64 JIT アセンブラ）
  - 根拠: `xbyak/xbyak.h`, `xbyak/xbyak_util.h` を include。
- **capstone**（逆アセンブラ）
  - 根拠: `capstone/capstone.h` を include。
- **toml11 / toml.hpp**（設定ファイル読み書き）
  - 根拠: `toml.hpp` を include し `toml::parse`, `toml::find_or` を利用。
- **cmrc**（埋め込みリソース）
  - 根拠: `cmrc/cmrc.hpp` を include し `cmrc::ConsoleFonts::get_filesystem()` を利用。
- **GLM**（数学ライブラリ）
  - 根拠: `glm/glm.hpp` を include。
- **Lua**（スクリプト）
  - 根拠: `lua.h`, `lualib.h` を include。
- **stb_image_write**（画像出力）
  - 根拠: `stb_image_write.h` を include。

### プラットフォーム SDK / フレームワーク
- **Apple CommonCrypto**
  - 根拠: `CommonCrypto/CommonCryptor.h`, `CommonCrypto/CommonDigest.h`。
- **Apple AudioToolbox**
  - 根拠: `AudioToolbox/AudioQueue.h`, `AudioToolbox/AudioToolbox.h`。
- **Apple Foundation / QuartzCore**
  - 根拠: `Foundation/Foundation.h`, `QuartzCore/QuartzCore.h`。

## 除外（内蔵のため対象外）

- **fmt**
  - `src/core/mikage/reference/Panda3DS/include/fmt/*` が同梱。
- **xxHash**
  - `src/core/mikage/reference/Panda3DS/include/xxhash/xxhash.h` が同梱。
- **Teakra**
  - `src/core/mikage/reference/Panda3DS/include/teakra/*` および `src/core/mikage/reference/Panda3DS/src/core/audio/teakra/*` が同梱。
- **ELFIO**
  - `src/core/mikage/reference/Panda3DS/include/elfio/elfio.hpp` を同梱。
- **HTTP サーバ / メモリマップ I/O**
  - 外部ライブラリ（cpp-httplib, mio）ではなく、プラットフォーム提供 API（ソケット + `mmap`/`msync`）へ置換済み。

## 注記
- この repository の Panda3DS 参照ツリーには CMake 等の依存宣言ファイルが含まれていないため、
  判定はソース中の include/使用実態ベース。
- 依存によっては上位ビルド（親プロジェクト側）またはシステム/SDK で供給される前提。
