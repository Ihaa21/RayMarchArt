
layout(binding = 0) uniform sdf_input_buffer
{
    f32 Time;
    f32 RenderWidth;
    f32 RenderHeight;
} SdfInputs;

#define WATER_HEIGHT -10.5f

#define SKY_COLOR V3(0.4f, 0.4f, 0.8f)
#define FOG_COLOR V3(0.4f, 0.4f, 0.7f)
#define SUN_COLOR V3(1.0f, 0.9f, 0.7f)
