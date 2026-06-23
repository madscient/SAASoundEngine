// SAASoundEngine.cpp
// SAASound (SAA1099) の FmEngineApi ラッパー実装
//
// FmEngineApi 仕様:
//   - チップ名 "SAA" のみサポート
//   - port 引数は無視 (SAA1099 は port 概念なし)
//   - SetMemory / GetMemorySize は FM_ERR_UNAVAILABLE を返す
//   - GenerateMany の 16bit signed LE stereo interleaved → float32 deinterleaved 変換
//   - スレッドセーフ: Write / SetGain は atomic でなくても FmEngineTest の
//     使用パターン (ストリーム開始後にメインスレッドから 1 チップ分だけ書く) では問題なし。
//     より厳密にしたい場合は std::mutex を追加のこと。
//
// ビルド方法 (例):
//   Windows (MSVC cl /std:c++17 /EHsc):
//     cl /std:c++17 /EHsc /O2 /LD /DFMENGINE_EXPORTS /DHAVE_CONFIG_H=1
//        /I../SAASound/include /I../SAASound/src /I../SAASound/src/minIni /Isrc
//        SAASoundEngine.cpp
//        ../SAASound/src/SAASound.cpp ../SAASound/src/SAAImpl.cpp
//        ../SAASound/src/SAADevice.cpp ../SAASound/src/SAAAmp.cpp
//        ../SAASound/src/SAAFreq.cpp ../SAASound/src/SAANoise.cpp
//        ../SAASound/src/SAAEnv.cpp   ../SAASound/src/SAASndC.cpp
//        ../SAASound/src/SAAConfig.cpp ../SAASound/src/minIni/minIni.c
//        /Fe:SAASoundEngine.dll /link /DLL
//
//   Linux (g++ -std=c++17):
//     g++ -std=c++17 -O2 -shared -fPIC -DFMENGINE_EXPORTS -DHAVE_CONFIG_H=1
//         -I../SAASound/include -I../SAASound/src -I../SAASound/src/minIni -Isrc
//         SAASoundEngine.cpp
//         ../SAASound/src/SAASound.cpp ../SAASound/src/SAAImpl.cpp
//         ../SAASound/src/SAADevice.cpp ../SAASound/src/SAAAmp.cpp
//         ../SAASound/src/SAAFreq.cpp ../SAASound/src/SAANoise.cpp
//         ../SAASound/src/SAAEnv.cpp   ../SAASound/src/SAASndC.cpp
//         ../SAASound/src/SAAConfig.cpp ../SAASound/src/minIni/minIni.c
//         -o libSAASoundEngine.so

#include "FmEngineApi.h"       // FmEngineApi 仕様ヘッダ
#include "SAASound.h"          // SAASound C++ API

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <algorithm>

// =========================================================
//  定数
// =========================================================
static const char* const kChipName  = "SAA";
static const uint32_t    kSaaClock  = 8000000u;  // SAA1099 標準クロック 8 MHz

// =========================================================
//  チップスロット
// =========================================================
struct ChipSlot {
    std::string          name;         // "SAA"
    uint32_t             clock;        // マスタークロック (Hz)
    LPCSAASOUND          saa;          // SAASound オブジェクト
    float                gain_l = 1.0f;
    float                gain_r = 1.0f;

    ChipSlot() : clock(kSaaClock), saa(nullptr) {}
    ~ChipSlot() {
        if (saa) {
            DestroyCSAASound(saa);
            saa = nullptr;
        }
    }
    // non-copyable, movable
    ChipSlot(const ChipSlot&) = delete;
    ChipSlot& operator=(const ChipSlot&) = delete;
    ChipSlot(ChipSlot&& o) noexcept
        : name(std::move(o.name)), clock(o.clock), saa(o.saa),
          gain_l(o.gain_l), gain_r(o.gain_r)
    {
        o.saa = nullptr;
    }
};

// =========================================================
//  エンジン本体 (FmEngineOpaque の実体)
// =========================================================
struct FmEngineOpaque {
    uint32_t             sample_rate;
    std::vector<ChipSlot> chips;       // AddChip で追加されたスロット

    // GenerateMany が出力する 16bit LE stereo interleaved バッファ
    std::vector<uint8_t> pcm_buf;

    explicit FmEngineOpaque(uint32_t rate) : sample_rate(rate) {}
};

// =========================================================
//  内部ヘルパー
// =========================================================
static inline FmEngineOpaque* as_eng(FmEngineHandle h) {
    return reinterpret_cast<FmEngineOpaque*>(h);
}

// =========================================================
//  エンジン生成・破棄
// =========================================================
extern "C" {

FMENGINE_API FmEngineHandle FMENGINE_CALL FmEngine_Create(uint32_t sample_rate)
{
    if (sample_rate == 0) sample_rate = 44100;
    auto* eng = new (std::nothrow) FmEngineOpaque(sample_rate);
    return reinterpret_cast<FmEngineHandle>(eng);
}

FMENGINE_API void FMENGINE_CALL FmEngine_Destroy(FmEngineHandle engine)
{
    delete as_eng(engine);
}

// =========================================================
//  対応チップ問い合わせ
// =========================================================
FMENGINE_API uint32_t FMENGINE_CALL FmEngine_Inquiry(FmEngineHandle /*engine*/)
{
    return 1u;  // "SAA" のみ
}

FMENGINE_API const char* FMENGINE_CALL FmEngine_GetSupportedChip(
    FmEngineHandle /*engine*/, uint32_t index)
{
    if (index == 0) return kChipName;
    return nullptr;
}

// =========================================================
//  チップ追加
// =========================================================
FMENGINE_API FmResult FMENGINE_CALL FmEngine_AddChip(
    FmEngineHandle engine, const char* name, uint32_t clock, uint32_t* out_id)
{
    auto* eng = as_eng(engine);
    if (!eng || !name) return FM_ERR_INVALID_ARG;

    if (std::string(name) != kChipName)
        return FM_ERR_UNKNOWN_CHIP;

    // SAASound オブジェクト作成
    LPCSAASOUND saa = CreateCSAASound();
    if (!saa) return FM_ERR_ALLOC;

    // クロック設定 (0 → 標準クロック)
    uint32_t effective_clock = (clock == 0) ? kSaaClock : clock;
    saa->SetClockRate(effective_clock);
    saa->SetSampleRate(eng->sample_rate);

    // フィルタ/オーバーサンプル設定
    // SetSoundParameters で NOFILTER + 44100 + 16BIT + STEREO を指定
    // ただしサンプルレートは SetSampleRate で直接指定するので
    // ここではフィルタモードのみ設定する
    saa->SetHighpass(true);
    saa->SetOversample(6);   // デフォルト最高品質

    // チップ登録
    ChipSlot slot;
    slot.name  = kChipName;
    slot.clock = effective_clock;
    slot.saa   = saa;

    uint32_t id = static_cast<uint32_t>(eng->chips.size());
    eng->chips.push_back(std::move(slot));

    if (out_id) *out_id = id;
    return FM_OK;
}

// =========================================================
//  チップ情報取得
// =========================================================
FMENGINE_API const char* FMENGINE_CALL FmEngine_GetChipName(
    FmEngineHandle engine, uint32_t chip_id)
{
    auto* eng = as_eng(engine);
    if (!eng || chip_id >= eng->chips.size()) return nullptr;
    return eng->chips[chip_id].name.c_str();
}

FMENGINE_API uint32_t FMENGINE_CALL FmEngine_GetNativeRate(
    FmEngineHandle engine, uint32_t chip_id)
{
    auto* eng = as_eng(engine);
    if (!eng || chip_id >= eng->chips.size()) return 0;
    // SAA1099 のネイティブレートはクロック / 512
    // (8MHz → 15625 Hz. 実際の出力は oversample 後のサンプルレートなので
    //  チップの発音周波数基準として clock/512 を返す)
    return eng->chips[chip_id].clock / 512u;
}

FMENGINE_API uint32_t FMENGINE_CALL FmEngine_GetSampleRate(FmEngineHandle engine)
{
    auto* eng = as_eng(engine);
    if (!eng) return 0;
    return eng->sample_rate;
}

// =========================================================
//  レジスタ書き込み
//  SAA1099 は port 概念なし → port 引数は無視
// =========================================================
FMENGINE_API FmResult FMENGINE_CALL FmEngine_Write(
    FmEngineHandle engine, uint32_t chip_id,
    uint8_t reg, uint8_t value, uint32_t /*port*/)
{
    auto* eng = as_eng(engine);
    if (!eng || chip_id >= eng->chips.size()) return FM_ERR_INVALID_ARG;
    auto& slot = eng->chips[chip_id];
    if (!slot.saa) return FM_ERR_INVALID_ARG;

    slot.saa->WriteAddressData(reg, value);
    return FM_OK;
}

// =========================================================
//  ゲイン設定
// =========================================================
FMENGINE_API FmResult FMENGINE_CALL FmEngine_SetGain(
    FmEngineHandle engine, uint32_t chip_id, float gain_l, float gain_r)
{
    auto* eng = as_eng(engine);
    if (!eng || chip_id >= eng->chips.size()) return FM_ERR_INVALID_ARG;
    eng->chips[chip_id].gain_l = gain_l;
    eng->chips[chip_id].gain_r = gain_r;
    return FM_OK;
}

FMENGINE_API FmResult FMENGINE_CALL FmEngine_GetGain(
    FmEngineHandle engine, uint32_t chip_id,
    float* out_gain_l, float* out_gain_r)
{
    auto* eng = as_eng(engine);
    if (!eng || chip_id >= eng->chips.size()) return FM_ERR_INVALID_ARG;
    if (out_gain_l) *out_gain_l = eng->chips[chip_id].gain_l;
    if (out_gain_r) *out_gain_r = eng->chips[chip_id].gain_r;
    return FM_OK;
}

// =========================================================
//  外部メモリ (SAA1099 は外部メモリなし)
// =========================================================
FMENGINE_API FmResult FMENGINE_CALL FmEngine_SetMemory(
    FmEngineHandle /*engine*/, uint32_t /*chip_id*/,
    FmMemoryType /*mem_type*/, const uint8_t* /*data*/, uint32_t /*size*/)
{
    return FM_ERR_UNAVAILABLE;
}

FMENGINE_API uint32_t FMENGINE_CALL FmEngine_GetMemorySize(
    FmEngineHandle /*engine*/, uint32_t /*chip_id*/, FmMemoryType /*mem_type*/)
{
    return 0u;
}

// =========================================================
//  波形生成
//  SAASound::GenerateMany → 16bit signed LE stereo interleaved
//  → float32 deinterleaved [-1.0, 1.0] (L/R 個別)
// =========================================================
FMENGINE_API FmResult FMENGINE_CALL FmEngine_Generate(
    FmEngineHandle engine, float* out_l, float* out_r, uint32_t samples)
{
    auto* eng = as_eng(engine);
    if (!eng || !out_l || !out_r || samples == 0) return FM_ERR_INVALID_ARG;

    // 出力バッファをゼロクリア
    std::fill(out_l, out_l + samples, 0.0f);
    std::fill(out_r, out_r + samples, 0.0f);

    // 1サンプル = 4バイト (L:int16 + R:int16)
    const size_t buf_bytes = static_cast<size_t>(samples) * 4u;

    // 一時バッファ確保 (チップごとに再利用)
    if (eng->pcm_buf.size() < buf_bytes)
        eng->pcm_buf.resize(buf_bytes);

    static constexpr float kScale = 1.0f / 32768.0f;

    for (auto& slot : eng->chips) {
        if (!slot.saa) continue;

        // 16bit stereo interleaved PCM を生成
        std::memset(eng->pcm_buf.data(), 0, buf_bytes);
        slot.saa->GenerateMany(eng->pcm_buf.data(), samples);

        const float gl = slot.gain_l;
        const float gr = slot.gain_r;
        const uint8_t* p = eng->pcm_buf.data();

        for (uint32_t i = 0; i < samples; ++i) {
            // little endian signed 16bit
            int16_t sl, sr;
            std::memcpy(&sl, p,     2);
            std::memcpy(&sr, p + 2, 2);
            p += 4;

            // float 変換 + ゲイン + ミックス (複数チップ対応)
            out_l[i] += static_cast<float>(sl) * kScale * gl;
            out_r[i] += static_cast<float>(sr) * kScale * gr;
        }
    }

    // クリッピング
    for (uint32_t i = 0; i < samples; ++i) {
        out_l[i] = std::max(-1.0f, std::min(1.0f, out_l[i]));
        out_r[i] = std::max(-1.0f, std::min(1.0f, out_r[i]));
    }

    return FM_OK;
}

} // extern "C"
