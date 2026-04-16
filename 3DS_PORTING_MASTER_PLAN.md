# 3DS完全ポーティング計画 - Mikage統合スキーマ

**スコープ**: port_cpu と ports/ の全コンポーネントを mikage/core に統合
**制約条件**:
- ✅ 外部依存なし（std C++ + iOS SDK のみ）
- ✅ JIT なし（インタプリタのみ）
- ✅ コンパイルエラーなし
- ✅ 段階的実装

---

## 📊 コンポーネント別統合計画

### Phase 1: インフラストラクチャ (Week 1)
#### 1. GDBスタブ統合 ✅ (既に実装)
- [ ] gdbstub/gdbstub.h - スタブ実装

#### 2. メモリシステム完全化 (2-3日)
- [ ] mem_map.h - 全体設定
- [ ] Memory::MemorySystem - 完全実装
- [ ] MMU仮想化層
- [ ] キャッシュ管理
- [ ] アクセス権限チェック

**ファイル**:
```
core/
├── mem_map.h               (拡張)
├── memory.h                (拡張)
├── mem_map.cpp             (拡張)
└── mem_map_funcs.cpp       (拡張)
```

#### 3. コア・タイミング系 (2-3日)
- [ ] core_timing.h/cpp - 完全イベントスケジューラ
- [ ] Timer/Clock管理
- [ ] Frame同期
- [ ] Interrupt dispatch

**ファイル系統から移植**:
- ports/ にはなし（mikageで独自実装）

---

### Phase 2: HLE / システムコール (Week 2-3)
#### システムコール基盤 (3-4日)
- [ ] kernel/svc.h/cpp - SVC dispatcher
- [ ] kernel/thread.h/cpp - スレッド管理
- [ ] kernel/mutex.h/cpp - ミューテックス
- [ ] kernel/event.h/cpp - イベントシステム
- [ ] kernel/semaphore.h/cpp - セマフォ
- [ ] kernel/timer.h/cpp - タイマー

**ファイル**:
```
core/hle/kernel/
├── svc.h                   (ports から移植)
├── svc.cpp                 (ports から移植)
├── thread.h                (ports から移植)
├── thread.cpp              (ports から移植)
├── mutex.h/cpp             (ports から移植)
├── event.h/cpp             (ports から移植)
├── semaphore.h/cpp         (ports から移植)
└── timer.h/cpp             (ports から移植)
```

#### メモリ管理 (2-3日)
- [ ] kernel/vm_manager.h/cpp - 仮想メモリ管理
- [ ] kernel/memory.h/cpp - メモリ割り当て
- [ ] kernel/handle_table.h/cpp - ハンドルテーブル
- [ ] kernel/process.h/cpp - プロセス管理
- [ ] kernel/shared_memory.h/cpp - 共有メモリ

#### IPC/メッセージング (2-3日)
- [ ] kernel/ipc.h/cpp - プロセス間通信
- [ ] kernel/hle_ipc.h/cpp - HLE IPC
- [ ] kernel/session.h/cpp - セッション
- [ ] kernel/port.h/cpp - ポート

---

### Phase 3: HLE サービス層 (Week 4-5)
#### 最小必須サービス
- [ ] service/sm/ - Service Manager (最重要)
- [ ] service/gsp/ - GPU Service (レンダリング基盤)
- [ ] service/ptm/ - Power/Battery/Time
- [ ] service/hid/ - Input
- [ ] service/fs/ - File System
- [ ] service/cfg/ - Configuration
- [ ] service/act/ - Accounts
- [ ] service/mic/ - Microphone (スタブ)
- [ ] service/cam/ - Camera (スタブ)
- [ ] service/http/ - HTTP (スタブ)

#### スタブサービス（必要最小限）
- [ ] service/ac/ - Wireless
- [ ] service/nim/ - Network
- [ ] service/dlp/ - Download Play
- [ ] その他 - 空実装またはエラーリターン

**ファイル**:
```
core/hle/service/
├── (各サービスディレクトリ)/
│   ├── service.h
│   ├── service.cpp
│   └── ...
└── service.h               (マスター)
```

---

### Phase 4: ハードウェア / I/O (Week 5-6)
#### 3DS ハードウェアコンポーネント
- [ ] hw/gpu.h/cpp - GPU状態管理
- [ ] hw/dsp.h/cpp - Audio DSP (スタブ)
- [ ] hw/aes/ - AES暗号化（IOレジスタ）
- [ ] hw/rsa/ - RSA暗号化（IOレジスタ）
- [ ] hw/ecc/ - ECC（IOレジスタ）
- [ ] hw/timers/ - タイマー
- [ ] hw/interrupts/ - 割り込みコントローラ
- [ ] hw/iram.h - 内部RAM管理
- [ ] hw/lcd.h - LCD制御
- [ ] hw/ndma.h - ダイレクトメモリアクセス

**ファイル**:
```
core/hw/
├── gpu/
├── dsp/
├── aes/
├── rsa/
├── ecc/
├── timers.h/cpp
├── interrupts.h/cpp
└── ...
```

---

### Phase 5: レンダラ / ビデオコア (Week 6-8)
#### ビデオサブシステム
最小実装：CPU部分のみ、ソフトウェアレンダリング

- [ ] video_core/gpu.h/cpp - GPU状態機
- [ ] video_core/shader/ - シェーダエンジン（ソフトウェア）
- [ ] video_core/rasterizer/ - ラスタライザ
- [ ] video_core/texture/ - テクスチャ管理
- [ ] video_core/framebuffer/ - フレームバッファ
- [ ] video_core/pica/ - PICA200コマンドプロセッサ

注: Metal/OpenGLなしでソフトウェア実装

**ファイル**: port/video_core から大幅に簡略化

---

### Phase 6: ファイルシステム (Week 8-9)
#### ファイルシステム基盤
- [ ] file_sys/fs.h/cpp - ファイルシステム抽象
- [ ] file_sys/romfs.h/cpp - ROM ファイルシステム
- [ ] file_sys/save.h/cpp - セーブデータ
- [ ] file_sys/sd.h/cpp - SD カード
- [ ] file_sys/loader.h/cpp - ゲームロード

**実装**: ports/file_sys から移植、iOS SDK で統合

---

### Phase 7: ローダ / アプリケーション (Week 9)
#### ゲーム実行基盤
- [ ] loader/elf.h/cpp - ELF ローダ
- [ ] loader/ncch.h/cpp - NCCH (3DS ゲーム) ローダ
- [ ] loader/app.h/cpp - アプリケーション管理
- [ ] loader/applets.h/cpp - アプレット（HLE）

**実装**: ports/loader から移植

---

## 🔧 外部依存性マッピング

### 削除 / 置き換え対象

| 依存性 | 出現 | 代替案 |
|--------|------|--------|
| Boost | config, timing, serialization | std::chrono, std::optional, JSON |
| Boost PropertyTree | config files | JSON parser (nlohmann/json) |
| GLFW/SDL | window handling | iOS UIKit / Views |
| OpenGL/Metal GPU | rendering | Software rasterizer |
| RapidJSON | JSON parsing | nlohmann/json または std::stringstream |
| xxhash | hashing | std::hash または SHA256 |
| cryptopp | encryption | iOS Security Framework |
| pthread | threading | std::thread, iOS GCD |
| libuv | async I/O | std::async, iOS Foundation |
| libcurl/libssl | networking | iOS URLSession / Network framework |

### 保持 / 活用対象

| 依存性 | 理由 |
|--------|------|
| std:: (C++17) | 標準ライブラリ |
| iOS/macOS SDK | ネイティブ機能 |
| 標準 math 関数 | 数値計算 |

### 実装パターン

```cpp
// 例: 暗号化
// Before (cryptopp):
// #include <cryptopp/aes.h>
// CryptoPP::AES_DES_Cipher cipher;

// After (iOS Security):
#include <Security/Security.h>
OSStatus result = CCCrypt(
    kCCEncrypt, kCCAlgorithmAES128,
    kCCOptionPKCS7Padding, 
    key, keyLen, NULL,
    data, dataLen,
    outData, outLen, &outLen
);
```

---

## 📈 実装優先度

### 必須（Tier 1）
1. ✅ メモリシステム（既に基盤あり、拡張のみ）
2. ✅ CPU（既に実装）
3. **SVC ディスパッチャ** - HLE基盤
4. **Thread 管理** - 必須
5. **Timer / Event** - スケジューリング

### 重要（Tier 2）
6. Service Manager （sm）
7. ファイルシステム基盤
8. GPU キュー
9. 入力（HID）

### オプション（Tier 3）
10. ネットワークサービス（スタブで十分）
11. Camera / Mic（スタブ）
12. 完全なシェーダー実装

---

## ⚙️ 実装パターン

### HLE システムコール

```cpp
// core/hle/kernel/svc.h
namespace HLE::Kernel {

struct SVC_Context {
    u32 r0, r1, r2, r3;  // Arguments / Return value
    const char* name;
    int (*handler)(SVC_Context&);
};

int SVC_CreateThread(SVC_Context& ctx);
int SVC_ExitThread(SVC_Context& ctx);
int SVC_SleepThread(SVC_Context& ctx);
// ... etc

}
```

### 外部依存削除済みイタプリタ

```cpp
// ports/video_core/gpu.cpp から
// Before:
// #include <boost/optional.hpp>
// std::vector<boost::optional<Texture>> textures;

// After:
// #include <optional>
// std::vector<std::optional<Texture>> textures;
```

---

## 🧪 段階的テスト戦略

各フェーズ後：

1. **単体テスト** - コンポーネント個別
2. **統合テスト** - インタフェース検証
3. **ブートテスト** - 3DS起動可否
4. **RTCテスト** - ゲーム実行基本

---

## ⏱️ 総所要時間

| Phase | 日数 | 完了予定 |
|-------|------|----------|
| 1. インフラ | 5-7 | Week 1 |
| 2. HLE 基盤 | 7-10 | Week 3 |
| 3. HLE Service | 7-10 | Week 5 |
| 4. HW/IO | 5-7 | Week 6 |
| 5. ビデオ | 10-14 | Week 8 |
| 6. ファイルシステム | 5-7 | Week 9 |
| 7. ローダ | 3-5 | Week 10 |
| **合計** | **42-60日** | **3ヶ月** |

---

## 🎯 成功指標

✅ ゲーム起動まで
1. NCCH ローダが正常に動作
2. メモリ配置が正しい
3. SVC が基本的に機能
4. Timer / スレッドが動作
5. フレームが描画可能

✅ ゲーム実行可能
6. GPIO / LCD 制御
7. GPU キューイング
8. ファイルシステムアクセス
9. 入力取得
10. オーディオバッファ

---

## 📋 チェックリスト

### Pre-Implementation
- [ ] ports/ 完全スキャン
- [ ] 依存性グラフ作成
- [ ] インターフェース設計
- [ ] テストフレームワーク

### Implementation
- [ ] Phase ごとにテスト
- [ ] コンパイルエラーまでゼロ
- [ ] 機能テスト
- [ ] 統合テスト

### Post-Implementation
- [ ] ゲーム起動テスト
- [ ] パフォーマンスプロファイリング
- [ ] ドキュメント
- [ ] クリーンアップ
