#pragma once

#define VALIDATION 1

#include "framework_vulkan\framework_vulkan.h"

/*

  NOTE: The goal for this demo is to try various volumetric fog techniques. 

  References:

    - https://www.alexandre-pestana.com/volumetric-lights/
    
 */

struct gpu_raymarch_inputs
{
    f32 Time;
    f32 RenderWidth;
    f32 RenderHeight;
};

struct demo_state
{
    platform_block_arena PlatformBlockArena;
    linear_arena Arena;
    linear_arena TempArena;

    // NOTE: Render targets
    render_target_entry SwapChainEntry;
    render_target RenderTarget;
    
    // NOTE: Samplers
    VkSampler PointSampler;
    VkSampler LinearSampler;
    VkSampler AnisoSampler;

    ui_state UiState;

    f32 CurrFrameTime;
    VkBuffer RayMarchInputs;
    VkDescriptorSetLayout RayMarchDescLayout;
    VkDescriptorSet RayMarchDescriptor;
    vk_pipeline* RayMarchPipeline;
};

global demo_state* DemoState;
