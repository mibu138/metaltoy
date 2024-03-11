#include <metal_stdlib>
using namespace metal;

half mandelbrot(float2 st)
{
    float x0 = 2.0 * st.x - 1.5;
    float y0 = 2.0 * st.y - 1.0;

    // Implement Mandelbrot set
    float x = 0.0;
    float y = 0.0;
    uint iteration = 0;
    uint max_iteration = 512;
    float xtmp = 0.0;
    while(x * x + y * y <= 4 && iteration < max_iteration)
    {
        xtmp = x * x - y * y + x0;
        y = 2 * x * y + y0;
        x = xtmp;
        iteration += 1;
    }

    // Convert iteration result to colors
    half color = (0.5 + 0.5 * sin(3.0 + iteration * 0.15));
    return color;
}

float thevoid(float2 st)
{
    return 0.0;
}

float justice(float2 st)
{
    if (st.x < 0.5)
        return half(0.0);
    return half(1.0);
}

// return radial coordinates, r and angle (with angle remapped to 0-1)
float2 radial(float2 st)
{
    float r = sqrt(dot(st, st));
    //float angle = atan2(st.y, st.x) / (2 * M_PI_F) + 0.5;
    float angle = atan2(st.y, st.x);
    return float2(r, angle);
}

float fill(float x, float e)
{
    return 1.0 - step(e, x);
}

float stroke(float sdf, float e0, float e1)
{
    return step(sdf, e1) - step(sdf, e0);
}

float2 center(float2 st)
{
    return 2 * st - float2(1.0, 1.0);
}

float heartsdf(float2 st)
{
    float r, h, a, b, c, d;

    st.y = 1.0 - st.y;
    st -= float2(0.5, 0.8);
    r = length(st) * 5.0;
    st = normalize(st);

    return r - 
        ((st.y * pow(abs(st.x),0.67)) / (st.y+1.5) - (2.)*st.y + 1.25);
}

float heartsdf2(float2 st, float t, float scale)
{
    float r, h, a, f, w, b;

    st.y = 1.0 -0.3 * (1.0 - scale) - st.y;
    st -= float2(0.5, 0.8);
    st *= scale;
    r = length(st) * 5.0;
    st = normalize(st);

    w = 3.4;

    //f = cos(t*2);
    //f = f * f * f * f;
    //a = cos((t*4 + f) * 4) * 0.1;
    a = sin(t * w) + 0.7 * sin(4 * t * w);
    b = pow(0.5 * sin(t * w) + 0.5, 2);

    a *= 0.2;
    a *= b;
    
    return r - 
        ((st.y * pow(abs(st.x),0.60 + a*0.1)) / (st.y+1.5) - (2.)*st.y + 1.25 + a);
}

float3 lovers(float2 st, float t)
{
    float h, a1, a2, h2;
    float3 c, c1, c2, c3;

    c1 = float3(0.9, 0.1, 0.07);
    c2 = float3(1.0, 0.04, 0.05);
    c3 = float3(0.0, 0.0, 0.0);

    c = float3(0.0, 0.0, 0.0);

    h = heartsdf2(st, t, 1.1);
    h2 = heartsdf2(st, t, 1.0);

    a1 = fill(h, 0.5);
    a2 = fill(h2, 0.5);

    c = c1 * a1 + c2 * a2 * (1.0 - a1);

    return c;
}

float fun1(float2 st)
{
    float a, r;

    st = center(st);
    st = radial(st);

    a = cos(st.y * 4);
    r = st.r;

    a *= a;

    a = a * r * r * r * r;
    return stroke(a, 0.1, 0.2);
}

kernel void computeMain(texture2d< half, access::write > tex [[texture(0)]],
                           uint2 index [[thread_position_in_grid]],
                           uint2 gridSize [[threads_per_grid]],
                           device const float *time [[buffer(0)]])
{
    float2 st;
    half3 color;

    st.x = float(index.x) / gridSize.x;
    st.y = float(index.y) / gridSize.y;

    color = half3(lovers(st, *time));
    tex.write(half4(color, 1.0), index, 0);
}
