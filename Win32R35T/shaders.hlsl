/* vertex attributes go here to input to the vertex shader */
struct vs_in {
    float3 position_local : POS;
    float2 texcoord : TEX;
};

/* outputs from vertex shader go here. can be interpolated to pixel shader */
struct vs_out {
    float4 position_clip : SV_POSITION; // required output of VS
    float2 texcoord : TEX;
};

vs_out vs_main(vs_in input) {
    vs_out output = (vs_out)0; // zero the memory first
    output.position_clip = float4(input.position_local, 1.0);
    output.texcoord = input.texcoord;
    return output;
}

float2 random2(float2 inp) {
    return frac(sin(float2(dot(inp, float2(127.1f, 311.7f)), dot(inp, float2(269.5f, 183.3f)))) * 43758.5453f);
}

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float2 mousepos;
    int2 resolution;
    // 16 byte

    int fTick;
    float yoverx;
    float2 filler;
};

float4 ps_main(vs_out input) : SV_TARGET{
    float greyarea = 0.7f;

    float3 color = float3(.0f, .0f, .0f);
    float zoom = 7.1f;
    float2 st = input.texcoord;
    st -= 0.5f;
    st *= zoom;

    greyarea -= 0.5f;
    greyarea *= zoom;

    float2 i_st = floor(st);
    float2 f_st = frac(st);

    float m_dist = 10.f;
    float2 m_point;
    float2 m_pointpos;

    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            float2 neighbor = float2(float(i), float(j));
            float2 tpointpos = i_st + neighbor;
            float2 tpoint = random2(tpointpos);
            tpoint = 0.5f + 0.5f * sin(fTick / 1000.0f + 6.2831f * tpoint);
            float2 diff = neighbor + tpoint - f_st;
            float dist = length(diff);

            if (dist < m_dist) {
                m_dist = dist;
                m_point = tpoint;
                m_pointpos = tpointpos;
            }
        }
    }
    float2 tpointpos = mousepos - 0.5f;
    tpointpos *= zoom;
    float2 diff = tpointpos - st;
    float dist = length(diff);

    if (dist < m_dist) {
        m_dist = dist;
        m_point = tpointpos / zoom * 2.0f;
        m_pointpos = tpointpos;
        color += 0.15f * sin(fTick / 1000.0f);
    }
    
    float3 color1 = float3(0.058f, 0.198f, 0.815f);
    float3 color2 = float3(1.000f, 0.034f, 0.616f);
    float3 color3 = float3(0.148f, 0.980f, 0.000f);
    float3 color4 = float3(0.086f, 0.980f, 0.122f);

    float mixval = (m_pointpos.x + m_pointpos.y) * .5f / zoom + 0.5f;
    mixval = clamp(mixval, 0.0f, 1.0f);

    color += color1 * (1.0f - mixval) + color2 * mixval;
    color += dot(m_point, float2(.3f, .6f)) * color3 * 0.596f;

    if (m_pointpos.y > greyarea) {
        float minc = (color.x + color.y + color.z) * 0.1f;
        color = float3(minc, minc, minc) + color * 0.45f;
    }

    color += color4 * (m_pointpos.x - m_pointpos.y - st.x + st.y) * 0.010f;

    // mouse
    //float2 rela = mousepos - input.texcoord;
    //rela.y *= yoverx;
    //float boost = 0.1f;
    //float limit = 0.1f;
    ////if (length(rela) < limit) color = float3(1.0f, 1.0f, 1.0f);
    //if (length(rela) < limit) {
    //    float inc = (1.0f - length(rela) / limit) * boost;
    //    color += float3(inc, inc, inc);
    //}

    color = clamp(color, 0.0f, 1.0f);
    

    return float4(color, 1.0f); // must return an RGBA colour
}