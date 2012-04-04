/* Released into the Public Domain */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glfw.h>
#include <bsm.h>

static float mat_proj[16];
static float mat_view[16];
static float mat_model[16];

static GLuint vbo_model_pos;
static GLuint vbo_model_tex;
static GLuint vbo_model_norm;
static GLuint vbo_model_tan;

static GLuint ebo_model;
static GLuint vao_model;

static GLuint ubo_scene;

static bool isrunning = true;

static void mat_mul(float *dst, float *A, float *B) {
  static float C[16];
  memset(C, 0, 16 * sizeof(float));
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      for (int i = 0; i < 4; i++) {
        C[c*4+r] += A[i*4+r] * B[c*4+i];
      }
    }
  }
  memcpy(dst, C, 16 * sizeof(float));
}

static void mat_rotx(float *dst, float theta) {
  float x = cosf(theta);
  float y = sinf(theta);
  memcpy(dst, (float[16]){
    1.0f,  0.0f,  0.0f, 0.0f,
    0.0f,  x,     y,    0.0f,
    0.0f, -y,     x,    0.0f,
    0.0f,  0.0f,  0.0f, 1.0f
  }, 16*sizeof(float));
}

static void mat_roty(float *dst, float theta) {
  float x = cosf(theta);
  float y = sinf(theta);
  memcpy(dst, (float[16]){
    x,    0.0f, -y,    0.0f,
    0.0f, 1.0f,  0.0f, 0.0f,
    y,    0.0f,  x,    0.0f,
    0.0f, 0.0f,  0.0f, 1.0f
  }, 16*sizeof(float));
}

static void reshape(int width, int height) {
  glViewport(0, 0, width, height);
  float ratio = (float)width / (float)height;
  memcpy(mat_proj, (float[16]){
    1.0f / ratio, 0.0f,  0.0f, 0.0f,
    0.0f,         1.0f,  0.0f, 0.0f,
    0.0f,         0.0f, -0.5f, 0.0f,
    0.0f,         0.0f,  0.0f, 1.0f
  }, 16*sizeof(float));
}

static int close(void) {
  isrunning = false;
  return GL_TRUE;
}

static void mouse(int x, int y, int state) {
  static bool down = false;
  static int old_x = 0;
  static int old_y = 0;
  
  if (down && x >= 0 && y >= 0) {
    float dx = x - old_x;
    float dy = y - old_y;
    float theta_x = 3.14159f * dx / 800.0f;
    float theta_y = 3.14159f * dy / 800.0f;
    static float rot_x[16];
    static float rot_y[16];
    mat_rotx(rot_x, theta_y);
    mat_roty(rot_y, theta_x);
    mat_mul(mat_view, rot_x, mat_view);
    mat_mul(mat_view, rot_y, mat_view);
  }
  
  if (x >= 0) old_x = x;
  if (y >= 0) old_y = y;
  if (state >= 0) down = (state == 1);
}

static void mousemove(int x, int y) {
  mouse(x, y, -1);
}

static void mouseclick(int button, int action) {
  if (button != GLFW_MOUSE_BUTTON_LEFT) return;
  mouse(-1, -1, action == GLFW_PRESS ? 1 : 0);
}

static bool drawnorms = true;
static int  shademode = 0;
static void keypress(int key, int action) {
  if (action != GLFW_RELEASE) return;
  switch (key) {
    case '1':
      drawnorms = !drawnorms;
      break;
    case '2':
      shademode = 0;
      break;
    case '3':
      shademode = 1;
      break;
  }
}

static bool init_window() {
  glfwInit();
  
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  
  if (glfwOpenWindow(1280, 720, 8, 8, 8, 0, 24, 8, GLFW_WINDOW) < 0) {
    fprintf(stderr, "Failed to open window\n");
    return false;
  }
  
  glfwSetWindowSizeCallback(&reshape);
  glfwSetWindowCloseCallback(&close);
  glfwSetMousePosCallback(&mousemove);
  glfwSetMouseButtonCallback(&mouseclick);
  glfwSetKeyCallback(&keypress);

  glfwSetWindowTitle("Binary Static Mesh Viewer");

  glfwSwapInterval(0);
  
  return true;
}

GLuint create_vbo(void *data, int n) {
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return vbo;
}

GLuint create_ebo(int *data, int n) {
  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, n, data, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return ebo;
}

GLuint create_vao(GLuint *vbo, GLint *size, GLenum *type, GLuint ebo, int n) {
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  for (int i=0; i < n; i++) {
    glEnableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
    glVertexAttribPointer(i, size[i], type[i], GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBindVertexArray(0);
  return vao;
}

static bool create_shader(char *name, GLenum type, const char *src, GLuint *shader) {
  static char log[0x1000];

  int status;
  int n = strlen(src);

  GLuint id = glCreateShader(type);
  *shader = id;
  glShaderSource(id, 1, &src, &n);
  glCompileShader(id);
  glGetShaderiv(id, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderInfoLog(id, 0x1000, NULL, log);
    fprintf(stderr, "Render: Failed to compile %s\n\tDetails: %s\n", name, log);
    return false;
  }
  return true;
}

static bool create_program(char *name, GLuint *shaders, int n, GLuint *program) {
  static char log[0x1000];
  
  int status;

  GLuint id = glCreateProgram();
  *program = id;
  for (int i=0; i < n; i++) {
    glAttachShader(id, shaders[i]);
  }
  glLinkProgram(id);
  glGetProgramiv(id, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    glGetProgramInfoLog(id, 0x1000, NULL, log);
    fprintf(stderr, "Render: Failed to link %s\n\tDetails: %s\n", name, log);
    return false;
  }
  return true;
}

static void bind_ubo(GLuint program, char *name, GLuint binding) {
  GLuint index = glGetUniformBlockIndex(program, name);
  glUniformBlockBinding(program, index, binding);
}

static void update_scene() {
  static float scene[48];
  memcpy(&scene[ 0], mat_model, 16*sizeof(float));
  memcpy(&scene[16], mat_view, 16*sizeof(float));
  memcpy(&scene[32], mat_proj, 16*sizeof(float));
  glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
  glBufferData(GL_UNIFORM_BUFFER, 48*sizeof(float), &scene, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static GLuint program_default;
static GLuint program_normals;
static GLuint program_texcoord;
static bool init_opengl() {
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if (err != GLEW_OK) {
    fprintf(stderr, "Failed to load GLEW: %s\n", glewGetErrorString(err));
    return false;
  }

  glClearColor(0.39f, 0.58f, 0.93f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  
  glGenBuffers(1, &ubo_scene);
  
  GLuint vshader, gshader, fshader;
  
  if (!create_shader("default vertex shader", GL_VERTEX_SHADER,
    "#version 330\n"
    "layout(std140) uniform scene {\n"
    "  uniform mat4 mat_model;\n"
    "  uniform mat4 mat_view;\n"
    "  uniform mat4 mat_proj;\n"
    "};\n"
    "layout(location = 0) in vec3 vposition;\n"
    "layout(location = 1) in vec2 vtexcoord;\n"
    "layout(location = 2) in vec3 vnormal;\n"
    "layout(location = 3) in vec4 vtangent;\n"
    "smooth out vec2 texcoord;\n"
    "smooth out mat3 tbnmatrix;\n"
    "void main() {\n"
    "  texcoord = vtexcoord;\n"
    "  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
    "  tbnmatrix = mat3(mat_view[0].xyz, mat_view[1].xyz, mat_view[2].xyz) * mat3(vtangent.xyz, vbitangent, vnormal);\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(vposition, 1.0);\n"
    "}\n", &vshader)) return false;
  
  if (!create_shader("default fragment shader", GL_FRAGMENT_SHADER,
    "#version 330\n"
    "smooth in vec2 texcoord;\n"
    "smooth in mat3 tbnmatrix;\n"
    "const vec3 L1 = vec3( 0.707,  0.0,   -0.707);\n"
    "const vec3 L2 = vec3(-0.707,  0.0,   -0.707);\n"
    "const vec3 L3 = vec3(-0.577, -0.577,  0.577);\n"
    "layout(location = 0) out vec4 color;\n"
    "float sgamma(float x) {\n"
    "  if (x <= 0.0031308) return 12.92 * x;\n"
    "  return 1.055 * pow(x, 1.0/2.4) - 0.055;\n"
    "}\n"
    "vec3 srgb(vec3 c) {\n"
    "  return vec3(sgamma(c.r), sgamma(c.g), sgamma(c.b));\n"
    "}\n"
    "void main() {\n"
    "  vec3 N = normalize(tbnmatrix * vec3(0.0, 0.0, 1.0));\n"
    "  vec3 c = vec3(0.0, 0.0, 0.0);\n"
    "  c += 0.5 * vec3(1.0, 0.8, 0.6) * max(0.0, dot(N, -L1));\n"
    "  c += 0.5 * vec3(1.0, 0.8, 0.6) * max(0.0, dot(N, -L2));\n"
    "  c += 0.5 * vec3(0.8, 0.8, 1.0) * max(0.0, dot(N, -L3));\n"
    "  color = vec4(srgb(c), 1.0);\n"
    "}\n", &fshader)) return false;
  
  if (!create_program("default program", (GLuint[2]){ vshader, fshader }, 2, &program_default)) return false;
  bind_ubo(program_default, "scene", 0);
  
  if (!create_shader("normals vertex shader", GL_VERTEX_SHADER,
    "#version 330\n"
    "layout(std140) uniform scene {\n"
    "  uniform mat4 mat_model;\n"
    "  uniform mat4 mat_view;\n"
    "  uniform mat4 mat_proj;\n"
    "};\n"
    "layout(location = 0) in vec3 vposition;\n"
    "layout(location = 1) in vec2 vtexcoord;\n"
    "layout(location = 2) in vec3 vnormal;\n"
    "layout(location = 3) in vec4 vtangent;\n"
    "out vec3 position;\n"
    "out mat3 tbnmatrix;\n"
    "void main() {\n"
    "  position = vposition;\n"
    "  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
    "  tbnmatrix = mat3(vtangent.xyz, vbitangent, vnormal);\n"
    "  gl_Position = vec4(position, 1.0);\n"
    "}\n", &vshader)) return false;
  
  if (!create_shader("normals geometry shader", GL_GEOMETRY_SHADER,
    "#version 330\n"
    "layout(std140) uniform scene {\n"
    "  uniform mat4 mat_model;\n"
    "  uniform mat4 mat_view;\n"
    "  uniform mat4 mat_proj;\n"
    "};\n"
    "layout(points) in;\n"
    "layout(line_strip, max_vertices = 6) out;\n"
    "in vec3 position[1];\n"
    "in mat3 tbnmatrix[1];\n"
    "smooth out vec3 color;\n"
    "void main() {\n"
    "  vec3 origin = position[0];\n"
    "  vec3 x = 0.125 * tbnmatrix[0] * vec3(1.0, 0.0, 0.0);\n"
    "  vec3 y = 0.125 * tbnmatrix[0] * vec3(0.0, 1.0, 0.0);\n"
    "  vec3 z = 0.125 * tbnmatrix[0] * vec3(0.0, 0.0, 1.0);\n"
    ""
    "  color = vec3(1.0, 0.0, 0.0);\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin, 1.0);\n"
    "  EmitVertex();\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin + x, 1.0);\n"
    "  EmitVertex();\n"
    "  EndPrimitive();\n"
    ""
    "  color = vec3(0.0, 1.0, 0.0);\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin, 1.0);\n"
    "  EmitVertex();\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin + y, 1.0);\n"
    "  EmitVertex();\n"
    "  EndPrimitive();\n"
    ""
    "  color = vec3(0.0, 0.0, 1.0);\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin, 1.0);\n"
    "  EmitVertex();\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(origin + z, 1.0);\n"
    "  EmitVertex();\n"
    "  EndPrimitive();\n"
    "}\n", &gshader)) return false;
  
  if (!create_shader("normals fragment shader", GL_FRAGMENT_SHADER,
    "#version 330\n"
    "smooth in vec3 color;\n"
    "layout(location = 0) out vec4 outcolor;\n"
    "void main() {\n"
    "  outcolor = vec4(color, 1.0);\n"
    "}\n", &fshader)) return false;
  
  if (!create_program("normals program", (GLuint[3]){ vshader, gshader, fshader }, 3, &program_normals)) return false;
  bind_ubo(program_normals, "scene", 0);
  
  if (!create_shader("texcoord vertex shader", GL_VERTEX_SHADER,
    "#version 330\n"
    "layout(std140) uniform scene {\n"
    "  uniform mat4 mat_model;\n"
    "  uniform mat4 mat_view;\n"
    "  uniform mat4 mat_proj;\n"
    "};\n"
    "layout(location = 0) in vec3 vposition;\n"
    "layout(location = 1) in vec2 vtexcoord;\n"
    "layout(location = 2) in vec3 vnormal;\n"
    "layout(location = 3) in vec4 vtangent;\n"
    "smooth out vec2 texcoord;\n"
    "void main() {\n"
    "  texcoord = vtexcoord;\n"
    "  gl_Position = mat_proj * mat_view * mat_model * vec4(vposition, 1.0);\n"
    "}\n", &vshader)) return false;
  
  if (!create_shader("texcoord fragment shader", GL_FRAGMENT_SHADER,
    "#version 330\n"
    "smooth in vec2 texcoord;\n"
    "layout(location = 0) out vec4 color;\n"
    "void main() {\n"
    "  color = vec4(texcoord, 0.0, 1.0);\n"
    "}\n", &fshader)) return false;
    
  if (!create_program("texcoord program", (GLuint[2]){ vshader, fshader }, 2, &program_texcoord)) return false;
  bind_ubo(program_texcoord, "scene", 0);
  
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_scene);
  
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: bsmviewer <filename>\n");
    return 1;
  }
  
  char *path = argv[1];
  
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    printf("Failed to open file!\n");
    return 1;
  }
  
  int size;
  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);
  uint8_t *data = malloc(size);
  if (fread(data, 1, size, file) != size) {
    printf("Failed to read file!\n");
    return 1;
  }
  
  bsm_header_v1_t header;
  if(!bsm_read_header_v1(data, size, &header)) {
    printf("File is not a valid Binary Static Mesh!\n");
    return 1;
  }
  printf("File size: %d bytes\n", size);
  printf("HEADER:\n");
  printf("Magic Number: %x %x %x %x\n", header.magic[0], header.magic[1], header.magic[2], header.magic[3]);
  printf("Version: %d\n", header.version);
  printf("Extension ID: %d\n", header.extension);
  printf("Bounding Sphere: <%.3f, %.3f, %.3f> %.3f\n", header.bsphere.x, header.bsphere.y, header.bsphere.z, header.bsphere.radius);
  printf("Bounding Box:\n\t%.3f <= x <= %0.3f\n\t%.3f <= y <= %.3f\n\t%.3f <= z <= %.3f\n", header.bbox.x0, header.bbox.x1, header.bbox.y0, header.bbox.y1, header.bbox.z0, header.bbox.z1);
  printf("Verts:\n\tCount: %d\n\tPosition Offset: %d\n\tTexcoord Offset: %d\n\tNormal Offset:   %d\n\tTangent Offset:  %d\n", header.num_verts, header.offs_positions, header.offs_texcoords, header.offs_normals, header.offs_tangents);
  printf("Triangles:               %d (offset: %d)\n", header.num_tris, header.offs_tris);
  printf("Meshes:                  %d (offset: %d)\n", header.num_meshes, header.offs_meshes);
  printf("Collision Hull Vertices: %d (offset: %d)\n", header.num_hullverts, header.offs_hullverts);
  printf("Collision Hulls:         %d (offset: %d)\n", header.num_hulls, header.offs_hulls);
  printf("Occlusion Mesh Vertices: %d (offset: %d)\n", header.num_visverts, header.offs_visverts);
  printf("Occlusion Triangles:     %d (offset: %d)\n", header.num_vistris, header.offs_vistris);

  if (!init_window()) return -1;
  if (!init_opengl()) return -1;
  
  bsm_position_t *posbuf  = malloc(bsm_positions_bytes(&header));
  bsm_texcoord_t *texbuf  = malloc(bsm_texcoords_bytes(&header));
  bsm_normal_t   *normbuf = malloc(bsm_normals_bytes(&header));
  bsm_tangent_t  *tanbuf  = malloc(bsm_tangents_bytes(&header));
  bsm_triangle_t *tribuf  = malloc(bsm_tris_bytes(&header));
  bsm_mesh_t     *meshes  = malloc(bsm_meshes_bytes(&header));
  
  bsm_read_positions(data, size, &header, posbuf);
  bsm_read_texcoords(data, size, &header, texbuf);
  bsm_read_normals(data, size, &header, normbuf);
  bsm_read_tangents(data, size, &header, tanbuf);
  bsm_read_tris(data, size, &header, tribuf);
  bsm_read_meshes(data, size, &header, meshes);
  
  free(data);
  
  vbo_model_pos  = create_vbo(posbuf,  bsm_positions_bytes(&header));
  vbo_model_tex  = create_vbo(texbuf,  bsm_texcoords_bytes(&header));
  vbo_model_norm = create_vbo(normbuf, bsm_normals_bytes(&header));
  vbo_model_tan  = create_vbo(tanbuf,  bsm_tangents_bytes(&header));
  
  ebo_model = create_ebo((int*)tribuf, bsm_tris_bytes(&header));
  
  vao_model = create_vao(
    (GLuint[4]){ vbo_model_pos, vbo_model_tex, vbo_model_norm, vbo_model_tan },
    (GLint[4]){ 3, 2, 3, 4 },
    (GLenum[4]){ GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT },
    ebo_model, 4);
    
  free(posbuf);
  free(texbuf);
  free(normbuf);
  free(tanbuf);
  free(tribuf);
  
  float scale = 1.0f / header.bsphere.radius;
  float x = header.bsphere.x;
  float y = header.bsphere.y;
  float z = header.bsphere.z;
  memcpy(mat_model, (float[16]){
    scale,    0.0f,     0.0f,    0.0f,
    0.0f,     scale,    0.0f,    0.0f,
    0.0f,     0.0f,     scale,   0.0f,
   -x*scale, -y*scale, -z*scale, 1.0f
  }, 16*sizeof(float));
  memcpy(mat_view, (float[16]){
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  }, 16*sizeof(float));
 
  while (isrunning) {
    update_scene();
    glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
    glBindVertexArray(vao_model);
    
    switch (shademode) {
      case 1:
        glUseProgram(program_texcoord);
        break;
      default:
        glUseProgram(program_default);
        break;
    }
    for (int i=0; i < header.num_meshes; i++) {
      bsm_mesh_t mesh = meshes[i];
      glDrawElements(GL_TRIANGLES, mesh.num_tris * 3, GL_UNSIGNED_INT, (void*)(mesh.idx_tris * sizeof(bsm_triangle_t)));
    }
    
    if (drawnorms) {
      glUseProgram(program_normals);
      glDrawArrays(GL_POINTS, 0, header.num_verts);
    }
    
    glfwSwapBuffers();
  }
  
  return 0;
}