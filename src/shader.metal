#include <metal_stdlib>
using namespace metal;

struct v2f
{
    float4 position [[position]];
    half3 color;
    float2 uv;
};

v2f vertex vertexMain( uint vertexId [[vertex_id]]
                      , device const float3* positions [[buffer(0)]]
                      , device const float3* colors [[buffer(1)]]
                      , device const float2* uvs [[buffer(2)]]
)
{
    v2f o;
    o.position = float4( positions[ vertexId ], 1.0 );
    o.color = half3 ( colors[ vertexId ] );
    o.uv = uvs[ vertexId ];
    return o;
}

half4 fragment fragmentMain( v2f in [[stage_in]] 
                        , texture2d<half, access::sample> tex [[texture(0)]]
)
{
    constexpr sampler s( address::repeat, filter::nearest);

    half4 sample = tex.sample(s, in.uv);

    return sample;
}

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

    half color = mandelbrot(st);
    tex.write(half4(color, color, color, 1.0), index, 0);
}
