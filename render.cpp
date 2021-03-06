#include "render.h"
#include "shader.h"
#include "opengl.h"
#include "appconfig.h"
#include "win.h"

#include "FullbrightShader.h"
#include "FullbrightTexturedShader.h"
#include "PhongTexturedShader.h"
#include "OculusShader.h"

#ifdef DEBUG
// If there is a glError this outputs it along with a message to stderr.
// otherwise there is no output.
void GLErrorCheck(const char* message)
{
    GLenum val = glGetError();
    switch (val)
    {
    case GL_INVALID_ENUM:
        fprintf(stderr, "GL_INVALID_ENUM : %s\n", message);
        break;
    case GL_INVALID_VALUE:
        fprintf(stderr, "GL_INVALID_VALUE : %s\n", message);
        break;
    case GL_INVALID_OPERATION:
        fprintf(stderr, "GL_INVALID_OPERATION : %s\n", message);
        break;
#ifndef GL_ES_VERSION_2_0
    case GL_STACK_OVERFLOW:
        fprintf(stderr, "GL_STACK_OVERFLOW : %s\n", message);
        break;
    case GL_STACK_UNDERFLOW:
        fprintf(stderr, "GL_STACK_UNDERFLOW : %s\n", message);
        break;
#endif
    case GL_OUT_OF_MEMORY:
        fprintf(stderr, "GL_OUT_OF_MEMORY : %s\n", message);
        break;
    case GL_NO_ERROR:
        break;
    }
}
#endif

FullbrightShader* s_fullbrightShader = 0;
FullbrightTexturedShader* s_fullbrightTexturedShader = 0;
FullbrightTexturedShader* s_fullbrightTexturedTextShader = 0;
PhongTexturedShader* s_phongTexturedShader = 0;
OculusShader* s_oculusShader = 0;
Shader* s_prevShader = 0;
GLuint s_checker = 0;

static GLint CreateCheckerTexture()
{
    GLuint retVal = 0;
    glGenTextures(1, &retVal);
    glBindTexture(GL_TEXTURE_2D, retVal);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const int W = 512;
    const uint8_t O = 100;
    const uint8_t X = 255;
    uint8_t* checker = new uint8_t[W * W];
    int i, j;
    for (i = 0; i < W; i++) {
        for (j = 0; j < W; j++) {
            if (i < W/2) {
                if (j < W/2)
                    checker[i * W + j] = O;
                else
                    checker[i * W + j] = X;
            } else {
                if (j < W/2)
                    checker[i * W + j] = X;
                else
                    checker[i * W + j] = O;
            }
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, W, W, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, checker);
    delete [] checker;
    return retVal;
}

void RenderInit()
{
    s_fullbrightShader = new FullbrightShader();
    bool success = s_fullbrightShader->compileAndLinkFromFiles("shader/fullbright.vsh",
                                                               "shader/fullbright.fsh");
    if (!success)
        exit(EXIT_FAILURE);

    s_fullbrightTexturedShader = new FullbrightTexturedShader();
    success = s_fullbrightTexturedShader->compileAndLinkFromFiles("shader/fullbright_textured.vsh",
                                                                  "shader/fullbright_textured.fsh");
    if (!success)
        exit(EXIT_FAILURE);

    s_fullbrightTexturedTextShader = new FullbrightTexturedShader();
    success = s_fullbrightTexturedTextShader->compileAndLinkFromFiles("shader/fullbright_textured.vsh",
                                                                      "shader/fullbright_textured_text.fsh");
    if (!success)
        exit(EXIT_FAILURE);

    s_phongTexturedShader = new PhongTexturedShader();
    success = s_phongTexturedShader->compileAndLinkFromFiles("shader/phong_textured.vsh",
                                                             "shader/phong_textured.fsh");
    if (!success)
        exit(EXIT_FAILURE);

    s_oculusShader = new OculusShader();
    success = s_oculusShader->compileAndLinkFromFiles("shader/oculus.vsh",
                                                      "shader/oculus-simple.fsh");
    if (!success)
        exit(EXIT_FAILURE);

    s_checker = CreateCheckerTexture();
}

void RenderShutdown()
{
    delete s_fullbrightShader;
    delete s_fullbrightTexturedShader;
    delete s_fullbrightTexturedTextShader;
}

static Vector4f UintColorToVector4(uint32_t color)
{
    return Vector4f((color & 0xff) / 255.0f,
                    ((color >> 8) & 0xff) / 255.0f,
                    ((color >> 16) & 0xff) / 255.0f,
                    ((color >> 24) & 0xff) / 255.0f);
}

void RenderBegin()
{
    s_prevShader = 0;
}

void RenderEnd()
{

}

void RenderTextBegin(const Matrixf& projMatrix, const Matrixf& viewMatrix, const Matrixf& modelMatrix)
{
    Matrixf fullMatrix = projMatrix * viewMatrix * modelMatrix;
    s_fullbrightShader->setMat(fullMatrix);
    s_fullbrightTexturedShader->setMat(fullMatrix);
    s_fullbrightTexturedTextShader->setMat(fullMatrix);
}

void RenderText(GB_GlyphQuad* quads, uint32_t num_quads)
{
    static uint16_t indices[] = {0, 2, 1, 2, 3, 1};

    uint32_t i = 0;
    for (i = 0; i < num_quads; ++i)
    {
        const WIN_TextUserData* data = (const WIN_TextUserData*)quads[i].user_data;
        Vector4f bg_color = UintColorToVector4(data->bg_color);
        bg_color.w = 0.5;
        s_fullbrightShader->setColor(bg_color);

        uint32_t y_offset = data->line_height / 3; // hack
        Vector2f origin = Vector2f(quads[i].pen[0], quads[i].pen[1] + y_offset);
        Vector2f size = Vector2f(data->max_advance, -(float)data->line_height);

        float pos[12] = {
            origin.x, origin.y, 0,
            origin.x + size.x, origin.y, 0,
            origin.x, origin.y + size.y, 0,
            origin.x + size.x, origin.y + size.y
        };

        s_fullbrightShader->apply(s_prevShader, pos);
        s_prevShader = s_fullbrightShader;

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    }

    const float kDepthOffset = 3.0f;
    for (i = 0; i < num_quads; i++) {
        const WIN_TextUserData* data = (const WIN_TextUserData*)quads[i].user_data;

        if (quads[i].size[0] > 0 && quads[i].size[1] > 0)
        {
            Vector2f origin = Vector2f(quads[i].origin[0], quads[i].origin[1]);
            Vector2f size = Vector2f(quads[i].size[0], quads[i].size[1]);
            Vector2f uv_origin = Vector2f(quads[i].uv_origin[0], quads[i].uv_origin[1]);
            Vector2f uv_size = Vector2f(quads[i].uv_size[0], quads[i].uv_size[1]);
            float attrib[20] = {
                origin.x, origin.y, kDepthOffset, uv_origin.x, uv_origin.y,
                origin.x + size.x, origin.y, kDepthOffset, uv_origin.x + uv_size.x, uv_origin.y,
                origin.x, origin.y + size.y, kDepthOffset, uv_origin.x, uv_origin.y + uv_size.y,
                origin.x + size.x, origin.y + size.y, kDepthOffset, uv_origin.x + uv_size.x, uv_origin.y + uv_size.y
            };

            s_fullbrightTexturedTextShader->setColor(UintColorToVector4(data->fg_color));
            s_fullbrightTexturedTextShader->setTex(quads[i].gl_tex_obj);
            s_fullbrightTexturedTextShader->apply(s_prevShader, attrib);
            s_prevShader = s_fullbrightTexturedTextShader;

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
        }
    }
    GL_ERROR_CHECK("TextRenderFunc");
}

void RenderTextEnd()
{

}

void RenderFloor(const Matrixf& projMatrix, const Matrixf& viewMatrix, float height)
{
    Matrixf worldMatrix = Matrixf::Trans(Vector3f(0, height, 0));
    Matrixf normalMatrix = worldMatrix;
    normalMatrix.SetTrans(Vector3f(0, 0, 0));

    const float kFeetToCm = 30.48;
    const float kInchesToCm = 2.54;

    s_phongTexturedShader->setFullMat(projMatrix * viewMatrix * worldMatrix);
    s_phongTexturedShader->setWorldMat(worldMatrix);
    s_phongTexturedShader->setWorldNormalMat(normalMatrix);
    s_phongTexturedShader->setColor(Vector4f(1, 1, 1, 1));
    s_phongTexturedShader->setTex(s_checker);

    static bool init = false;
    if (!init) {
        std::vector<Vector3f> lightWorldPos;
        lightWorldPos.push_back(Vector3f(0, 4 * kFeetToCm, 0));
        lightWorldPos.push_back(Vector3f(10 * kFeetToCm, 4 * kFeetToCm, 0.0f));
        lightWorldPos.push_back(Vector3f(0, 4 * kFeetToCm, 10 * kFeetToCm));
        s_phongTexturedShader->setLightWorldPos(lightWorldPos);

        std::vector<Vector3f> lightColor;
        lightColor.push_back(Vector3f(1, 1, 1));
        lightColor.push_back(Vector3f(1, 0, 0));
        lightColor.push_back(Vector3f(0, 0, 1));
        s_phongTexturedShader->setLightColor(lightColor);

        std::vector<float> lightStrength;
        lightStrength.push_back(200);
        lightStrength.push_back(100);
        lightStrength.push_back(100);
        s_phongTexturedShader->setLightStrength(lightStrength);

        s_phongTexturedShader->setNumLights(3);
        init = true;
    }

    float kOffset = 1000.0f * kFeetToCm;
    float kTexOffset = 100.0f;
    float attrib[32] = {
        0 - kOffset, 0, 0 - kOffset, 0, 0, 0, 1, 0,
        0 + kOffset, 0, 0 - kOffset, 0 + kTexOffset, 0, 0, 1, 0,
        0 - kOffset, 0, 0 + kOffset, 0, 0 + kTexOffset, 0, 1, 0,
        0 + kOffset, 0, 0 + kOffset, 0 + kTexOffset, 0 + kTexOffset, 0, 1, 0
    };
    s_phongTexturedShader->apply(s_prevShader, attrib);

    static uint16_t indices[] = {0, 2, 1, 2, 3, 1};
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void RenderFullScreenQuad(GLuint texture, int width, int height)
{
    Matrixf projMatrix = Matrixf::Ortho(0, width, 0, height, -1, 1);

    s_fullbrightTexturedShader->setMat(projMatrix);
    s_fullbrightTexturedShader->setColor(Vector4f(1, 1, 1, 1));
    s_fullbrightTexturedShader->setTex(texture);

    float attrib[20] = {
        0, 0, 0, 0, 0,
        width, 0, 0, 1, 0,
        0, height, 0, 0, 1,
        width, height, 0, 1, 1
    };

    s_fullbrightTexturedShader->apply(s_prevShader, attrib);
    s_prevShader = s_fullbrightTexturedShader;

    static uint16_t indices[] = {0, 2, 1, 2, 3, 1};
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}

void RenderPostProcessWarp(OVR::Util::Render::StereoConfig& sConfig, uint32_t texture, bool left)
{
    OVR::Util::Render::StereoEyeParams eyeParams = sConfig.GetEyeRenderParams(left ? OVR::Util::Render::StereoEye_Left : OVR::Util::Render::StereoEye_Right);
    OVR::Util::Render::DistortionConfig dConfig = *eyeParams.pDistortion;

    glViewport(eyeParams.VP.x, eyeParams.VP.y, eyeParams.VP.w, eyeParams.VP.h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(eyeParams.VP.x, 0, eyeParams.VP.w, eyeParams.VP.h);
    if (left)
        glClearColor(1, 0, 0, 1);
    else
        glClearColor(0, 0, 1, 1);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float w = float(eyeParams.VP.w) / float(sConfig.GetFullViewport().w),
          h = float(eyeParams.VP.h) / float(sConfig.GetFullViewport().h),
          x = float(eyeParams.VP.x) / float(sConfig.GetFullViewport().w),
          y = float(eyeParams.VP.y) / float(sConfig.GetFullViewport().h);

    float as = (float)eyeParams.VP.w / (float)eyeParams.VP.h;

    // We are using 1/4 of DistortionCenter offset value here, since it is
    // relative to [-1,1] range that gets mapped to [0, 0.5].
    float sign = left ? 1 : -1;
    s_oculusShader->setLensCenter(Vector2f(x + (w + sign * dConfig.XCenterOffset * 0.5f)*0.5f,
                                           y + h*0.5f));

    s_oculusShader->setScreenCenter(Vector2f(x + w*0.5f, y + h*0.5f));

    // MA: This is more correct but we would need higher-res texture vertically; we should adopt this
    // once we have asymmetric input texture scale.
    float scaleFactor = 1.0f / dConfig.Scale;

    s_oculusShader->setScale(Vector2f((w/2) * scaleFactor, (h/2) * scaleFactor * as));
    s_oculusShader->setScaleIn(Vector2f((2/w), (2/h) / as));
    s_oculusShader->setHmdWarpParam(Vector4f(dConfig.K[0], dConfig.K[1], dConfig.K[2], dConfig.K[3]));

#if 0
        s_oculusShader->setChromAbParam(Vector4f(dConfig.ChromaticAberration[0],
                                                 dConfig.ChromaticAberration[1],
                                                 dConfig.ChromaticAberration[2],
                                                 dConfig.ChromaticAberration[3]));
#endif

    Matrixf texm = Matrixf::Rows(Vector4f(w, 0, 0, x),
                                 Vector4f(0, h, 0, y),
                                 Vector4f(0, 0, 0, 0),
                                 Vector4f(0, 0, 0, 1));
    s_oculusShader->setTexm(texm);

    Matrixf view = Matrixf::Rows(Vector4f(2, 0, 0, -1),
                                 Vector4f(0, 2, 0, -1),
                                 Vector4f(0, 0, 0, 0),
                                 Vector4f(0, 0, 0, 1));

    s_oculusShader->setView(view);
    s_oculusShader->setTexture0(texture);

    float attrib[6 * 4] = {
        0, 1, 0, 1, 0, 0,
        1, 1, 0, 1, 1, 0,
        0, 0, 0, 1, 0, 1,
        1, 0, 0, 1, 1, 1
    };

    s_oculusShader->apply(s_prevShader, attrib);
    s_prevShader = s_oculusShader;

    static uint16_t indices[] = {0, 2, 1, 2, 3, 1};
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
}
