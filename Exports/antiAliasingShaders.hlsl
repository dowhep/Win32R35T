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

float3 getColor(float2 m_pointpos, float2 m_point, float2 st, float zoom) {
    float3 colorPrBtLf = float3(0.200f, 0.300f, 0.700f);
    float3 colorPrTpRt = float3(0.900f, 0.600f, 0.300f);
    float3 colorRand = float3(0.300f, 0.200f, 0.200f);
    float3 colorGrad = float3(0.086f, 0.980f, 0.122f);

    float mixval = (m_pointpos.x + m_pointpos.y) * .5f / zoom + 0.5f;
    mixval = clamp(mixval, 0.0f, 1.0f);

    float3 color = float3(.0f, .0f, .0f);

    color += colorPrBtLf * (1.0f - mixval) + colorPrTpRt * mixval;
    color += dot(m_point, float2(.3f, .6f)) * colorRand * 0.596f;
    color += colorGrad * (m_pointpos.x - m_pointpos.y - st.x + st.y) * 0.010f;
    return color;
}

float3 checkGrey(float3 color, float2 m_pointpos, float greyarea, float greyRange) {
    if (m_pointpos.y > greyarea) {
        float minc = (color.x + color.y + color.z) * 0.1f;
        float3 colorGreyed = float3(minc, minc, minc) + color * 0.45f;
        float mixGrey = clamp(m_pointpos.y - greyarea, 0.0f, greyRange) / greyRange;
        color = color + (colorGreyed - color) * mixGrey;
    }

    return color;
}


float4 ps_main(vs_out input) : SV_TARGET{
    float greyarea = 0.7f;
    float greyRange = 0.1f;
    float frequency = 0.0009f;

    float AARadius = 0.002f;

    float3 color = float3(.0f, .0f, .0f);
    float3 colorMouse = float3(0.050f, 0.050f, 0.050f);
    float zoom = 6.9f;
    float2 st = input.texcoord;
    st -= 0.5f;
    st *= zoom;


    greyarea -= 0.5f;
    greyarea *= zoom;

    greyRange *= zoom;
    AARadius *= zoom;

    float2 i_st = floor(st);
    float2 f_st = frac(st);

    float2 mouseDiff = (mousepos - 0.5f) * zoom - st;
    float mouseDist = length(mouseDiff);
    float2 m_diff = mouseDiff;
    float m_dist = mouseDist;
    float2 m_point = (mousepos - 0.5f) * 2.0f;
    float2 m_pointpos = m_point * 0.5f * zoom;

    float2 m_diff2 = mouseDiff;
    float2 m_point2 = m_point;
    float2 m_pointpos2 = m_pointpos;
    bool onEdge = false;

    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            float2 neighbor = float2(float(i), float(j));
            float2 tpointpos = i_st + neighbor;
            float2 tpoint = random2(tpointpos);
            tpoint = 0.5f + 0.5f * sin(fTick * frequency + 6.2831f * tpoint);
            float2 diff = neighbor + tpoint - f_st;
            
            float2 step = diff - m_diff;
            step = step / length(step);
            step *= AARadius;

            float dist = length(diff);

            if (dist < m_dist) {
                if (length(m_diff + step) < length(diff + step)) {
                    onEdge = true;
                    m_diff2 = m_diff;
                    m_point2 = m_point;
                    m_pointpos2 = m_pointpos;
                } else {
                    onEdge = false;
                }

                m_dist = dist;
                m_diff = diff;
                m_point = tpoint;
                m_pointpos = tpointpos;
            } else {
                if (length(diff - step) < length(m_diff - step)) {
                    onEdge = true;
                    m_diff2 = diff;
                    m_point2 = tpoint;
                    m_pointpos2 = tpointpos;
                }
            } 
            
        }
    }

    if (mouseDist <= m_dist) {
        color += 0.080f * sin(fTick * frequency) + colorMouse;
    }
    
    color += getColor(m_pointpos, m_point, st, zoom);
    color = checkGrey(color, m_pointpos, greyarea, greyRange);
    
    // anti-aliasing
    if (onEdge) {
        // return float4(0.0f, 0.0f, 0.0f, 1.0f);
        float2 edge = m_diff2 - m_diff;
        float edgeLength = length(edge);
        float projLength = -dot(m_diff, edge) / edgeLength;
        float distToEdge = edgeLength * 0.5f - projLength;

        float mixcolor = (distToEdge / AARadius - 1.0f) * -0.5f;

        float3 color2 = getColor(m_pointpos2, m_point2, st, zoom);
        color2 = checkGrey(color2, m_pointpos2, greyarea, greyRange);

        if (mouseDist <= length(m_diff2)) {
            color2 += 0.080f * sin(fTick * frequency) + colorMouse;
        }
    

        color = color * (1.0f - mixcolor) + color2 * mixcolor;
    }




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