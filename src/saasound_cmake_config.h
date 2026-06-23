#pragma once
// saasound_cmake_config.h
// FmEngine_SAA ビルド用カスタム設定。
// USE_CONFIG_FILE を有効化 → SAAConfig が読まれるが、SAASound.cfg がなければ
// デフォルト値 (ログ無効, highpass=on, boost=1, output_bitmask=0x3f) で動作する。

#define EXTERNAL_CLK_HZ 8000000
/* #undef SAAFREQ_FIXED_CLOCKRATE */
#define SAMPLE_RATE_HZ 44100
#define DEFAULT_OVERSAMPLE 6
#define DEFAULT_UNBOOSTED_MULTIPLIER 11.35
#define DEFAULT_BOOST 1
/* #undef DEBUGSAA */
#define DEBUG_SAA_REGISTER_LOG "debugsaa.txt"
#define DEBUG_SAA_PCM_LOG  "debugsaa.pcm"

#define USE_CONFIG_FILE
#define CONFIG_FILE_PATH "SAASound.cfg"
