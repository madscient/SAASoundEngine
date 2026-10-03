# SAASoundEngine 作業計画・経緯

AI 向けの文書。決定の根拠と前提、見送った案、確かめた結果、未決事項、実行経緯を書く。
利用者向けの現在の仕様は `README.md` にある。

確度の印: **確認済み** = 走らせて確かめた（方法を添える）／**未検証** = 作ったが走らせていない／
**推測** = 出典を示せない（根拠を添える）。

## §0 現在地

- FmEngineApi の改訂に、FMEngineTest `20c4923` まで追随した（§2、§4）
- **次の一手: §3（クロックの違う SAA チップを混ぜたときの扱い）を利用者に確認する**

## §1 仕様の出どころ

| 対象 | 正 | 時点 |
|---|---|---|
| API の仕様 | madscient/FMEngineTest `docs/FmEngineApi.md` | `20c4923` |
| 各エンジンに求める対応 | madscient/FMEngineTest `docs/CHANGELOG.md` | `20c4923` |
| ヘッダ | madscient/FMEngineTest `include/FmEngineApi.h`（本リポジトリの `src/FmEngineApi.h` はその写し） | `20c4923` |

ヘッダは一字も変えていない（**確認済み**: `git hash-object` が写し元の blob `206f723` と一致）。
置き場所は `src/` のままにした（ビルド設定の include パスを変えずに済む）。

ヘッダの写し元は `7d8ed2d` まで madscient/YMEngine の `src/FmEngineApi.h` だった。`20c4923` で
正本が FMEngineTest に移り、特定のエンジンに依らない書き方になった。

## §2 2026-10-02 FmEngineApi の改訂への追随

この節は FMEngineTest `866f4a3`・YMEngine `7d8ed2d` の時点の記録。部位ゲインと外部メモリの
関数の形は §4 で変わっている。

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

## §4 2026-10-03 FmEngineApi の改訂（名前での指定）への追随

読んだ改訂: FMEngineTest `20c4923`。部位と外部メモリを名前の文字列で指定する形になり、
`FmPart`・`FmEngine_GetPartMask`・`FmMemoryType`・`FmEngine_GetMemorySize` が無くなった。
外部メモリの関数は任意の組になり、必須シンボルは 14 個から 12 個になった。ヘッダの正本は
FMEngineTest の `include/FmEngineApi.h` に移った（§1）。

### 4.1 ヘッダを写し直す

`src/FmEngineApi.h` を FMEngineTest `20c4923` の `include/FmEngineApi.h` の写しにした。

### 4.2 外部メモリの関数のエクスポートをやめる

- 根拠: FMEngineTest の `docs/CHANGELOG.md`（`20c4923`）の「エンジンとアプリケーション側の対応」が、
  本エンジンを含むスタブの 5 本に「外部メモリの関数のエクスポートをやめる」を求めている。
  仕様書は「`FmEngine_GetMemoryCount` を持たずに `FmEngine_SetMemory` をエクスポートする DLL は、
  この仕様と互換性がない」とする
- 実装: `FmEngine_SetMemory`（常に `FM_ERR_UNAVAILABLE`）と `FmEngine_GetMemorySize`（常に 0）を
  消した。新しい呼び出し側は、`FmEngine_GetMemoryCount` が無い DLL を、外部メモリを持たない
  エンジンとして扱う
- 利用者の決定（2026-10-03）: エクスポートをやめる。対応前の呼び出し側からロードできなくなる
  ことは問題にしない
- 前提: 呼び出し側が `FmEngine_GetMemoryCount` の有無で判定すること（`20c4923` の仕様）。
  アプリケーション側も並行して追随すること（利用者による）
- 影響: `FmEngine_SetMemory` を必須として読み込む呼び出し側は、この DLL をロードできなくなる
  - **確認済み**: FMEngineTest `866f4a3` は `FmEngine_SetMemory not found in DLL` でロードに
    失敗する（§4.4）
  - **未検証**（ソースを読んだだけで、動かしていない）: madscient/FitomEmuIF `75d9542` は
    `FmEngine_SetMemory` が無いと例外を投げ、madscient/Y8960Sequencer `13050cb` はライブラリの
    open を失敗にする。FMEngineTest の CHANGELOG は、この 2 つを「`FmEngine_SetMemory` を必須
    として読むのをやめる」対応が要るアプリケーションに挙げている
- 見送った案: 外部メモリの 3 関数を組で、スタブとしてエクスポートする（`GetMemoryCount` は 0、
  `GetMemoryName` は NULL、`SetMemory` はどの名前にも `FM_ERR_INVALID_ARG`）。これも仕様に
  準拠する。見送った理由: 仕様の CHANGELOG がスタブのエンジンにエクスポートをやめることを
  求めており、利用者が互換性を保つ必要は無いとした。この案なら `FmEngine_SetMemory` のシンボルが残るので、それだけを必須として読む
  対応前の呼び出し側（上の FitomEmuIF と Y8960Sequencer）からもロードできる見込みだが、
  **未検証**。`FmEngine_GetMemorySize` も必須とする FMEngineTest `866f4a3` 以前は、どちらの
  案でもロードできない
- やり直しの値段: スタブで残す案に変えるのは、`src/SAASoundEngine.cpp` に 3 関数（20 行ほど）と、
  `README.md` の「任意シンボル」の行、この節

### 4.3 部位ゲインは引き続きエクスポートしない

仕様は、`FmEngine_GetPartCount` が無い DLL を「どのチップも部位を持たない」ものとして扱う。
SAA1099 は仕様の部位の表に無い。§2.2 の判断と同じで、関数の形が変わっただけ。

### 4.4 確認

**確認済み**（MSVC 19.51 で、変更前 `ca97218` と変更後の DLL をビルドし、§2.5 と同じ使い捨ての
ハーネスを新しいヘッダに合わせて直して叩いた。ハーネスはリポジトリに入れていない）:

- 「外部メモリの 4 シンボルをエクスポートしていない」「廃止された 2 シンボル
  （`GetMemorySize`、`GetPartMask`）をエクスポートしていない」の 2 項目が、変更前の DLL で落ち、
  変更後の DLL で通った。変更後は必須 12 シンボルがあり、部位ゲインの 4 シンボルが無い
- clock の検査（§2.5 と同じ 9 項目）は、変更前・変更後とも通った
- clock=8,000,000 で鳴らした出力（float、約 2 秒）が、変更前と変更後でバイト一致。
  §2.5 で書き出したものとも一致
- FMEngineTest `20c4923` をビルドし（configure で「header and spec list the same 20 symbols」）、
  `saa.json` を WAV に書き出した。変更前の DLL と変更後の DLL の WAV がバイト一致し、§2.5 の
  WAV とも一致。どちらの DLL でも `FmEngine_GetMemoryCount is not exported: ROM files will not
  be loaded.` が 1 行出る
- FMEngineTest `866f4a3` は、変更後の DLL を `FmEngine_SetMemory not found in DLL` でロード
  できず、変更前の DLL はロードできる

**未検証**:

- Linux / macOS でのビルド、`NMakefile` でのビルド（CMake の NMake ジェネレータでだけビルドした）
- FMEngineTest `20c4923` のリアルタイム再生（WAV への書き出しだけを走らせた）
