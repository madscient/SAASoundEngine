# SAASoundEngine 作業計画・経緯

AI 向けの文書。決定の根拠と前提、見送った案、確かめた結果、未決事項、実行経緯を書く。
利用者向けの現在の仕様は `README.md` にある。

確度の印: **確認済み** = 走らせて確かめた（方法を添える）／**未検証** = 作ったが走らせていない／
**推測** = 出典を示せない（根拠を添える）。

## §0 現在地

- FmEngineApi の改訂（FMEngineTest `866f4a3`、YMEngine `7d8ed2d`）に追随した（§2）
- **次の一手: §3（クロックの違う SAA チップを混ぜたときの扱い）を利用者に確認する**

## §1 仕様の出どころ

| 対象 | 正 | 時点 |
|---|---|---|
| API の仕様 | madscient/FMEngineTest `docs/FmEngineApi.md` | `866f4a3` |
| ヘッダ | madscient/YMEngine `src/FmEngineApi.h`（本リポジトリの `src/FmEngineApi.h` はその写し） | `7d8ed2d` |

ヘッダは一字も変えていない（**確認済み**: `git hash-object` が写し元の blob `6fc64fe` と一致）。

ヘッダの注記のうち YMEngine 固有で、本エンジンに当てはまらないもの: `FmEngine_SetMemory` の
「data は複製せず参照する」「チップからの書き込みは捨てる」、`FmEngine_GetMemorySize` の
「割り当てたブロックの大きさの合計」。本エンジンは外部メモリを持たず、`SetMemory` は常に
`FM_ERR_UNAVAILABLE`、`GetMemorySize` は常に 0。写しの規則に従い、ヘッダは直さない。

## §2 2026-10-02 FmEngineApi の改訂への追随

読んだ改訂: FMEngineTest の `e39b206`（部位ゲイン）、`e002890`・`c0589c1`（外部メモリの割り当て）、
`866f4a3`（clock=0 の廃止）。FMEngineTest の `docs/CHANGELOG.md` は、本リポジトリ `4173159` を
「clock=0 を既定値に読み替えるので仕様に準拠しない」エンジンに数えていた。

### 2.1 clock=0 を FM_ERR_INVALID_ARG にする

- 仕様（`866f4a3`、利用者の決定）: エンジンは既定のクロックを持たない。clock=0 は `FM_ERR_INVALID_ARG`
- 実装: 既定クロックの定数（8 MHz）を消し、`AddChip` はチップ名を見る前に clock=0 を弾く。
  参照実装 YMEngine `7d8ed2d` と同じ順で、未知のチップ名でも clock=0 なら `FM_ERR_INVALID_ARG`
- FMEngineTest の `saa.json` は `clock: 8000000`（変更前の既定値と同じ）を持つので、出力は変わらない

### 2.2 任意シンボルはエクスポートしない

- 部位ゲイン: 仕様の部位の表に SAA1099 は無い。仕様は「表に無いチップは部位を持たない」
  「シンボルが無いエンジンでは、どのチップも部位を持たないものとして扱う」とするので、
  エクスポートしないことと `GetPartMask` が 0 を返すことは、呼び出し側から見て同じ
- `FmEngine_SetMemoryEx`: 仕様の判断基準は「外部メモリのバスが外に出ているチップ」。SAA1099 は
  外部メモリを持たない
- 前提: 仕様の部位の表に SAA1099 が載らない限り成立する。部位ゲインが必須に上がった場合は、
  スタブ（`GetPartMask` は 0、`Set/GetPartGain` は `FM_ERR_INVALID_ARG`）で準拠できる
- 見送った案: `GetPartMask` だけを、0 を返すスタブとしてエクスポートする。理由: 仕様上
  シンボルが無いのと同じ意味で、呼び出し側に足すものが無い

### 2.3 Write / SetGain / GetGain と Generate を排他する

- 仕様: `Write`・`SetGain`・`GetGain` はオーディオコールバックスレッドと並行して呼べる。
  変更前のソースは「FMEngineTest の使い方では問題ない」として排他していなかった。
  FMEngineTest のリアルタイム再生はストリームを開始してからチャンネルを鳴らす（**確認済み**:
  起動時の出力で `Stream started` の後に CH の行が出る）ので、`Write` と `Generate` は並行しうる
- 実装: エンジンに mutex を 1 つ持たせ、この 4 関数の中でだけ取る。仕様が並行呼び出しを
  認めていない関数（`AddChip` など）では取らない
- 改訂で増えた要件ではなく、前からの不準拠（ヘッダは変更前から `Write` / `SetGain` を
  並行呼び出し可能としていた）。追随と同時に直した

### 2.4 その他

- `src/SAASoundEngine.cpp` 冒頭の、手で打つビルド手順のコメントを消した。submodule の場所が
  `../SAASound` になっていて実際（`extern/SAASound`）と合わず、`README.md` の手順と重複していた
- `README.md` の起動時出力の例の CH0 の行を、FMEngineTest `866f4a3` の実際の出力に合わせた
- `README.md` のライセンス欄: `FmEngineApi.h` の写し元を FMEngineTest から YMEngine に改めた
  （YMEngine も MIT。LICENSE を読んだ）

### 2.5 確認

**確認済み**（MSVC 19.51 で、変更前 `4173159` と変更後の DLL をビルドし、DLL を実行時にロード
する使い捨てのハーネスで叩いた。ハーネスはリポジトリに入れていない）:

- clock=0 の検査 4 項目（`FM_ERR_INVALID_ARG` を返す、`out_id` に書かない、チップが追加されない、
  未知のチップ名でも `FM_ERR_INVALID_ARG`）が、変更前の DLL で落ち、変更後の DLL で通った
- 変更後: 必須 14 シンボルがあり、任意 4 シンボルが無い。clock=8,000,000 で native_rate 15,625、
  7,159,090 で clock/512
- 変更前に clock=0 で鳴らした出力と、変更後に clock=8,000,000 で鳴らした出力（float、約 2 秒、
  ほぼ全サンプルが非ゼロ）がバイト一致。clock を 7,159,090 にすると出力が変わるので、
  この比較はクロックの違いを検出できる
- FMEngineTest `866f4a3` をビルドし、`saa.json` を WAV に書き出した。変更前の FMEngineTest・
  変更前の `saa.json`（clock なし）・変更前の DLL の WAV と、`866f4a3` の FMEngineTest・
  `saa.json`・変更後の DLL の WAV がバイト一致。`clock` を消した `saa.json` は `[SKIP]` になる

**未検証**:

- 排他によってデータ競合が無くなったこと。ソースを読んで、4 関数がすべて同じ mutex の中で
  SAASound に触ることだけを見た。並行に叩く試験はしていない（変更前でも落ちるとは限らず、
  通っても証拠にならないため）
- Linux / macOS でのビルド、`NMakefile` でのビルド（CMake の NMake ジェネレータでだけビルドした）

## §3 未決: クロックの違う SAA チップを混ぜたとき

**確認済み**（§2.5 のハーネスで、chip0 に oct=4・offset=0xFC のトーンを鳴らし、後から追加した
チップはゲイン 0 にして、ゼロ交差で chip0 の周波数を測った）:

| chip0 | 後から追加したチップ | chip0 の音程 | chip0 のクロックでの期待値 |
|---|---|---|---|
| 8 MHz | なし | 965.3 Hz | 965.3 Hz |
| 4 MHz | なし | 482.6 Hz | 482.6 Hz |
| 8 MHz | 4 MHz（同じエンジン） | 482.6 Hz | 965.3 Hz |
| 4 MHz | 8 MHz（同じエンジン） | 965.3 Hz | 482.6 Hz |
| 8 MHz | 4 MHz（別のエンジンハンドル） | 482.6 Hz | 965.3 Hz |

変更前の DLL でも同じ（8 MHz の後に 4 MHz で 482.6 Hz）。今回の追随で入ったものではない。
clock が必須になり、呼び出し側が必ずクロックを選ぶようになったので記録する。

原因（ソースを読んだ。SAASound `3d92322`）: `CSAAFreq::m_FreqTable` と `m_nClockRate` が static で、
`SetClockRate` と、`CreateCSAASound` が作るオブジェクトのコンストラクタ（8 MHz で作る）のたびに
作り直される。トーンの増分 `m_nAdd` は `SetAdd` でこのテーブルから引き、`SetAdd` はレジスタ
書き込み時と生成中の両方から呼ばれる。ノイズの増分は `CSAANoise` のインスタンスごとに持つので、
チップごとのクロックに従う（**未検証**: 読んだだけで、測っていない）。

付随: 別スレッドの別エンジンで `AddChip` すると、static テーブルの書き換えが他のエンジンの
`Write` / `Generate` と競合する（**未検証**: ソースからの判断）。§2.3 の mutex はエンジンごと
なので防げない。

`README.md` には「複数の SAA チップは、すべて同じクロックで使う」と書いた。直し方の候補
（利用者に未確認。どれも未着手）:

1. 文書だけ（現状）
2. 生きている SAA チップとクロックが違う `AddChip` を拒否する（プロセス全体で数える）
3. チップごとに、`Write` と `GenerateMany` の直前で `SetClockRate` を呼んでテーブルを切り替え、
   プロセス全体の mutex で囲む（`SetClockRate` はクロックが前回と同じならテーブルを作り直さない）
4. SAASound 側を直す（submodule の改造、または upstream への提案）
