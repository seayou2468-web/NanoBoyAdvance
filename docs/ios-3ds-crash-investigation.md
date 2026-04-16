# iOS で .3ds ROM 起動時にクラッシュする件の調査メモ

## 結論（原因）

`.3ds` は iOS 側で `EMULATOR_CORE_TYPE_3DS` として認識され、Mikage 3DS コアへ渡されていますが、
Mikage 側には `abort()` でプロセスを即時終了する経路が残っており、3DS タイトル実行中にそこへ到達すると
アプリがクラッシュします。

## 根拠

- iOS ライブラリ画面は `.3ds` 拡張子を 3DS コアへマッピングしている。
- エミュレータ画面は `EmulatorCore_LoadROMFromPath()` 成功後にフレーム実行 (`EmulatorCore_StepFrame`) を開始する。
- Mikage の ARM インタプリタには `abort()` 呼び出しが残っている（安全にエラーへ変換していない）。

## 補足

3DS コアは iOS 実装がまだ発展途上で、
UI 側では 3DS を選択可能でも、コア内部で未実装/想定外ケースに入るとクラッシュしやすい状態です。

## 改善案（次アクション）

1. `abort()` を呼ぶ箇所を「エラー状態を返す実装」に置換する。
2. 置換完了まで iOS では 3DS を「実験的機能」扱いにし、明示警告または起動ブロックを入れる。
3. `EmulatorCore_StepFrame` 側で致命エラー時に即停止し、ユーザーへエラーメッセージ表示する。

## 進捗（Cytrus CPUインタプリタ移植）

- Cytrus の Skyeye CPU interpreter 関連ソースを `src/core/mikage/cytrus_cpu/` にステージング取り込み済み。
- 取り込み元: `folium-app/Cytrus` (`84f59c91a889fccd0c4a077f5c529c1836f40387`)
- 現時点では「内蔵（vendor）」まで完了。実行系への差し替えは API/メモリ層アダプタを順次実装して行う。
