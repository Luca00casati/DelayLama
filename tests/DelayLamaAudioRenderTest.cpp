#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "damsdk/api/DamPlugin.h"
#include "RenderScenario.h"

// The plugin entry point, as a host would call it.
extern "C" DamSDK::Api::DamPlugin* __cdecl VSTPluginMain(DamSDK::Api::dispatchFunc hostCallback);

// Allows last-bit float rounding differences between compilers; a wrong table
// lookup or envelope step is several orders of magnitude larger.
static const float kSampleTolerance = 1e-6f;

// tests/fixtures/ReferenceAudio.f32 is the output of the original Delay Lama for the
// scenario in tests/helpers/RenderScenario.h (rendered with tools/render_reference.cpp).
static std::vector<float> loadReference() {
    std::vector<float> samples;
    FILE* f = std::fopen(TEST_FIXTURES_DIR "/ReferenceAudio.f32", "rb");
    if (!f)
        return samples;
    float buffer[4096];
    size_t n;
    while ((n = std::fread(buffer, sizeof(float), 4096, f)) > 0)
        samples.insert(samples.end(), buffer, buffer + n);
    std::fclose(f);
    return samples;
}

static std::vector<float> renderThisBuild() {
    DamSDK::Api::DamPlugin* plugin = VSTPluginMain((DamSDK::Api::dispatchFunc)&RenderScenario::hostCallback);
    if (!plugin)
        return std::vector<float>();
    std::vector<float> samples = RenderScenario::render((RenderScenario::Plugin*)plugin);

    // Set DELAYLAMA_RENDER_OUT to keep this build's output for listening or analysis.
    if (const char* path = std::getenv("DELAYLAMA_RENDER_OUT")) {
        if (FILE* f = std::fopen(path, "wb")) {
            std::fwrite(&samples[0], sizeof(float), samples.size(), f);
            std::fclose(f);
        }
    }
    return samples;
}

TEST(DelayLamaAudioRenderTest, OutputMatchesOriginal) {
    std::vector<float> expected = loadReference();
    ASSERT_FALSE(expected.empty()) << "missing fixture ReferenceAudio.f32";

    std::vector<float> actual = renderThisBuild();
    ASSERT_EQ(actual.size(), expected.size());

    const int channels = 2;
    const int frames = (int)(expected.size() / channels);
    int mismatches = 0;
    for (int i = 0; i < frames * channels; i++) {
        ASSERT_FALSE(std::isnan(actual[i])) << "NaN at frame " << i / channels;
        if (std::fabs(actual[i] - expected[i]) > kSampleTolerance) {
            if (++mismatches <= 10) {
                ADD_FAILURE() << "frame " << i / channels << " (block " << i / channels / RenderScenario::kBlockSize
                              << ") channel " << i % channels << ": expected " << expected[i] << " but got " << actual[i];
            }
        }
    }
    EXPECT_EQ(mismatches, 0) << "samples outside tolerance";
}
