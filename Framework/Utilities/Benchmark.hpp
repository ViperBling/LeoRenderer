#pragma once


#include "ProjectPCH.hpp"

namespace LeoVK
{
    class Benchmark
    {
    public:
        void Run(std::function<void()> renderFunc, VkPhysicalDeviceProperties deviceProp);
        void SaveResults();

    public:
        bool mbActive = false;
        bool mbOutputFrameTime = false;
        int mOutputFrames = -1;
        uint32_t mWarmup = 1;
        uint32_t mDuration = 10;
        std::vector<double> mFrameTimes;
        std::string mFilename;
        double mRuntime = 0.0;
        uint32_t mFrameCount = 0;

    private:
        FILE* mStream;
        VkPhysicalDeviceProperties mDeviceProps;
    };
}