# SAASoundEngine

**SAASound (SAA1099)** の **FmEngineApi** ラッパー DLL。  
`FMEngineTest` の `-e` オプションで指定することで SAA1099 エミュレーションをテストできます。

---

## ディレクトリ構成

```
SAASoundEngine/
├── .gitmodules
├── .gitignore
├── CMakeLists.txt              ← クロスプラットフォームビルド (Linux/macOS/Windows)
├── NMakefile                   ← MSVC NMake ビルド (Windows のみ)
├── README.md
├── extern/
│   └── SAASound/               ← git submodule (stripwax/SAASound)
└── src/
    ├── SAASoundEngine.cpp      ← ラッパー実装本体
    ├── FmEngineApi.h           ← FmEngineApi 仕様ヘッダ (各エンジン共通)
    └── saasound_cmake_config.h ← SAASound 用ビルド設定
```

---

## セットアップ

```bash
git clone https://github.com/madscient/SAASoundEngine
cd SAASoundEngine
git submodule update --init --recursive
```

---

## ビルド

### Linux / macOS (CMake)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 成果物: build/bin/libSAASoundEngine.so  (Linux)
#          build/bin/libSAASoundEngine.dylib (macOS)
```

### Windows (CMake + Visual Studio 2022)

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# 成果物: build\bin\Release\SAASoundEngine.dll
```

### Windows (MSVC NMake — 最小構成)

Visual Studio Developer Command Prompt で:

```cmd
nmake /f NMakefile

# 成果物: build\SAASoundEngine.dll
```

---

## FMEngineTest での使い方

```bash
# Linux — SAA チップのみテスト
FMEngineTest -e libSAASoundEngine.so patches/saa.json

# Windows
FMEngineTest.exe -e SAASoundEngine.dll patches\saa.json
```

期待される起動時出力:

```
FMEngineTest
Loading engine: libSAASoundEngine.so
Engine loaded.
  FmEngine_GetMemoryCount is not exported: ROM files will not be loaded.

Sample rate: 48000 Hz

Supported chips (1): SAA

[SAA] chip_id=0
  CH0 CH0 261Hz (oct=4 F=0xFC) pan=L
  ...
```

---

## 設計メモ

| 項目 | 内容 |
|---|---|
| 対応チップ | `"SAA"` のみ (SAA1099) |
| クロック | `FmEngine_AddChip` の `clock` には 8,000,000 を指定する。それ以外 (0 を含む) は `FM_ERR_INVALID_ARG`。SAASound が 8 MHz 以外のクロックを正しく扱えないため |
| `port` 引数 | 無視 (SAA1099 はポート概念なし) |
| 任意シンボル | 部位ゲイン (`FmEngine_GetPartCount` / `FmEngine_GetPartName` / `FmEngine_SetPartGain` / `FmEngine_GetPartGain`) と外部メモリ (`FmEngine_GetMemoryCount` / `FmEngine_GetMemoryName` / `FmEngine_SetMemory` / `FmEngine_SetMemoryEx`) はエクスポートしない。SAA1099 は出力が 1 系統で、外部メモリを持たない。これらを必須として読み込む呼び出し側からはロードできない |
| バッファ変換 | `GenerateMany` の 16bit LE stereo interleaved → float32 deinterleaved |
| `SAASound.cfg` | 存在しない場合はデフォルト値で動作 (ログ無効・highpass=on・boost=1) |

---

## ライセンス

- **SAASoundEngine.cpp**: MIT
- **SAASound** (`extern/SAASound`): 独自ライセンス ([LICENCE](extern/SAASound/LICENCE) 参照)
- **FmEngineApi.h**: [FMEngineTest](https://github.com/madscient/FMEngineTest) の MIT ライセンスに準拠
