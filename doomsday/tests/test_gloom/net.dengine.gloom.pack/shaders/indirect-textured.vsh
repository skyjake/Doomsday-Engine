uniform highp mat4 uMvpMatrix;

DENG_ATTRIB highp vec4 aVertex;
DENG_ATTRIB highp vec2 aUV;
DENG_ATTRIB highp vec2 aUV2;
DENG_ATTRIB highp vec4 aBounds;
DENG_ATTRIB highp vec4 aColor;

DENG_VAR highp vec2 vUV;
DENG_VAR highp vec2 vTexSize;
DENG_VAR highp vec4 vColor;
DENG_VAR highp vec4 vBounds;

void main(void) 
{
    gl_Position = uMvpMatrix * aVertex;
    vUV = aUV;
    vTexSize = aUV2;
    vBounds = aBounds;
    vColor = aColor;
}

