#include "renderer/layers/gvizDiscShader.h"

#include <stdio.h>
#include <stdlib.h>

#define GL_SILENCE_DEPRECATION
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

static const char *DISC_VS_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec3 aCenter;\n"
    "layout(location=1) in float aRadius;\n"
    "layout(location=2) in vec2 aCorner;\n"
    "layout(location=3) in float aHighlight;\n"
    "uniform mat4 uMVP;\n"
    "uniform vec2 uViewport;\n"
    "out vec2 vLocal;\n"
    "out float vHighlight;\n"
    "void main() {\n"
    "    vec4 clip = uMVP * vec4(aCenter, 1.0);\n"
    "    clip.xy += aCorner * (aRadius * 2.0 / uViewport) * clip.w;\n"
    "    gl_Position = clip;\n"
    "    vLocal = aCorner;\n"
    "    vHighlight = aHighlight;\n"
    "}\n";

static const char *DISC_FS_SRC =
    "#version 330 core\n"
    "in vec2 vLocal;\n"
    "in float vHighlight;\n"
    "uniform vec4 uColor;\n"
    "uniform float uFill;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    float d = length(vLocal);\n"
    "    if (d > 1.0) discard;\n"
    "    if (uFill < 0.5 && d < 0.65) discard;\n"
    "    float aa = fwidth(d);\n"
    "    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0, d);\n"
    "    vec3 base = mix(uColor.rgb, vec3(1.0, 0.0, 0.0), step(0.5, vHighlight));\n"
    "    fragColor = vec4(base, 1.0);\n"
    "}\n";

static gvizDiscShader g_shader = {0};
static int g_initialized = 0;

static unsigned int compileShader(GLenum type, const char *src) {
  GLuint id = glCreateShader(type);
  glShaderSource(id, 1, &src, NULL);
  glCompileShader(id);
  GLint ok = 0;
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    GLsizei n = 0;
    glGetShaderInfoLog(id, sizeof(log), &n, log);
    fprintf(stderr, "[gvizDiscShader] compile failed (type=0x%x): %.*s\n", type,
            (int)n, log);
    glDeleteShader(id);
    return 0;
  }
  return id;
}

static unsigned int linkProgram(unsigned int vs, unsigned int fs) {
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    GLsizei n = 0;
    glGetProgramInfoLog(prog, sizeof(log), &n, log);
    fprintf(stderr, "[gvizDiscShader] link failed: %.*s\n", (int)n, log);
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}

const gvizDiscShader *gvizDiscShaderGet(void) {
  if (g_initialized)
    return &g_shader;

  unsigned int vs = compileShader(GL_VERTEX_SHADER, DISC_VS_SRC);
  unsigned int fs = compileShader(GL_FRAGMENT_SHADER, DISC_FS_SRC);
  if (!vs || !fs) {
    if (vs)
      glDeleteShader(vs);
    if (fs)
      glDeleteShader(fs);
    return NULL;
  }

  unsigned int prog = linkProgram(vs, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!prog)
    return NULL;

  g_shader.programId = prog;
  g_shader.locMVP = glGetUniformLocation(prog, "uMVP");
  g_shader.locViewport = glGetUniformLocation(prog, "uViewport");
  g_shader.locColor = glGetUniformLocation(prog, "uColor");
  g_shader.locFill = glGetUniformLocation(prog, "uFill");
  g_initialized = 1;
  return &g_shader;
}

void gvizDiscShaderRelease(void) {
  if (!g_initialized)
    return;
  if (g_shader.programId)
    glDeleteProgram(g_shader.programId);
  g_shader.programId = 0;
  g_initialized = 0;
}
