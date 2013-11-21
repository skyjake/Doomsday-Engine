#version 120 
 
uniform sampler2D texture; 
uniform float distortionScale = 1.714;
uniform vec2 screenSize = vec2(0.14976, 0.0936);
uniform float lensSeparationDistance = 0.0635;
uniform vec4 chromAbParam = vec4(0.996, -0.004, 1.014, 0.0);
uniform vec4 hmdWarpParam = vec4(1.0, 0.220, 0.240, 0.000);

varying highp vec2 vTexCoord;
 
const float aspectRatio = 1.0;
const vec2 inputCenter = vec2(0.5, 0.5); // We render center at center of unwarped image
const vec2 scaleIn = vec2(2.0, 2.0/aspectRatio); 

void main() { 
   vec2 scale = vec2(0.5/distortionScale, 0.5*aspectRatio/distortionScale);
   vec2 lensCenter = vec2(1.0 - lensSeparationDistance/screenSize.x, 0.5); // left eye
   //
   vec2 tcIn = vTexCoord;
   vec2 uv = vec2(tcIn.x*2, tcIn.y); // unwarped image coordinates (left eye) 
   if (tcIn.x > 0.5) // right eye 
       uv.x = 2 - 2*tcIn.x; 
   vec2 theta = (uv - lensCenter) * scaleIn; 
   float rSq = theta.x * theta.x + theta.y * theta.y; 
   vec2 rvector = theta * ( hmdWarpParam.x + 
                            hmdWarpParam.y * rSq + 
                            hmdWarpParam.z * rSq * rSq + 
                            hmdWarpParam.w * rSq * rSq * rSq); 
   // Chromatic aberration correction 
   vec2 thetaBlue = rvector * (chromAbParam.z + chromAbParam.w * rSq); 
   vec2 tcBlue = inputCenter + scale * thetaBlue; 
   // Blue is farthest out 
   if ( (abs(tcBlue.x - 0.5) > 0.5) || (abs(tcBlue.y - 0.5) > 0.5) ) { 
        gl_FragColor = vec4(0, 0, 0, 1); 
        return; 
   } 
   vec2 thetaRed = rvector * (chromAbParam.x + chromAbParam.y * rSq); 
   vec2 tcRed = inputCenter + scale * thetaRed; 
   vec2 tcGreen = inputCenter + scale * rvector; // green 
   tcRed.x *= 0.5; // because output only goes to 0-0.5 (left eye) 
   tcGreen.x *= 0.5; // because output only goes to 0-0.5 (left eye) 
   tcBlue.x *= 0.5; // because output only goes to 0-0.5 (left eye) 
   if (tcIn.x > 0.5) { // right eye 0.5-1.0 
        tcRed.x = 1 - tcRed.x; 
        tcGreen.x = 1 - tcGreen.x; 
        tcBlue.x = 1 - tcBlue.x; 
    } 
    float red = texture2D(texture, tcRed).r; 
    vec2 green = texture2D(texture, tcGreen).ga; 
    float blue = texture2D(texture, tcBlue).b; 

    gl_FragColor = vec4(red, green.x, blue, green.y); 
} 
