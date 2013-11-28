uniform sampler2D uTex;
varying highp vec4 vColor;
varying highp vec2 vUV;

void main(void) {
    highp vec4 tex = texture2D(uTex, vUV);
    gl_FragColor = tex * vColor;

    // Discard fragments without alpha.
    if(gl_FragColor.a <= 0.0) { 
        discard; 
    }
}
