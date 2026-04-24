# GX描画到達性の静的調査 (2026-04-24)

## 結論
描画コマンド (`GX::ProcessCommandList`) に到達しない場合、最も起きやすい静的要因は **GSP共有メモリが有効化されていないこと**。

この実装では、共有メモリが未設定 (`sharedMem == nullptr`) だと `TriggerCmdReqQueue` 経由で `processCommandBuffer()` に進んでも即 return し、
コマンド (`ProcessCommandList`, `MemoryFill` など) の switch に到達しない。

## 依存フロー
1. `GSP::GPU::RegisterInterruptRelayQueue` で GSP shared memory handle を受け取る。
2. クライアント側がその handle を `MapMemoryBlock` する。
3. `Kernel::mapMemoryBlock` が `serviceManager.setGSPSharedMem(ptr)` を呼び、`GPUService::sharedMem` が有効になる。
4. その後 `TriggerCmdReqQueue` で command buffer を処理できる。

## 今回の修正ポイント
- `RegisterInterruptRelayQueue` の返却時、translation descriptor が 0 だったため、
  共有メモリ handle の IPC 返却が不正になる可能性がある。
- 他サービス実装との整合に合わせ、descriptor を `0x04000000` (copy handle) に修正。

## 期待される効果
クライアントが GSP shared memory handle を正しく受け取りやすくなり、
`MapMemoryBlock` -> `setGSPSharedMem` が成立して GX command 処理経路へ到達できる可能性が上がる。

## 追加確認項目
- ログで `GSP::GPU::RegisterInterruptRelayQueue` が呼ばれているか
- 続いて `MapMemoryBlock(block=GSPSharedMemHandle, ...)` が呼ばれているか
- `TriggerCmdReqQueue` で `processCommandBuffer skipped because shared memory is null` が消えるか
