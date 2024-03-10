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

half thevoid(float2 st)
{
    return 0.0;
}

half justice(float2 st)
{
    if (st.x < 0.5)
        return half(0.0);
    return half(1.0);
}

kernel void computeMain(texture2d< half, access::write > tex [[texture(0)]],
                           uint2 index [[thread_position_in_grid]],
                           uint2 gridSize [[threads_per_grid]])
{
    float2 st;

    st.x = float(index.x) / gridSize.x;
    st.y = float(index.y) / gridSize.y;

    half color = justice(st);
    tex.write(half4(color, color, color, 1.0), index, 0);
}
