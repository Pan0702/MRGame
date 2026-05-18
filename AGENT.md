# UE5 Agent Skills — Routing Guide

このファイルはUnreal Engine 5のスキルファイル群のルーティング定義です。
タスクに応じて対応するスキルファイルを読み込んでください。

スキルファイルのパスはプロジェクトの配置に合わせて変更してください。
例: `./skills/ue-cpp-foundations.txt`

---

## スキル一覧と使用タイミング

### C++ 基礎
**ファイル:** `./skills/ue-cpp-foundations.txt`
**使用タイミング:** UPROPERTY / UFUNCTION / UCLASS / USTRUCT / UENUM を書くとき、TArray / TMap / TSet などのコンテナ、デリゲート、FString / FName / FText、スマートポインタ（TObjectPtr / TWeakObjectPtr）、UObjectのガベージコレクション、UE_LOG、サブシステム全般

---

### モジュール・ビルドシステム
**ファイル:** `./skills/ue-module-build-system.txt`
**使用タイミング:** Build.cs / Target.cs の編集、モジュール作成、プラグイン構成、「unresolved external symbol」「cannot open include file」などのビルドエラー、IWYU違反、APIマクロ不足

---

### アクター・コンポーネント設計
**ファイル:** `./skills/ue-actor-component-architecture.txt`
**使用タイミング:** Actor / Component の設計・実装、BeginPlay / Tick / EndPlay のライフサイクル、SpawnActor、CreateDefaultSubobject、UActorComponent / USceneComponent、アタッチメント、インターフェース（UINTERFACE）

---

### ゲームプレイフレームワーク
**ファイル:** `./skills/ue-gameplay-framework.txt`
**使用タイミング:** GameMode / GameState / PlayerController / PlayerState / Pawn / Character / GameInstance の設計、マッチフロー、プレイヤー管理・スポーン、ゲームルール全般

---

### キャラクター移動
**ファイル:** `./skills/ue-character-movement.txt`
**使用タイミング:** CharacterMovementComponent（CMC）の拡張、移動モード（歩行・落下・水中・飛行・カスタム）、ネットワーク予測（FSavedMove）、ルートモーション、LaunchCharacter、床判定・段差乗り越え

---

### 入力システム
**ファイル:** `./skills/ue-input-system.txt`
**使用タイミング:** Enhanced Input、InputAction / InputMappingContext、キーバインド・ゲームパッド対応、ETriggerEvent、Hold / Tap / Combo トリガー、DeadZone / Scalar / SwizzleAxis モディファイア、カスタムトリガー・モディファイア実装

---

### ゲームプレイアビリティシステム（GAS）
**ファイル:** `./skills/ue-gameplay-abilities.txt`
**使用タイミング:** GameplayAbility / GameplayEffect / AttributeSet / GameplayTag、バフ・デバフ・クールダウン、アビリティタスク（AbilityTask）、GASのセットアップ・レプリケーション設定

---

### AIとナビゲーション
**ファイル:** `./skills/ue-ai-navigation.txt`
**使用タイミング:** AIController / BehaviorTree / Blackboard / EQS / NavMesh、AIPerception、パスファインディング、StateTree（AI用途）、SmartObject

---

### ステートツリー
**ファイル:** `./skills/ue-state-trees.txt`
**使用タイミング:** UStateTree / StateTreeTask / StateTreeCondition / StateTreeEvaluator / StateTreeSchema、AI以外のゲームロジック・UI状態管理、MassEntityとの統合

---

### アニメーションシステム
**ファイル:** `./skills/ue-animation-system.txt`
**使用タイミング:** AnimInstance / AnimMontage / BlendSpace / AnimStateMachine / AnimNotify、IK、AnimGraph、スケルタルメッシュアニメーション、リンクアニムグラフ、ロコモーションセットアップ

---

### 物理・コリジョン
**ファイル:** `./skills/ue-physics-collision.txt`
**使用タイミング:** LineTrace / SweepTrace / Overlap、コリジョンチャンネル設定、物理シミュレーション（Chaos）、OnHit / OnBeginOverlap、物理アセット

---

### ネットワーク・レプリケーション
**ファイル:** `./skills/ue-networking-replication.txt`
**使用タイミング:** DOREPLIFETIME / GetLifetimeReplicatedProps、RPC（Server / Client / NetMulticast）、ネットロール（Authority / Proxy）、予測・同期、マルチプレイヤー全般

---

### UIシステム（UMG / Slate / CommonUI）
**ファイル:** `./skills/ue-ui-umg-slate.txt`
**使用タイミング:** UUserWidget / HUD / BindWidget、Slate、CommonUIプラグイン、メニュー・モーダル・インワールドウィジェット、入力モード管理

---

### マテリアル・レンダリング
**ファイル:** `./skills/ue-materials-rendering.txt`
**使用タイミング:** マテリアル / シェーダー / MID（動的マテリアルインスタンス）、ポストプロセス / レンダーターゲット、デカール、Nanite / Lumen、マテリアルパラメータコレクション

---

### Niagaraエフェクト
**ファイル:** `./skills/ue-niagara-effects.txt`
**使用タイミング:** Niagaraパーティクルシステム・エミッタの操作、ユーザーパラメータの設定、データインターフェース（SkeletalMesh / StaticMesh / Curve / Array）、OnSystemFinished、VFXパフォーマンスチューニング

---

### オーディオシステム
**ファイル:** `./skills/ue-audio-system.txt`
**使用タイミング:** UAudioComponent / PlaySoundAtLocation / SoundCue / MetaSound、アッテネーション / サブミックス / コンカレンシー、空間オーディオ、BGM・環境音システム

---

### データアセット・テーブル
**ファイル:** `./skills/ue-data-assets-tables.txt`
**使用タイミング:** UDataAsset / UDataTable、TSoftObjectPtr / TSoftClassPtr、非同期ロード（AsyncLoad / StreamableManager）、AssetManager、データ駆動設計

---

### セーブ・シリアライゼーション
**ファイル:** `./skills/ue-serialization-savegames.txt`
**使用タイミング:** USaveGame / SaveGameToSlot / LoadGameFromSlot、FArchive、プレイヤー進行状況の永続化、マルチユーザーセーブ、設定データの保存

---

### ワールド・レベルストリーミング
**ファイル:** `./skills/ue-world-level-streaming.txt`
**使用タイミング:** World Partition / Level Streaming / Data Layer、OpenLevel / ServerTravel / シームレストラベル、WorldSubsystem、レベルインスタンス、HLOD、オープンワールド設計

---

### プロシージャル生成
**ファイル:** `./skills/ue-procedural-generation.txt`
**使用タイミング:** PCGフレームワーク、ProceduralMeshComponent / HISM / スプライン、ランタイムメッシュ生成、ノイズ・地形生成、ダンジョン生成

---

### Massエンティティ
**ファイル:** `./skills/ue-mass-entity.txt`
**使用タイミング:** MassEntity / MassProcessor / MassFragment / MassTag / MassObserver / MassSpawner、大規模エンティティシミュレーション（群衆・弾丸・インスタンスなど）、ECSパターン

---

### シーケンサー・シネマティクス
**ファイル:** `./skills/ue-sequencer-cinematics.txt`
**使用タイミング:** LevelSequence / カットシーン / カメラワーク / シーケンサーイベント / Movie Render Queue、ダイアログ演出、インゲームカメラ制御

---

### エディタ拡張ツール
**ファイル:** `./skills/ue-editor-tools.txt`
**使用タイミング:** エディタユーティリティウィジェット（Blutility）、DetailCustomization / PropertyCustomization、エディタモード、UToolMenus、アセット操作スクリプト、EditorSubsystem

---

### ゲームフィーチャー・モジュラーゲームプレイ
**ファイル:** `./skills/ue-game-features.txt`
**使用タイミング:** Game Featureプラグイン、GameFeatureAction / GameFeatureData、GameFrameworkComponentManager、Lyraスタイルのモジュラーアーキテクチャ、UPawnComponent / UControllerComponent

---

### 非同期・スレッディング
**ファイル:** `./skills/ue-async-threading.txt`
**使用タイミング:** UE::Tasks::Launch / FAsyncTask / TaskGraph / ParallelFor / TFuture / TPromise、バックグラウンドスレッド処理、FCriticalSection / FRWLock、ゲームスレッドへのディスパッチ

---

### テスト・デバッグ
**ファイル:** `./skills/ue-testing-debugging.txt`
**使用タイミング:** オートメーションテスト / Functional Test、UE_LOG / ログカテゴリ、check / ensure / verify、DrawDebugLine / DrawDebugSphere などのデバッグ描画、コンソールコマンド、Unreal Insights / statコマンドによるプロファイリング

---

### プロジェクトコンテキスト設定
**ファイル:** `./skills/ue-project-context.txt`
**使用タイミング:** 初回セットアップ時、エンジンバージョン・モジュール構成・使用プラグイン・コーディング規約などのプロジェクト固有情報を記録する。他の全スキルがこのコンテキストを参照する。

---

## 使い方のルール

1. タスク開始時にこのファイルを参照し、該当スキルファイルを特定する
2. 複数のスキルが関係する場合はすべて読み込む（例：GAS + ネットワーク → 両方読む）
3. 初回または設定変更時は必ず `./skills/ue-project-context.txt` を先に読む
4. スキルファイルに記載された「情報収集」セクションに従い、不明点はユーザーに確認する
5. 架空のAPIは使用しない — スキルファイルに記載された正確なシグネチャのみ使用する
