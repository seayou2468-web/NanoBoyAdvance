# Panda3DS (reference) の外部依存（内蔵を除外）

対象: `src/core/mikage/reference/Panda3DS`

この一覧は、Panda3DS 参照実装内の `#include` と利用箇所を根拠に、**Panda3DS ディレクトリ内に同梱されていない依存**を抽出したもの。

## 外部依存

### コア/ライブラリ
- **Oaknut**（ARM64 コード生成）
  - 根拠: `oaknut/code_block.hpp`, `oaknut/oaknut.hpp` を include。
- **Xbyak**（x64 JIT アセンブラ）
  - 根拠: `xbyak/xbyak.h`, `xbyak/xbyak_util.h` を include。
- **capstone**（逆アセンブラ）
  - 根拠: `capstone/capstone.h` を include。
- **cmrc**（埋め込みリソース）
  - 根拠: `cmrc/cmrc.hpp` を include し `cmrc::ConsoleFonts::get_filesystem()` を利用。

### プラットフォーム SDK / フレームワーク
- **Apple CommonCrypto**
  - 根拠: `CommonCrypto/CommonCryptor.h`, `CommonCrypto/CommonDigest.h`。
- **Apple AudioToolbox**
  - 根拠: `AudioToolbox/AudioQueue.h`, `AudioToolbox/AudioToolbox.h`。
- **Apple Foundation / QuartzCore**
  - 根拠: `Foundation/Foundation.h`, `QuartzCore/QuartzCore.h`。
- **Apple SIMD**
  - 根拠: `simd/simd.h` を include（センサー変換処理）。

## 除外（内蔵のため対象外）

- **fmt**
  - `src/core/mikage/reference/Panda3DS/include/fmt/*` が同梱。
- **xxHash**
  - `src/core/mikage/reference/Panda3DS/include/xxhash/xxhash.h` が同梱。
- **Teakra**
  - `src/core/mikage/reference/Panda3DS/include/teakra/*` および `src/core/mikage/reference/Panda3DS/src/core/audio/teakra/*` が同梱。
- **ELFIO**
  - `src/core/mikage/reference/Panda3DS/include/elfio/elfio.hpp` を同梱。
- **toml11**
  - `src/core/mikage/reference/Panda3DS/include/toml.hpp` および `src/core/mikage/reference/Panda3DS/include/toml/*` を同梱。
- **JavaScriptCore（iOS SDK）**
  - Lua 同梱実装は廃止し、iOS SDK の `JavaScriptCore.framework` が提供する同等機能へ置換。
  - 依存はシステム SDK 供給とし、Panda3DS ツリー内への Lua ソース同梱は行わない。
- **HTTP サーバ / メモリマップ I/O**
  - 外部ライブラリ（cpp-httplib, mio）ではなく、プラットフォーム提供 API（ソケット + `mmap`/`msync`）へ置換済み。

- **stb_image_write**
  - `src/core/mikage/reference/Panda3DS/src/stb_image_write.c` を削除済み（機能ごと除去）。

## CPU 実装
- Cytrus/Citra 系 DynCom CPU インタプリタを `src/core/mikage/reference/Panda3DS/src/core/CPU/cytrus_arm/*` と
  `src/core/mikage/reference/Panda3DS/include/cytrus_arm/*` に移植。
- Panda3DS 側 CPU フロント (`cpu_interpreter.*`) も JIT 非依存のインタプリタ経路に置換。

## 注記
- この repository の Panda3DS 参照ツリーには CMake 等の依存宣言ファイルが含まれていないため、
  判定はソース中の include/使用実態ベース。
- 依存によっては上位ビルド（親プロジェクト側）またはシステム/SDK で供給される前提。
