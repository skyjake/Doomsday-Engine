#ifndef GLOOM_EXPOSURE_H
#define GLOOM_EXPOSURE_H

uniform sampler2D uBrightnessSamples;

float Gloom_Exposure(void) {
    ivec2 sampleSize = textureSize(uBrightnessSamples, 0);
    vec3 channelWeights = vec3(0.2125, 0.7154, 0.0721);
    float brightest = 0.0;
    for (int x = 0; x < sampleSize.x; ++x) {
        for (int y = 0; y < sampleSize.y; ++y) {
            vec3 bsample = texelFetch(uBrightnessSamples, ivec2(x, y), 0).rgb;
            brightest = max(brightest, dot(bsample, channelWeights));
        }
    }
    return min(1.0, 1.75 / brightest); // never boost exposure to keep darkness dark
}

#endif // GLOOM_EXPOSURE_H
