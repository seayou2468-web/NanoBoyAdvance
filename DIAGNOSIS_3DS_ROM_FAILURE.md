# 3DS ROM実行失敗 - 診断レポート

## 🔴 主な問題点

### 1. **ARM_DynCom の未統合** ⚠️ 
**状況：** 移植したARM_DynComが初期化フロー（emulator.cpp）に統合されていない。

**証拠：**
```cpp
// emulator.cpp 内
bool Emulator::InitializeCPU() {
    NOTICE_LOG(CORE, "Initializing ARM11 CPU core");
    
    // ❌ 問題: ARM_Interpreterを使用
    cpu_core = std::make_unique<ARM_Interpreter>();
    
    // ✓ 移植したコードを使うべき:
    // cpu_core = std::make_unique<Core::ARM_DynCom>(...);
}
```

**影響：** DynComの高速インタプリタの代わりに、既存のARM_Interpreterが使用される。

---

### 2. **ヘッダーパスの不一致**

**問題ファイル：**
- `core/arm/dyncom/arm_dyncom_dec.cpp:5` → `#include "core/arm/dyncom/arm_dyncom_dec.h"` ❌
- `core/arm/dyncom/arm_dyncom_thumb.cpp:10` → `#include "core/arm/dyncom/arm_dyncom_thumb.h"` ❌

**期待：** 相対パスに修正すべき
- `#include "dyncom/arm_dyncom_dec.h"` ✓

**生じうる結果：** ビルド時のincludeエラー

---

### 3. **旧ヘッダーの混在** 

**emulator.cpp の includes:**
```cpp
#include "emulator.h"
#include "../common/log.h"             // ❌ 旧ヘッダー (compat層を使うべき)
#include <cstring>
```

**修正すべき：**
```cpp
#include "emulator.h"
#include "../common/compat/common/logging/log.h"  // ✓ 新compat層
```

---

### 4. **compat層の非使用**

**compat層が存在するが、実際には使用されていない：**
```
✓ common/compat/core/core.h
✓ common/compat/core/memory.h
✓ common/compat/core/core_timing.h
❌ emulator.cpp などで使用なし
```

---

### 5. **メモリシステムの不整合**

**问题：** emulator.cppは独自のベクトルベースメモリを使用
```cpp
memory.resize(256 * 1024 * 1024);    // std::vector
std::memset(memory.data(), 0, memory.size());
```

**ARM_DynCom期待：** `Memory::MemorySystem` インスタンス

---

### 6. **ARM9 <-> ARM11の未実装**

**3DSエミュレーションに必要：**
- ARM9コア（セキュリティプロセッサ）- **未実装**
- ARM11コア（メインコア）- ARM_Interpreterのみ

**ROM実行フロー：**
```
1. ブートROMでARM9起動 ❌ (ARM9エミュレータなし)
2. ARM9がファームウェア検証
3. ARM11に制御を移す
```

---

## 📊 システム構成状況

| コンポーネント | 状態 | 備考 |
|---|---|---|
| ARM_DynCom (JIT-free) | ✅ 移植完了 | core/arm/dyncom/ に配置 |
| ARM_Interpreter | ✅ 既存 | core/arm/interpreter/ (現在使用中) |
| 互換層 (compat) | ✅ 完成 | common/compat/ (未使用) |
| メモリシステム | 🟡 部分的 | 簡易実装のみ |
| ARM9エミュレータ | ❌ なし | **セキュリティプロセッサ未実装** |
| ARM11-ARM9 IPC | ❌ なし | 通信メカニズムなし |
| HLE実装 | 🟡 スケルトン | kernel/ 基本структура のみ |

---

## 🛠️ 修正スケジュール

### フェーズ1: 統合 (最優先)
1. **emulator.cpp を修正** → ARM_DynComを使用するよう変更
2. **compat層のincludeを統一** → 全ファイルで新ヘッダーを使用
3. **includeパスの修正** → core/arm/dyncom/*で相対パスを正確化

### フェーズ2: 完全性の確保
1. **ARM9エミュレータの実装** または **スタブ作成**
2. **IPC（プロセス間通信）メカニズム** の実装
3. **HLE Kernel の拡張** → SVC実装の完成

### フェーズ3: 検証
1. ROM読み込みフロー確認
2. 命令実行トレース
3. メモリアクセスパターン検証

---

## 💡 主要な根本原因

**3DSロムが動かない理由の優先順位：**

1. **🔴 ARM9エミュレータの欠如** - ROM起動時点で失敗
2. **🔴 ARM_DynComが未統合** - ARM11でも信頼性の問題
3. **🟡 compat層の非統合** - ビルドやリンクの失敗
4. **🟡 HLE不完全** - ファームウェアロード失敗
5. **🟡 メモリマッピング未実装** - アドレス解決エラー

---

## 次のアクション

修正優先度（推奨順）：

### ✅ 今すぐ可能
- [ ] emulator.cpp: ARM_DynCom統合 (15分)
- [ ] compat層の統合 (30分)
- [ ] includeパスの修正 (20分)

### ⏳ 次に実施推奨
- [ ] ARM9スタブエミュレータ作成 (1-2h)
- [ ] HLE Kernel拡張 (2-3h)
- [ ] IPC実装 (2-3h)

