# SAASoundEngine 作業計画・経緯

AI 向けの文書。決定の根拠と前提、見送った案、確かめた結果、未決事項、実行経緯を書く。
利用者向けの現在の仕様は `README.md` にある。

確度の印: **確認済み** = 走らせて確かめた（方法を添える）／**未検証** = 作ったが走らせていない／
**推測** = 出典を示せない（根拠を添える）。

## §0 現在地

- FmEngineApi の改訂に、FMEngineTest `0c22d67` まで追随した（§2、§4）
- `FmEngine_AddChip` は clock が 8,000,000 のときだけ受け付ける（§5）
- **次の一手: FMEngineTest が `0c22d67` の後に API を改訂している（`831947d`
  「Remove FmEngine_GetNativeRate from the API」。ヘッダの blob は `7deb007`）。未追随。
  利用者に確認してから追随する**
- 未決: ハイパスフィルタの状態の共有（§3.3）

## §1 仕様の出どころ

| 対象 | 正 | 時点 |
|---|---|---|
| API の仕様 | madscient/FMEngineTest `docs/FmEngineApi.md` | `0c22d67` |
| 各エンジンに求める対応 | madscient/FMEngineTest `docs/CHANGELOG.md` | `0c22d67` |
| ヘッダ | madscient/FMEngineTest `include/FmEngineApi.h`（本リポジトリの `src/FmEngineApi.h` はその写し） | `0c22d67` |

ヘッダは一字も変えていない（**確認済み**: `git hash-object` が写し元の blob `206f723` と一致）。
置き場所は `src/` のままにした（ビルド設定の include パスを変えずに済む）。

FMEngineTest のコミットは、2026-10-03 に同じ内容のまま別のハッシュになった。この文書の参照は
新しいハッシュに付け替えてある（**確認済み**: 新旧のコミットで tree が一致。`20c4923`→`0c22d67`、
`866f4a3`→`a17c372`、`c0589c1`→`e3632cc`、`e002890`→`98d73ec`、`e39b206`→`58e62da`）。
本リポジトリのコミットメッセージ（`ca97218`、`bb0c0cd`）には古いハッシュが残っている。

ヘッダの写し元は `7d8ed2d` まで madscient/YMEngine の `src/FmEngineApi.h` だった。`0c22d67` で
正本が FMEngineTest に移り、特定のエンジンに依らない書き方になった。

## §2 2026-10-02 FmEngineApi の改訂への追随

この節は FMEngineTest `a17c372`・YMEngine `7d8ed2d` の時点の記録。部位ゲインと外部メモリの
関数の形は §4 で変わっている。

読んだ改訂: FMEngineTest の `58e62da`（部位ゲイン）、`98d73ec`・`e3632cc`（外部メモリの割り当て）、
`a17c372`（clock=0 の廃止）。FMEngineTest の `docs/CHANGELOG.md` は、本リポジトリ `4173159` を
「clock=0 を既定値に読み替えるので仕様に準拠しない」エンジンに数えていた。

### 2.1 clock=0 を FM_ERR_INVALID_ARG にする

- 仕様（`a17c372`、利用者の決定）: エンジンは既定のクロックを持たない。clock=0 は `FM_ERR_INVALID_ARG`
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
- `README.md` の起動時出力の例の CH0 の行を、FMEngineTest `a17c372` の実際の出力に合わせた
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
- FMEngineTest `a17c372` をビルドし、`saa.json` を WAV に書き出した。変更前の FMEngineTest・
  変更前の `saa.json`（clock なし）・変更前の DLL の WAV と、`a17c372` の FMEngineTest・
  `saa.json`・変更後の DLL の WAV がバイト一致。`clock` を消した `saa.json` は `[SKIP]` になる

**未検証**:

- 排他によってデータ競合が無くなったこと。ソースを読んで、4 関数がすべて同じ mutex の中で
  SAASound に触ることだけを見た。並行に叩く試験はしていない（変更前でも落ちるとは限らず、
  通っても証拠にならないため）
- Linux / macOS でのビルド、`NMakefile` でのビルド（CMake の NMake ジェネレータでだけビルドした）

## §3 複数の SAA チップで共有される SAASound の状態

SAASound（`3d92322`）は、インスタンスをまたいで共有する状態を 2 つ持つ。トーンの周波数テーブル
（§3.1）と、ハイパスフィルタの状態（§3.3）。どちらも本エンジンの変更で入ったものではない
（§3.1 は `4173159` の DLL でも同じ結果）。§3.2 は 1 チップでも起きる。

**§3.1 と §3.2 は、§5 の決定（8 MHz 以外のクロックを断る）で起きなくなった。** テーブルも
コンストラクタが作る値も常に 8 MHz なので、食い違いが生じない。下の測定は `bb0c0cd`（8 MHz
以外も受け付けていた版）のもので、決定の根拠として残す。**§3.3 は未決のまま。**

測定は 2026-10-03、`bb0c0cd` の DLL を使い捨てのハーネスで叩いた（リポジトリには入れていない）。
周波数は、CH0 に oct=4・offset=0xFC のトーン（8 MHz で 965.3 Hz）を鳴らし、ゼロ交差で測った。

### 3.1 トーンの周波数テーブル（クロックの違うチップを混ぜたとき）

仕組み（ソースを読んだ）:

- `CSAAFreq::m_FreqTable[2048]` と `m_nClockRate` が static。DLL に 1 つ
- テーブルを作り直すのは `CSAAFreq::_SetClockRate`（クロックが今の値と違うときだけ）。呼ばれるのは
  `SetClockRate` と、チップを作るときのコンストラクタ（8 MHz を渡す）。本エンジンの `AddChip` は
  `CreateCSAASound` → `SetClockRate(clock)` の順なので、テーブルはいったん 8 MHz になってから
  clock になる
- 各オシレータは、テーブルから引いた増分を自分の `m_nAdd` に写して持つ。写し直す（`SetAdd`）のは、
  周波数レジスタ（offset 0x08–0x0D、octave 0x10–0x12）に書いた値が次の半周期で反映されるとき、
  同期ビット（0x1C bit1）を立てたとき、コンストラクタ。テーブルを作り直しても `m_nAdd` は変わらない

したがって、**あるオシレータの音程は、その周波数が最後に反映された時点のテーブルで決まる**。

**確認済み**:

| 手順 | A（8 MHz）の音程 |
|---|---|
| A だけ | 965.2 Hz |
| A を鳴らしてから B（4 MHz）を追加。A には触らない | 965.3 Hz（変わらない） |
| 続けて A に同じ周波数を書き直す | 482.6 Hz |
| A と B（4 MHz）を追加してから A を鳴らす（同じエンジン／別のエンジンハンドル） | 482.6 Hz |
| A 作成 → 別エンジンに B（4 MHz）を作成して破棄 → A を鳴らす | 482.6 Hz（破棄しても戻らない） |
| 続けて別エンジンに C（8 MHz）を作成 → A に書き直す | 965.2 Hz |
| A と B（7,159,090 Hz）を追加してから A を鳴らす | 863.8 Hz（−192 セント） |
| 4 MHz のチップだけ | 482.6 Hz（正しい） |

**未検証**（読んだだけ）:

- エンベロープ: 内部クロックのとき、オシレータ 1 と 4 の半周期で進む。トーンと同じく引きずられる
- ノイズ（ソース 3）: オシレータ 0 と 3 の半周期で進む。トーンと同じく引きずられる。
  ソース 0–2 はインスタンスごとの値を使うので、他のチップのクロックには引きずられない
- octave レジスタは 1 本で 2 つのオシレータを受け持つので、片方の octave を書くと、もう片方も
  その時点のテーブルで写し直される
- 別スレッドの別エンジンで `AddChip` すると、テーブルの作り直しが他のエンジンの `Write` /
  `Generate` の `SetAdd` と競合する。§2.3 の mutex はエンジンごとなので防げない。作り直しの
  途中（8 MHz になっている間を含む）に反映された周波数は、その時点の値になる

### 3.2 クロックを設定した直後に残る 8 MHz の値（1 チップでも起きる）

チップを作るとコンストラクタが 8 MHz で値を作り、その後の `SetClockRate` は写しを更新しない。

**確認済み**（比較ごとに新しいプロセスで走らせた）:

- ノイズ（ソース 0–2）: 増分を作り直すのは reg 0x16 の書き込みだけ。4 MHz のチップで reg 0x16 を
  書かずに鳴らしたノイズは、8 MHz のチップの出力とバイト一致した（交差 15,630 回/秒）。
  reg 0x16 に 0 を書くと 7,807 回/秒になる
- トーン: 周波数レジスタを一度も書いていないオシレータ（oct=0・offset=0）は、4 MHz のチップでも
  30.58 Hz（8 MHz の値）で鳴る。offset に 0 を書くと 15.29 Hz になる

### 3.3 ハイパスフィルタの状態（クロックが同じでも起きる）

仕組み（ソースを読んだ）: `CSAASoundInternal::GenerateMany` の中の `static double
filterout_z1_left_mixed / filterout_z1_right_mixed` が、直流成分の推定値。出力は「入力 − 推定値」。
static なので全インスタンスで 1 つ。本エンジンは `Generate` のたびに各チップの `GenerateMany` を
順に呼ぶので、推定値は全チップの直流成分の平均に寄る。SAA1099 の出力は片極性で、鳴っている
チップは直流成分を持つ。

**確認済み**（比較ごとに新しいプロセスで走らせた。A はトーンを鳴らし、B は何も書かない。512
サンプルずつ生成し、後半 0.5 秒を見た）:

| 構成 | 出力の平均（直流） | RMS |
|---|---|---|
| A だけ | +0.0001 | 0.0820 |
| A + B、ゲインはどちらも 1 | +0.0001 | 0.0820（A だけとの差は RMS 0.0005、−44 dB） |
| A + B、B のゲインを 0 | +0.0412 | 0.0918 |
| A + B、A のゲインを 0（無音の B だけを聞く） | −0.0411 | 0.0413 |

- チップごとの出力には「自分の直流 − 全チップの平均」の直流が乗る。無音のチップも直流を出す
- ゲインが全チップで同じなら、ミックスでは打ち消し合う。ゲインが違うと直流が残る
- B のクロックを 4 MHz にしても同じ結果（差は RMS 0.0005）
- 状態はチップを破棄しても残る。同じプロセスで続けて作ったチップは、前のチップが残した
  推定値から始まる（同じ手順の出力が、プロセス内の 2 回目ではバイト一致しなかった）

### 3.4 直し方の候補

§3.1 と §3.2 は、利用者がこの表に無い案（8 MHz 以外を断る。§5）を選んだ。表の §3.1・§3.2 の
候補はどれも見送り。§3.3 の候補（d、e）は利用者に未確認で、未着手、効果は**未検証**。
`README.md` に §3.3 は書いていない。

| 対象 | 候補 | 値段 |
|---|---|---|
| §3.2 | `AddChip` で `SetClockRate` の後に `Clear()` を呼ぶ。`Clear()` は同期ビットを立ててから全レジスタに 0 を書くので、トーンとノイズの写しが作り直される見込み | `src/SAASoundEngine.cpp` に 1 行と試験 |
| §3.1 | (a) 文書だけ | なし |
| §3.1 | (b) 生きている SAA チップとクロックが違う `AddChip` を拒否する（プロセス全体で数える）。返すエラーコードは外に出る値 | cpp に数十行、README |
| §3.1 | (c) チップごとに、`Write` と `GenerateMany` の直前で `SetClockRate` を呼んでテーブルを切り替え、プロセス全体の mutex で囲む（クロックが同じならテーブルは作り直されない） | cpp に数十行、README の制限の段落を消す |
| §3.3 | (d) SAASound のハイパスを切り、チップごとの状態でエンジン側が直流を除く。SAASound は 16bit に丸めて切り詰めてから返すので、ハイパス無しでは片極性のまま切り詰めに当たりうる。出力は今とバイト一致しなくなる | cpp に十数行、出力レベルの確認 |
| §3.1・§3.3 | (e) SAASound 側を直す（static をメンバにする。submodule の改造、または upstream への提案） | 最も大きい |

## §4 2026-10-03 FmEngineApi の改訂（名前での指定）への追随

読んだ改訂: FMEngineTest `0c22d67`。部位と外部メモリを名前の文字列で指定する形になり、
`FmPart`・`FmEngine_GetPartMask`・`FmMemoryType`・`FmEngine_GetMemorySize` が無くなった。
外部メモリの関数は任意の組になり、必須シンボルは 14 個から 12 個になった。ヘッダの正本は
FMEngineTest の `include/FmEngineApi.h` に移った（§1）。

### 4.1 ヘッダを写し直す

`src/FmEngineApi.h` を FMEngineTest `0c22d67` の `include/FmEngineApi.h` の写しにした。

### 4.2 外部メモリの関数のエクスポートをやめる

- 根拠: FMEngineTest の `docs/CHANGELOG.md`（`0c22d67`）の「エンジンとアプリケーション側の対応」が、
  本エンジンを含むスタブの 5 本に「外部メモリの関数のエクスポートをやめる」を求めている。
  仕様書は「`FmEngine_GetMemoryCount` を持たずに `FmEngine_SetMemory` をエクスポートする DLL は、
  この仕様と互換性がない」とする
- 実装: `FmEngine_SetMemory`（常に `FM_ERR_UNAVAILABLE`）と `FmEngine_GetMemorySize`（常に 0）を
  消した。新しい呼び出し側は、`FmEngine_GetMemoryCount` が無い DLL を、外部メモリを持たない
  エンジンとして扱う
- 利用者の決定（2026-10-03）: エクスポートをやめる。対応前の呼び出し側からロードできなくなる
  ことは問題にしない
- 前提: 呼び出し側が `FmEngine_GetMemoryCount` の有無で判定すること（`0c22d67` の仕様）。
  アプリケーション側も並行して追随すること（利用者による）
- 影響: `FmEngine_SetMemory` を必須として読み込む呼び出し側は、この DLL をロードできなくなる
  - **確認済み**: FMEngineTest `a17c372` は `FmEngine_SetMemory not found in DLL` でロードに
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
  **未検証**。`FmEngine_GetMemorySize` も必須とする FMEngineTest `a17c372` 以前は、どちらの
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
- FMEngineTest `0c22d67` をビルドし（configure で「header and spec list the same 20 symbols」）、
  `saa.json` を WAV に書き出した。変更前の DLL と変更後の DLL の WAV がバイト一致し、§2.5 の
  WAV とも一致。どちらの DLL でも `FmEngine_GetMemoryCount is not exported: ROM files will not
  be loaded.` が 1 行出る
- FMEngineTest `a17c372` は、変更後の DLL を `FmEngine_SetMemory not found in DLL` でロード
  できず、変更前の DLL はロードできる

**未検証**:

- Linux / macOS でのビルド、`NMakefile` でのビルド（CMake の NMake ジェネレータでだけビルドした）
- FMEngineTest `0c22d67` のリアルタイム再生（WAV への書き出しだけを走らせた）

## §5 2026-10-03 8 MHz 以外のクロックを断る

### 決めたこと（利用者と決めた）

`FmEngine_AddChip` は、clock が 8,000,000 でなければ断る。

- 理由: §3.1（クロックの違うチップを混ぜると音程が引きずられる）と §3.2（8 MHz 以外では
  ノイズと未書き込みのオシレータに 8 MHz の値が残る）。SAASound は 8 MHz 以外を正しく扱えない
- 前提: SAASound（`3d92322`）の周波数テーブルが static で、`SetClockRate` が写しを更新しない
  こと。SAASound 側が直れば、この制限は外せる
- やり直しの値段: 制限を外すのは `src/SAASoundEngine.cpp` の 1 行と定数、`README.md` の
  「クロック」の行。外すなら §3.4 の候補（`Clear()` を呼ぶ、テーブルを切り替える）が要る

### 実装（こちらで決めた。利用者に明示的には確認していない）

- 返すのは `FM_ERR_INVALID_ARG`。仕様書（`0c22d67`）が `FmEngine_AddChip` の戻り値に挙げるのは
  `FM_OK` / `FM_ERR_INVALID_ARG` / `FM_ERR_UNKNOWN_CHIP` / `FM_ERR_ALLOC` で、扱えないクロックに
  当たるのはこれだけ。madscient/DBOPLEngine `c67d84f` も、扱えないクロックに同じ値を返す
- 検査の順: clock=0 → `FM_ERR_INVALID_ARG`、未知のチップ名 → `FM_ERR_UNKNOWN_CHIP`、
  8,000,000 以外 → `FM_ERR_INVALID_ARG`。8 MHz だけという制限はチップ SAA のものなので、
  チップ名を見た後に置いた（未知のチップ名に 4 MHz を渡すと `FM_ERR_UNKNOWN_CHIP`）
- `SetClockRate(clock)` の呼び出しは残した。SAASound の既定値（ビルド設定の
  `EXTERNAL_CLK_HZ`）に頼らないため

### 確認

**確認済み**（MSVC 19.51 で、変更前 `bb0c0cd` と変更後の DLL を §4.4 のハーネスで叩いた）:

- 「8 MHz 以外の 6 通り（4,000,000、7,159,090、7,999,999、8,000,001、16,000,000、
  4,294,967,295）がすべて `FM_ERR_INVALID_ARG`」「断ったとき `out_id` に書かない」「断ったとき
  チップが追加されない」「8 MHz のチップがある状態でも 4 MHz は断る」が、変更前の DLL で落ち、
  変更後の DLL で通った
- 変更後: clock=0 と、未知のチップ名＋clock=0 は `FM_ERR_INVALID_ARG`。未知のチップ名＋4 MHz と
  未知のチップ名＋8 MHz は `FM_ERR_UNKNOWN_CHIP`。8 MHz は 2 つ続けて追加できる
- 8 MHz で鳴らした出力が、変更前と変更後でバイト一致（§2.5 のものとも一致）
- FMEngineTest `0c22d67` で `saa.json` を WAV に書き出すと、§4.4 の WAV とバイト一致。
  `clock` を 4,000,000 にした `saa.json` は `[SKIP] SAA : AddChip failed (code=-1)` になる
  （変更前の DLL は受け付けて native_rate=7812 で鳴らしていた）

**未検証**: Linux / macOS でのビルド、`NMakefile` でのビルド。
