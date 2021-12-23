#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define MATH_GLSL 1
#include "../libs/math/math.h"

#include "shader_raymarch_terrain.h"

layout(location = 0) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

/*
  NOTE: A lot of referencing to this for ray marching step size: https://www.shadertoy.com/view/MdsSRs

  TODO:

   - shadows don't work
   - try to get grass on the top of mountain and rock on the side (find colors from a sample somewhere and use the smooth blends)
   - make the environment a night sky
   - add night light to the fog
   - add value noise to fog height
   - add stars in teh sky
   - add water coloring + reflecting the sky

   - https://www.shadertoy.com/view/MdX3Rr (reference)
   
 TODO: Long term

   - https://iquilezles.org/www/articles/fbmsdf/fbmsdf.htm
     - https://www.shadertoy.com/view/Ws3XWl (reference)
   - https://iquilezles.org/www/articles/gradientnoise/gradientnoise.htm
   - https://www.shadertoy.com/view/4ttSWf (reference)
   - https://www.shadertoy.com/view/XttSz2 (reference)
   - https://www.shadertoy.com/view/4tsGzf
   - https://www.shadertoy.com/view/ldSGzR (bump map)

   - setup random numbers + pcg
   - finish the noise algorithms + their variations (white noise), uses above
   
 */

// NOTE: http://www.diva-portal.org/smash/get/diva2:1059684/FULLTEXT01.pdf https://www.shadertoy.com/view/XlGcRh TODO: Implement
f32 WhiteNoise2d(v2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

DefineValueNoiseDeriv2d(ValueNoise2d, WhiteNoise2d)

v4 FbmNoise2d(v2 Input)
{
    f32 Scale = 0.002f;
    f32 Height = 190.0f;
    
    f32 H = 1.0f;
    f32 G = Pow(2.0f, -H);
    f32 F = 1.0f;
    f32 A = 1.0f;
    f32 T = 0.0f;
    v2 Derivative = V2(0);
    for (u32 OctaveId = 0; OctaveId < 10; ++OctaveId)
    {
        v3 NoiseSample = ValueNoise2d(F * Scale * Input);
        T += A*NoiseSample.x; // NOTE: Accumulate our noise height
        Derivative += A * NoiseSample.yz * F; // NOTE: Accumulate our derivatives
        F *= 2.0f; // NOTE: Double our frequency
        A *= G; // NOTE: Lower our amplitude
    }

    T *= Height;
    Derivative *= Height * Scale;

    v4 Result = V4(T, Normalize(V3(-Derivative.x, 1.0f, -Derivative.y)));
    
    return Result;
}

v4 SampleTerrain(v2 InputPos)
{
    v4 Result = V4(0.0f);
    Result = FbmNoise2d(InputPos);

    // NOTE: Water plane
    f32 WaterHeight = -10.5f;
    if (Result.x < WATER_HEIGHT)
    {
        Result = V4(WATER_HEIGHT, V3(0, 1, 0));
    }

    //Result = V4(Sin(InputPos.x), Normalize(V3(-Cos(InputPos.x), 1.0f, 0.0f)));
    
    return Result;
}

v4 CastRay(v3 RayStart, v3 RayDir, u32 NumIterations, f32 MaxT)
{
    v4 Result = V4(-1.0f, 0, 0, 0);

    // NOTE: Used to interpolate our height function lineraly for collision
    f32 PrevHeight = 0.0f;
    f32 PrevPosY = RayStart.y;

    f32 CurrT = 0.0f;
    for (u32 IterationId = 0; IterationId < NumIterations; ++IterationId)
    {
        v3 CurrPos = RayStart + RayDir * CurrT;
        v4 TerrainSample = SampleTerrain(CurrPos.xz);

        f32 TerrainHeight = TerrainSample.x;
        v3 TerrainNormal = TerrainSample.yzw;

        f32 Dt = 0.6f * (CurrPos.y - TerrainHeight) * (0.001f + TerrainSample.z);
        if ((CurrPos.y - TerrainHeight) < 0.05f)
        {
            // NOTE: Interpolate our intersection
            Result.x = CurrT - Dt + Dt*(PrevHeight - PrevPosY) / (CurrPos.y - PrevPosY - TerrainHeight + PrevHeight);
            Result.yzw = TerrainNormal;
            break;
        }

        if (CurrT > MaxT)
        {
            break;
        }
            
        // NOTE: Step based on difference in Y and where the y is pointing
        CurrT += Dt;

        PrevPosY = CurrPos.y;
        PrevHeight = TerrainHeight;
    }
    
    return Result;
}

void main()
{
    v2 Uv = InUv;
    {
        Uv.y = 1.0 - Uv.y; // NOTE: Take into accoutn vulkan coords
        Uv = 2.0 * Uv - 1.0; // NOTE: Convert range to -1, 1
        Uv.x *= SdfInputs.RenderWidth / SdfInputs.RenderHeight;
    }    

#if 0
    v3 RayStart = V3(1000, 0, 0);
    RayStart.x += 10.0f*Sin(SdfInputs.Time);
    f32 TerrainHeight = SampleTerrain(RayStart.xz).x;
    RayStart.y = TerrainHeight + 200.0f;
    v3 RayDir = Normalize(V3(Uv, 1));
#endif

#if 1
    //vec3 ro = vec3( 1000.0*cos(0.001*SdfInputs.Time) + 100, 0.0, 1000.0*sin(0.001*SdfInputs.Time) );
    v3 ro = V3(1000, 0, 0);
    vec3 ta = vec3( 0.0 );
    ro.y = SampleTerrain(ro.xz).x + 100.0;
    ta.y = SampleTerrain(ro.xz).x + 100.0;
    
    // camera matrix    
    vec3  cw = normalize( ta-ro );
    vec3  cu = normalize( cross(cw,vec3(0.0,1.0,0.0)) );
    vec3  cv = normalize( cross(cu,cw) );
    vec3  rd = normalize( Uv.x*cu + Uv.y*cv + 2.0*cw );
    
    v3 RayStart = ro;
    v3 RayDir = rd;
#endif
    
    v4 RayResult = CastRay(RayStart, RayDir, 150, 2000.0f);
    v3 Color = V3(0);
    if (RayResult.x != -1.0f)
    {
        // NOTE: We hit the terrain
        f32 Distance = RayResult.x;
        v3 SurfacePos = RayStart + RayDir * Distance;
        v3 SurfaceNormal = RayResult.yzw;

        f32 SurfaceSpecular = 32.0f;
        v3 SurfaceColor = V3(0.9f, 0.7f, 0.73f);
        if (SurfacePos.y - WATER_HEIGHT < 0.05f)
        {
            SurfaceColor = V3(0.1f, 0.5f, 0.8f);
            SurfaceSpecular = 2.0f;
        }
        
#if 0
        if (Dot(SurfaceNormal, V3(0, 1, 0)) < 0.4f)
        {
            Color = V3(0.2f);
        }
        else
        {
            Color = V3(0, 1, 0);
        }
#endif

        // NOTE: Light color
        v3 LightDir = Normalize(V3(0.5f, -1.0f, 0.0f));
        v3 LightColor = V3(0.7f, 0.7f, 1.0f);
        {
            f32 Occlusion = CastRay(SurfacePos + V3(0, 0.001f, 0), -LightDir, 64, 400.0f).x;
            Occlusion = (Occlusion == -1.0f) ? 1.0f : 0.0f;
            Occlusion = 1.0f;
            
            Color = Occlusion * BlinnPhongLighting(RayDir, SurfaceColor, SurfaceNormal, 32.0f, -LightDir, LightColor);
        }
        
        // NOTE: Add height fog (https://iquilezles.org/www/articles/fog/fog.htm)
        {
            f32 a = 0.005f;
            f32 b = 0.09f;
            f32 FogAmount = (a/b) * Exp(-RayStart.y * b) * (1.0f - Exp(-Distance * RayDir.y * b)) / RayDir.y;
            FogAmount = Clamp(FogAmount, 0, 1);
            Color = Lerp(Color, FOG_COLOR, FogAmount);
        }
    }
    else
    {
        // NOTE: Apply sky
        Color = SKY_COLOR;
    }

    // TODO: REMOVE?
    // NOTE: Visualize FBM noise
    {
#if 0
        f32 FbmValue = 0.25f*FbmNoise(10.0f * Uv.x, 0.0f);
        if (Uv.y < FbmValue)
        {
            Color = V3(0);
        }
        else
        {
            Color = V3(1, 0, 0);
        }
#endif

#if 0
        f32 A = WhiteNoise2d(V2(0, 0));
        f32 B = WhiteNoise2d(V2(1, 0));
        f32 C = WhiteNoise2d(V2(0, 1));
        f32 D = WhiteNoise2d(V2(1, 1));

        f32 Noise = Lerp(InUv.y, Lerp(InUv.x, A, B), Lerp(InUv.x, C, D));
        Color = V3(Noise);
#endif
    }    
    OutColor = V4(Color, 1);
}
