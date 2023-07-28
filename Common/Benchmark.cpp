
#include "Benchmark.hpp"

namespace LeoVK
{
void Benchmark::Run(std::function<void()> renderFunc, VkPhysicalDeviceProperties deviceProp) {

    mbActive = true;
    mDeviceProps = deviceProp;

    AttachConsole(ATTACH_PARENT_PROCESS);
    freopen_s(&mStream, "CONOUT$", "w+", stdout);
    freopen_s(&mStream, "CONOUT$", "w+", stderr);

    std::cout << std::fixed << std::setprecision(3);

    {
        double tMeasured = 0.0;
        while (tMeasured < (mWarmup * 1000))
        {
            auto tStart = std::chrono::high_resolution_clock::now();
            renderFunc();
            auto tDiff = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
            tMeasured += tDiff;
        }
    }
    // Benchmark phase
    {
        while (mRuntime < (mDuration * 1000.0)) {
            auto tStart = std::chrono::high_resolution_clock::now();
            renderFunc();
            auto tDiff = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
            mRuntime += tDiff;
            mFrameTimes.push_back(tDiff);
            mFrameCount++;
            if (mOutputFrames != -1 && mOutputFrames == mFrameCount) break;
        };
        std::cout << "Benchmark finished" << "\n";
        std::cout << "Device : " << mDeviceProps.deviceName << " (Driver version: " << mDeviceProps.driverVersion << ")" << "\n";
        std::cout << "Runtime: " << (mRuntime / 1000.0) << "\n";
        std::cout << "Frames : " << mFrameCount << "\n";
        std::cout << "FPS    : " << mFrameCount / (mRuntime / 1000.0) << "\n";
    }
}

void Benchmark::SaveResults() {

    std::ofstream result(mFilename, std::ios::out);
    if (result.is_open())
    {
        result << std::fixed << std::setprecision(4);

        result << "Device, DriverVersion, Duration (ms), Frames, FPS" << "\n";
        result << mDeviceProps.deviceName << "," << mDeviceProps.driverVersion << "," << mRuntime << "," << mFrameCount << "," << mFrameCount / (mRuntime / 1000.0) << "\n";

        if (mbOutputFrameTime)
        {
            result << "\n" << "Frame, ms" << "\n";
            for (size_t i = 0; i < mFrameTimes.size(); i++)
            {
                result << i << "," << mFrameTimes[i] << "\n";
            }

            double tMin = *std::min_element(mFrameTimes.begin(), mFrameTimes.end());
            double tMax = *std::max_element(mFrameTimes.begin(), mFrameTimes.end());
            double tAvg = std::accumulate(mFrameTimes.begin(), mFrameTimes.end(), 0.0) / (double)mFrameTimes.size();

            std::cout << "Best   : " << (1000.0 / tMin) << " FPS (" << tMin << " ms)" << "\n";
            std::cout << "Worst  : " << (1000.0 / tMax) << " FPS (" << tMax << " ms)" << "\n";
            std::cout << "AVG    : " << (1000.0 / tAvg) << " FPS (" << tAvg << " ms)" << "\n";
            std::cout << "\n";
        }

        result.flush();
        FreeConsole();
    }
}

}

