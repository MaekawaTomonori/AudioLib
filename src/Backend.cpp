// miniaudio の実装は再生(Mixer)とデコード(Loader)の両方が使うため、どちらにも置かずここでだけ組み込む

// miniaudio は実装より前に stb_vorbis の宣言があるときだけ OGG Vorbis のデコードを有効にする
#define STB_VORBIS_HEADER_ONLY
#include "vendor/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "vendor/stb_vorbis.c"
