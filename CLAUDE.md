# CLAUDE.md — プロジェクト指示書

## プロジェクト概要

- **ジャンル:** スラッシュアクション（VR）
- **エンジン:** Unreal Engine 5.7
- **ターゲットプラットフォーム:** Meta Quest 3 / 3s（Android / OpenXR）
- **ネットワーク:** なし（シングルプレイヤーのみ）
- **言語:** C++メイン（Blueprintは最小限）

---

## スキルルーティング

タスク開始時は必ず `./AGENT.md` を読み、該当スキルファイルを特定してから作業すること。

---

## コーディング規約

- Epicの公式UEコーディング規約に従う
- クラス名: `U`（UObject系）/ `A`（Actor系）/ `F`（構造体）/ `I`（インターフェース）プレフィックス
- `TObjectPtr<>` を使用（生ポインタ禁止）
- `UPROPERTY` / `UFUNCTION` マクロを必ず付ける
- ログは `UE_LOG` を使用、カテゴリを明示する
- 架空のAPIは絶対に使わない。不明な場合はスキルファイルを確認すること

---

## プラットフォーム制約（Meta Quest 3）

- **レンダリング:** Mobile Forward Renderer を使用（Lumen・Nanite 使用不可）
- **ポリゴン数・ドローコール:** 厳しく管理する（モバイルGPU制約）
- **入力:** OpenXRコントローラー / ハンドトラッキング（Enhanced Input経由）
- **物理:** 軽量に保つ、複雑な物理シミュレーション禁止
- **Niagara:** パーティクル数を最小限に（GPUパーティクル注意）
- **ネットワーク:** 不要、レプリケーション設定は省略

---

## VR固有の注意事項

- フレームレートは **72Hz / 90Hz** をターゲットに保つ
- **コンフォート優先:** 急激なカメラ移動・FOV変更を避ける
- Unreal の **VRTemplate** または **OpenXRプラグイン** をベースにする
- ルームスケール / スタンディングプレイを想定

---

## 優先して使うスキル

| 場面 | スキルファイル |
|------|--------------|
| C++全般 | `./skills/ue-cpp-foundations.txt` |
| キャラクター・剣の動き | `./skills/ue-character-movement.txt` |
| コントローラー入力 | `./skills/ue-input-system.txt` |
| 斬撃エフェクト | `./skills/ue-niagara-effects.txt` |
| 斬撃音・SE | `./skills/ue-audio-system.txt` |
| 当たり判定 | `./skills/ue-physics-collision.txt` |
| アニメーション | `./skills/ue-animation-system.txt` |
| ゲームフロー管理 | `./skills/ue-gameplay-framework.txt` |
| アビリティ・コンボ | `./skills/ue-gameplay-abilities.txt` |
| デバッグ・プロファイル | `./skills/ue-testing-debugging.txt` |

---

## やってはいけないこと

- Lumen / Nanite の使用
- ネットワーク・レプリケーション関連コードの追加
- モバイルで重い物理・GPUパーティクルの多用
- Blueprint のみでのロジック実装（C++で書くこと）
- 架空・未確認のUE APIの使用
- **ユーザーからの明示的な指示がない限り、コードを変更しない（実装はユーザー自身が行う）。相談・方針検討・コードレビューが基本のスタンス**
