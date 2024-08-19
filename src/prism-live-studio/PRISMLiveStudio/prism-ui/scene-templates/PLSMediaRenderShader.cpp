#include "PLSMediaRenderShader.h"

const char *SHADER_FOR_VERTEX = R"(
    attribute vec2 position;
    attribute vec2 texcoord;

    varying vec2 TexCoord;

    uniform mat4 matFullScreen;

    void main()
    {
        TexCoord = texcoord;
        gl_Position = vec4(position, 0, 1) * matFullScreen;
    }
)";

const char *SHADER_FOR_YUV420P = R"(
    varying vec2 TexCoord;

    uniform sampler2D textureY;
    uniform sampler2D textureU;
    uniform sampler2D textureV;

    void main() {
        float Y = texture2D(textureY, TexCoord).r;
        float U = texture2D(textureU, TexCoord).r;
        float V = texture2D(textureV, TexCoord).r;

        gl_FragColor.r = 1.164*(Y-16.0/255.0)+1.596*(V-128.0/255.0);
        gl_FragColor.g = 1.164*(Y-16.0/255.0)-0.813*(V-128.0/255.0)-0.391*(U-128.0/255.0);
        gl_FragColor.b = 1.164*(Y-16.0/255.0)+2.018*(U-128.0/255.0);
    }
)";

const char *SHADER_FOR_NV12 = R"(
    varying vec2 TexCoord;

    uniform sampler2D textureY;
    uniform sampler2D textureUV;

    void main() {
        float Y = texture2D(textureY, TexCoord).r;
        vec2 UV = texture2D(textureUV, TexCoord).rg;
        float U = UV.r;
        float V = UV.g;

        gl_FragColor.r = 1.164*(Y-16.0/255.0)+1.596*(V-128.0/255.0);
        gl_FragColor.g = 1.164*(Y-16.0/255.0)-0.813*(V-128.0/255.0)-0.391*(U-128.0/255.0);
        gl_FragColor.b = 1.164*(Y-16.0/255.0)+2.018*(U-128.0/255.0);
    }
)";

const char *SHADER_FOR_BGRA = R"(
    varying vec2 TexCoord;

    uniform sampler2D textureBGRA;

    void main() {
        gl_FragColor = texture2D(textureBGRA, TexCoord);
    }
)";
