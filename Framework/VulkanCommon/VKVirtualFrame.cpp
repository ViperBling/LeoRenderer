#include "VKVirtualFrame.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    void VirtualFrameProvider::Init(size_t frameCount, size_t stageBufferSize)
    {
        auto& vulkan = GetCurrentVulkanContext();
        this->mVirtualFrames.reserve(frameCount);

        vk::CommandBufferAllocateInfo cmdBufferAI {};
        cmdBufferAI.setCommandPool(vulkan.GetCommandPool());
        cmdBufferAI.setLevel(vk::CommandBufferLevel::ePrimary);
        cmdBufferAI.setCommandBufferCount(static_cast<uint32_t>(frameCount));

        auto cmdBuffer = vulkan.GetDevice().allocateCommandBuffers(cmdBufferAI);

        for (size_t i = 0; i < frameCount; i++)
        {
            auto fence = vulkan.GetDevice().createFence(vk::FenceCreateInfo { vk::FenceCreateFlagBits::eSignaled });
            this->mVirtualFrames.push_back(
                VirtualFrame { CommandBuffer(cmdBuffer[i]), StageBuffer(stageBufferSize), fence }
            );
        }
    }

    void VirtualFrameProvider::Destroy()
    {
        auto& vulkan = GetCurrentVulkanContext();

        for (auto& frame : this->mVirtualFrames)
        {
            if ((bool)frame.CommandQueueFence)
            {
                vulkan.GetDevice().destroyFence(frame.CommandQueueFence);
            }
        }
        this->mVirtualFrames.clear();
    }

    void VirtualFrameProvider::StartFrame()
    {
        auto& vulkan = GetCurrentVulkanContext();

        auto acquireNextImage = vulkan.GetDevice().acquireNextImageKHR(
            vulkan.GetSwapchain(),
            UINT64_MAX,
            vulkan.GetImageAvailableSemaphore()
        );
        assert(acquireNextImage.result == vk::Result::eSuccess || acquireNextImage.result == vk::Result::eSuboptimalKHR);
        this->mPresentImageIndex = acquireNextImage.value;

        auto& frame = this->GetCurrentFrame();

        vk::Result waiteFenceRes = vulkan.GetDevice().waitForFences(
            frame.CommandQueueFence,
            VK_FALSE,
            UINT64_MAX
        );
        assert(waiteFenceRes == vk::Result::eSuccess);
        vulkan.GetDevice().resetFences(frame.CommandQueueFence);

        frame.Commands.Begin();

        this->mbIsFrameRunning = true;
    }

    VirtualFrame &VirtualFrameProvider::GetCurrentFrame()
    {
        return this->mVirtualFrames[this->mCurrentFrame];
    }

    VirtualFrame &VirtualFrameProvider::GetNextFrame()
    {
        return this->mVirtualFrames[(this->mCurrentFrame + 1) % this->mVirtualFrames.size()];
    }

    const VirtualFrame &VirtualFrameProvider::GetCurrentFrame() const
    {
        return this->mVirtualFrames[this->mCurrentFrame];
    }

    const VirtualFrame &VirtualFrameProvider::GetNextFrame() const
    {
        return this->mVirtualFrames[(this->mCurrentFrame + 1) % this->mVirtualFrames.size()];
    }

    uint32_t VirtualFrameProvider::GetPresentImageIndex() const
    {
        return this->mPresentImageIndex;
    }

    bool VirtualFrameProvider::IsFrameRunning() const
    {
        return this-mbIsFrameRunning;
    }

    size_t VirtualFrameProvider::GetFrameCount() const
    {
        return this->mVirtualFrames.size();
    }

    void VirtualFrameProvider::EndFrame()
    {
        auto& frame = this->GetCurrentFrame();
        auto& vulkan = GetCurrentVulkanContext();

        auto lastPresentImageUsage = vulkan.GetSwapchainImageUsage(this->mPresentImageIndex);
        auto& presentImage = vulkan.AcquireSwapchainImage(this->mPresentImageIndex, ImageUsage::UNKNOWN);

        auto subresourceRange = GetDefaultImageSubresourceRange(presentImage);

        vk::ImageMemoryBarrier presentToTransferBarrier {};
        presentToTransferBarrier.setSrcAccessMask(ImageUsageToAccessFlags(lastPresentImageUsage));
        presentToTransferBarrier.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
        presentToTransferBarrier.setOldLayout(ImageUsageToImageLayout(lastPresentImageUsage));
        presentToTransferBarrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
        presentToTransferBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        presentToTransferBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        presentToTransferBarrier.setImage(presentImage.GetNativeImage());
        presentToTransferBarrier.setSubresourceRange(subresourceRange);

        frame.Commands.GetNativeCmdBuffer().pipelineBarrier(
            ImageUsageToPipelineStage(lastPresentImageUsage),
            vk::PipelineStageFlagBits::eBottomOfPipe,
            vk::DependencyFlags { },
            { },
            { },
            presentToTransferBarrier
        );

        frame.Commands.End();
        frame.StagingBuffer.Flush();
        frame.StagingBuffer.Reset();

        std::array waitDstStageMask = { (vk::PipelineStageFlags)vk::PipelineStageFlagBits::eTransfer };
        vk::SubmitInfo submitInfo {};
        submitInfo.setWaitSemaphores(vulkan.GetImageAvailableSemaphore());
        submitInfo.setWaitDstStageMask(waitDstStageMask);
        submitInfo.setSignalSemaphores(vulkan.GetRenderingFinishedSemaphore());
        submitInfo.setCommandBuffers(frame.Commands.GetNativeCmdBuffer());

        vulkan.GetGraphicsQueue().submit(submitInfo, frame.CommandQueueFence);

        vk::PresentInfoKHR presentInfo {};
        presentInfo.setWaitSemaphores(vulkan.GetRenderingFinishedSemaphore());
        presentInfo.setSwapchains(vulkan.GetSwapchain());
        presentInfo.setImageIndices(this->mPresentImageIndex);

        auto presentSucceeded = vulkan.GetPresentQueue().presentKHR(presentInfo);
        assert(presentSucceeded == vk::Result::eSuccess);

        this->mCurrentFrame = (this->mCurrentFrame + 1) % this->mVirtualFrames.size();
        this->mbIsFrameRunning = false;

    }
} // namespace LeVK
