# HLE差分メモ（reference 比較）

## 概要
- `reference/hle` と比較すると、Aurora 側は「サービス名は通すが中身は段階実装」という構成。
- 具体的には `srv:` が未知サービスを `OS_STUB` にフォールバックする実装を持つ。

## 大きい不足点
1. AM サービス
   - Aurora 側 `am.cpp` は 3 コマンドしか扱っていない。
   - reference 側 `am.cpp` は 5000 行規模で、多数の import/export/ticket/content API を実装。

2. 未実装モジュールを generic stub に集約
   - `cdc:U/gpio:U/i2c:U/mp:U/pdn:U/spi:U` は Aurora では `OS_STUB` に割当。

3. 実装済みサービスでも stub が多い
   - BOSS/AC/FRD などで `stubbed` ログや TODO が多い。

4. PM/PS/PXI など一部サービスは NotImplemented/stub 中心
   - PM は NotImplemented 応答がある。
   - PS/PXI は stub コマンド中心。
