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
