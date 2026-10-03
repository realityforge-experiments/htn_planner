// Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

#include "Core/HTNFileHelpers.h"
#include "Translator/HTNCompilerDomainLoader.h"
#include "Domain/Diagnostics/HTNDiagnosticSink.h"
#include "gtest/gtest.h"
#include "optick.h"
#include <cstdint>
#include <filesystem>
#include <fstream>

#if USE_OPTICK
TEST(HTNProfilingTest, SavesOptickCaptureWithRecordedEvents)
{
    const auto Capture = HTNFileHelpers::MakeAbsolutePath("build/logs/generated-frontend.opt");
    std::error_code Error;
    std::filesystem::create_directories(Capture.parent_path(), Error);
    ASSERT_FALSE(Error) << Error.message();
    std::filesystem::remove(Capture, Error);
    ASSERT_FALSE(Error) << Error.message();
    ASSERT_TRUE(Optick::StartCapture());
    bool Loaded = false;
    {
        OPTICK_EVENT("Generated frontend capture regression");
        HTNCompilerDomainLoadResult Domain;
        HTNDiagnosticSink Diagnostics;
        Loaded = HTNCompilerDomainLoader().LoadFromSource("capture.domain",
            "(:domain Capture top_level_domain (:method (run) top_level_method (branch () ((!result)))))",
            {}, Domain, Diagnostics);
    }
    const bool Stopped = Optick::StopCapture();
    const bool Saved = Optick::SaveCapture(Capture.string().c_str());
    EXPECT_TRUE(Loaded);
    EXPECT_TRUE(Stopped);
    ASSERT_TRUE(Saved);
    // Optick's SaveCapture does not propagate filesystem failures: verify the file.
    std::ifstream Input(Capture, std::ios::binary);
    ASSERT_TRUE(Input.good());
    std::uint32_t Magic = 0;
    Input.read(reinterpret_cast<char*>(&Magic), sizeof(Magic));
    EXPECT_EQ(Magic, 0xB50FB50Fu);
    EXPECT_GT(std::filesystem::file_size(Capture, Error), 64u);
    EXPECT_FALSE(Error);
}
#endif
