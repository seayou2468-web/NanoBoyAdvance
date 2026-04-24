# 描画経路調査メモ (2026-04-24)

対象: `Hoshi no Kirby - Robobo Planet (Japan) Decrypted.3ds` を `AuroraSDL` で起動した際の黒画面。

## 実施ログ

- コマンド:
  - `SDL_VIDEODRIVER=dummy timeout 25s ./build/AuroraSDL "/tmp/roms/Hoshi no Kirby - Robobo Planet (Japan) Decrypted.3ds"`
- 観測された主要警告:
  - `Composite output stayed black despite valid blit sources. ... head=00 00 00`

この警告は「フレームバッファとして選択されたポインタは有効だが、先頭サンプルが全ゼロ」の状態を示す。

## 静的経路の整理

### 1) GPUサービス側の更新発火

- `TriggerCmdReqQueue` から `processCommandBuffer()` が呼ばれ、必要なら `dirtyFlag` を見て `setBufferSwapImpl()` が走る。 
- `requestInterrupt(VBlank*)` でも `processCommandBuffer()` と `dirtyFlag` 処理が実行される。

=> つまり、理想経路では「GXコマンド消化」→「LCD向けフレームバッファ情報更新」が GSP サービス内で進む。

### 2) コマンド消化の前提条件

- `processCommandBuffer()` は `sharedMem == nullptr` の場合に即 return。
- コマンドリングは `sharedMem + 0x800 + thread*0x200` を参照し、`commandsLeft` を 0 まで処理する。

=> `RegisterInterruptRelayQueue` と `MapMemoryBlock(GSP shared mem)` の双方が成立しない限り、GX コマンド経路は進まない。

### 3) 最終合成 (`Emulator::compositeFramebuffers`)

- まず LCD 由来の top/bottom バッファ候補を引く。
- 取れない/黒サンプルなら、`ColourBufferLoc + VRAM base` を fallback として参照。
- それでも blit 元が黒の場合、`Composite output stayed black...` を周期的に警告する。

=> 今回の実行ログの警告は、この最終段で「取得済みバッファが全部黒」の分岐に一致。

## 現時点の示唆

1. 現在の失敗は「合成先の選択ミス」よりも、より手前で「描画元データが生成されていない/更新されていない」可能性が高い。
2. 特に `processCommandBuffer()` 到達可否 (sharedMem 成立可否) と、`GX::ProcessCommandList` が実際に何件流れているかの観測が必要。
3. `ColourBufferLoc` の VRAM base 補正は既に入っているため、アドレス計算の一次不整合は優先度低。

## 次の調査提案

- `GSP::GPU::registerInterruptRelayQueue` 成功時に `threadIndex` / 返却ハンドルを 1 回だけ強制ログ。
- `Kernel::mapMemoryBlock` で `GSPSharedMemHandle` が map された瞬間に 1 回だけログ。
- `processCommandBuffer` で `commandsLeft > 0` の最初の数件だけ cmd id を出す (毎フレーム大量出力は避ける)。
- `GX::ProcessCommandList` 入口でコマンドバッファ先頭アドレスとワード数をダンプ。

この 4 点を入れると、

- 「共有メモリ未接続」
- 「接続済みだがコマンド未投入」
- 「コマンド投入済みだが PICA 反映なし」

のどこで止まっているかを短時間で切り分けられる。
