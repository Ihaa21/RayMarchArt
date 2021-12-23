
#include "raymarch_art_demo.h"

//
// NOTE: Demo Code
//

inline void DemoAllocGlobals(linear_arena* Arena)
{
    // IMPORTANT: These are always the top of the program memory
    DemoState = PushStruct(Arena, demo_state);
    RenderState = PushStruct(Arena, render_state);
}

DEMO_INIT(Init)
{
    // NOTE: Init Memory
    {
        linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
        DemoAllocGlobals(&Arena);
        *DemoState = {};
        *RenderState = {};
        DemoState->Arena = Arena;
        DemoState->TempArena = LinearSubArena(&DemoState->Arena, MegaBytes(10));
        DemoState->PlatformBlockArena = PlatformBlockArenaCreate(KiloBytes(64), 16);
    }

    // NOTE: Init Vulkan
    {
        {
            const char* DeviceExtensions[] =
            {
                "VK_EXT_shader_viewport_index_layer",
            };
            
            render_init_params InitParams = {};
            InitParams.ValidationEnabled = true;
            InitParams.WindowWidth = WindowWidth;
            InitParams.WindowHeight = WindowHeight;
            InitParams.GpuLocalSize = GigaBytes(1);
            InitParams.DeviceExtensionCount = ArrayCount(DeviceExtensions);
            InitParams.DeviceExtensions = DeviceExtensions;
            VkInit(VulkanLib, hInstance, WindowHandle, &DemoState->Arena, &DemoState->TempArena, &DemoState->PlatformBlockArena, InitParams);
        }
    }
    
    // NOTE: Create samplers
    DemoState->PointSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->LinearSampler = VkSamplerCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, 0.0f);
    DemoState->AnisoSampler = VkSamplerMipMapCreate(RenderState->Device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f,
                                                    VK_SAMPLER_MIPMAP_MODE_LINEAR, 0, 0, 5);    
        
    // NOTE: Init render target entries
    DemoState->SwapChainEntry = RenderTargetSwapChainEntryCreate(RenderState->WindowWidth, RenderState->WindowHeight,
                                                                 RenderState->SwapChainFormat);
    
    // NOTE: Create render data
    {
        u32 RenderWidth = RenderState->WindowWidth;
        u32 RenderHeight = RenderState->WindowHeight;
    
        // NOTE: Render Target
        {
            render_target_builder Builder = RenderTargetBuilderBegin(&DemoState->Arena, &DemoState->TempArena, RenderWidth, RenderHeight);
            RenderTargetAddTarget(&Builder, &DemoState->SwapChainEntry, VkClearColorCreate(0, 0, 0, 1));
                            
            vk_render_pass_builder RpBuilder = VkRenderPassBuilderBegin(&DemoState->TempArena);

            u32 ColorId = VkRenderPassAttachmentAdd(&RpBuilder, DemoState->SwapChainEntry.Format, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                    VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderPassSubPassBegin(&RpBuilder, VK_PIPELINE_BIND_POINT_GRAPHICS);
            VkRenderPassColorRefAdd(&RpBuilder, ColorId, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            VkRenderPassSubPassEnd(&RpBuilder);

            DemoState->RenderTarget = RenderTargetBuilderEnd(&Builder, VkRenderPassBuilderEnd(&RpBuilder, RenderState->Device));
        }

        // NOTE: Create descriptor set layout
        {
            vk_descriptor_layout_builder Builder = VkDescriptorLayoutBegin(&DemoState->RayMarchDescLayout);
            VkDescriptorLayoutAdd(&Builder, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
            VkDescriptorLayoutEnd(RenderState->Device, &Builder);
        }

        // NOTE: Create resources
        DemoState->RayMarchInputs = VkBufferCreate(RenderState->Device, &RenderState->HostArena,
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   sizeof(gpu_raymarch_inputs));

        // NOTE: Create descriptor set
        {
            DemoState->RayMarchDescriptor = VkDescriptorSetAllocate(RenderState->Device, RenderState->DescriptorPool, DemoState->RayMarchDescLayout);
            VkDescriptorBufferWrite(&RenderState->DescriptorManager, DemoState->RayMarchDescriptor, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    DemoState->RayMarchInputs);
            VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
        }

        // NOTE: Raymarch Terrain
        VkDescriptorSetLayout DescriptorLayouts[] =
            {
                DemoState->RayMarchDescLayout,
            };
            
        DemoState->RayMarchPipeline = FullScreenPipelineCreate("shader_raymarch_terrain_frag.spv", "main",
                                                               DemoState->RenderTarget.RenderPass, 0,
                                                               ArrayCount(DescriptorLayouts), DescriptorLayouts);
    }
        
    VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);

    // NOTE: Upload assets
    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);
    {
        UiStateCreate(RenderState->Device, &DemoState->Arena, &DemoState->TempArena, RenderState->LocalMemoryId,
                      &RenderState->DescriptorManager, &RenderState->PipelineManager, Commands, RenderState->SwapChainFormat,
                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &DemoState->UiState);
        
        VkDescriptorManagerFlush(RenderState->Device, &RenderState->DescriptorManager);
    }
    
    VkCommandsSubmit(Commands, RenderState->Device, RenderState->GraphicsQueue);
}

DEMO_DESTROY(Destroy)
{
}

DEMO_SWAPCHAIN_CHANGE(SwapChainChange)
{
    VkCheckResult(vkDeviceWaitIdle(RenderState->Device));
    VkSwapChainReCreate(&DemoState->TempArena, WindowWidth, WindowHeight, RenderState->PresentMode);

    DemoState->SwapChainEntry.Width = RenderState->WindowWidth;
    DemoState->SwapChainEntry.Height = RenderState->WindowHeight;
}

DEMO_CODE_RELOAD(CodeReload)
{
    linear_arena Arena = LinearArenaCreate(ProgramMemory, ProgramMemorySize);
    // IMPORTANT: We are relying on the memory being the same here since we have the same base ptr with the VirtualAlloc so we just need
    // to patch our global pointers here
    DemoAllocGlobals(&Arena);

    VkGetGlobalFunctionPointers(VulkanLib);
    VkGetInstanceFunctionPointers();
    VkGetDeviceFunctionPointers();
}

DEMO_MAIN_LOOP(MainLoop)
{
    u32 ImageIndex;
    VkCheckResult(vkAcquireNextImageKHR(RenderState->Device, RenderState->SwapChain, UINT64_MAX, RenderState->ImageAvailableSemaphore,
                                        VK_NULL_HANDLE, &ImageIndex));
    DemoState->SwapChainEntry.View = RenderState->SwapChainViews[ImageIndex];

    vk_commands* Commands = &RenderState->Commands;
    VkCommandsBegin(Commands, RenderState->Device);

    // NOTE: Update pipelines
    VkPipelineUpdateShaders(RenderState->Device, &RenderState->CpuArena, &RenderState->PipelineManager);

    RenderTargetUpdateEntries(&DemoState->TempArena, &DemoState->RenderTarget);

    // NOTE: Update Ui State
    {
        ui_state* UiState = &DemoState->UiState;
        
        ui_frame_input UiCurrInput = {};
        UiCurrInput.MouseDown = CurrInput->MouseDown;
        UiCurrInput.MousePixelPos = V2(CurrInput->MousePixelPos);
        UiCurrInput.MouseScroll = CurrInput->MouseScroll;
        CopyStruct(CurrInput->KeysDown, UiCurrInput.KeysDown, UiCurrInput.KeysDown);
        UiStateBegin(UiState, FrameTime, RenderState->WindowWidth, RenderState->WindowHeight, UiCurrInput);

#if 0
        local_global v2 PanelPos = V2(100, 800);
        ui_panel Panel = UiPanelBegin(UiState, &PanelPos, "Fog Panel");
        UiPanelText(&Panel, "Fog Data:");
        
        UiPanelEnd(&Panel);
#endif
        
        UiStateEnd(UiState, &RenderState->DescriptorManager);
    }

    // NOTE: Upload input buffer
    {
        {
            gpu_raymarch_inputs* Data = VkCommandsPushWriteStruct(Commands, DemoState->RayMarchInputs, gpu_raymarch_inputs,
                                                                  BarrierMask(VkAccessFlagBits(0), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
                                                                  BarrierMask(VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT));
            *Data = {};
            Data->Time = DemoState->CurrFrameTime;
            Data->RenderWidth = f32(RenderState->WindowWidth);
            Data->RenderHeight = f32(RenderState->WindowHeight);

            DemoState->CurrFrameTime += FrameTime;
        }
            
        VkCommandsTransferFlush(Commands, RenderState->Device);
    }
    
    RenderTargetPassBegin(&DemoState->RenderTarget, Commands, RenderTargetRenderPass_SetViewPort | RenderTargetRenderPass_SetScissor);
    FullScreenPassRender(Commands, DemoState->RayMarchPipeline, 1, &DemoState->RayMarchDescriptor);
    RenderTargetPassEnd(Commands);
    UiStateRender(&DemoState->UiState, RenderState->Device, Commands, DemoState->SwapChainEntry.View);

    VkCommandsEnd(Commands, RenderState->Device);
                    
    // NOTE: Render to our window surface
    // NOTE: Tell queue where we render to surface to wait
    VkPipelineStageFlags WaitDstMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &RenderState->ImageAvailableSemaphore;
    SubmitInfo.pWaitDstStageMask = &WaitDstMask;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &Commands->Buffer;
    SubmitInfo.signalSemaphoreCount = 1;
    SubmitInfo.pSignalSemaphores = &RenderState->FinishedRenderingSemaphore;
    VkCheckResult(vkQueueSubmit(RenderState->GraphicsQueue, 1, &SubmitInfo, Commands->Fence));
    
    VkPresentInfoKHR PresentInfo = {};
    PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    PresentInfo.waitSemaphoreCount = 1;
    PresentInfo.pWaitSemaphores = &RenderState->FinishedRenderingSemaphore;
    PresentInfo.swapchainCount = 1;
    PresentInfo.pSwapchains = &RenderState->SwapChain;
    PresentInfo.pImageIndices = &ImageIndex;
    VkResult Result = vkQueuePresentKHR(RenderState->PresentQueue, &PresentInfo);

    switch (Result)
    {
        case VK_SUCCESS:
        {
        } break;

        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
        {
            // NOTE: Window size changed
            InvalidCodePath;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}
