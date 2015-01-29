#include <stdlib.h>
#include <ecl/ecl.h>
#include "GL/gl3w.h"
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

// A macro to create a DEFUN abstraction in C++
// Credit: https://gist.github.com/vwood/662109
#define DEFUN(name,fun,args) \
	cl_def_c_function(c_string_to_object(name), \
	(cl_objectfn_fixed)fun, \
	args)

#define CHUNK_WIDTH 16
#define CHUNK_HEIGHT 16
#define CHUNK_DEPTH 16

#define CHUNK_TYPE_CHUNK 0

struct chunk
{
	uint8_t* blocks;
	GLuint vbo;
	int8_t x, y, z, type;
};

chunk* create_chunk(int8_t x, int8_t y, int8_t z, int8_t type)
{
	size_t chunkMemSize = sizeof(chunk) + CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;
	
	chunk* c = (chunk*)malloc(chunkMemSize);
	memset(c, 1, chunkMemSize);

	c->x = x;
	c->y = y;
	c->z = z;
	c->type = type;
	c->blocks = (uint8_t*)(c + 1);
	glGenBuffers(1, &(c->vbo));

	return c;
}

void free_chunk(chunk* c)
{
	if (c)
	{
		glDeleteBuffers(1, &(c->vbo));
		free(c);
		c = NULL;
	}
}

struct mouselook_camera_controller
{
	glm::vec3 position;
	float yaw, pitch, roll;

	glm::vec2 lastMousePos;
	bool up, down, left, right, forward, backwards;
};

void update_camera(mouselook_camera_controller* camera)
{
	glm::vec3 direction(0.0f);
	if (camera->up)
		direction.y += 1.0f;
	if (camera->down)
		direction.y -= 1.0f;
	if (camera->left)
		direction.x -= 1.0f;
	if (camera->right)
		direction.x += 1.0f;
	if (camera->forward)
		direction.z -= 1.0f;
	if (camera->backwards)
		direction.z += 1.0f;

	glm::quat yawq = glm::angleAxis(camera->yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat pitchq = glm::angleAxis(camera->pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rollq = glm::angleAxis(camera->roll, glm::vec3(0.0f, 0.0f, 1.0f));

	glm::mat3 rot = glm::mat3_cast(yawq * pitchq * rollq);

	if (glm::length(direction) > 0)
		camera->position += glm::normalize(rot * direction) * 0.05f;
}

glm::vec3 camera_look_dir(mouselook_camera_controller* camera)
{
	glm::quat yawq = glm::angleAxis(camera->yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat pitchq = glm::angleAxis(camera->pitch, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat rollq = glm::angleAxis(camera->roll, glm::vec3(0.0f, 0.0f, 1.0f));

	glm::mat3 rot = glm::mat3_cast(yawq * pitchq * rollq);
	return rot * glm::vec3(0.0f, 0.0f, 1.0f);
}

GLFWwindow* window;
glm::vec3 position = glm::vec3(-0.5f, 0.0f, -0.5f);
float angle = 0.0f;
glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
mouselook_camera_controller camera = {};
FT_Library ft;
FT_Face face;
int windowWidth = 1024, windowHeight = 768;
bool terminalMode = false;
char* term_buffer = NULL;
size_t termBufferSize = 4096;
size_t termBufferIndex = 0;

cl_object proc_quit()
{
	if (window)
		glfwSetWindowShouldClose(window, GL_TRUE);
	return Cnil;
}

cl_object proc_set_clear_color(cl_object r, cl_object g, cl_object b)
{
	glClearColor(ecl_to_float(r), ecl_to_float(g), ecl_to_float(b), 1.0f);
	return Cnil;
}

cl_object proc_set_color(cl_object r, cl_object g, cl_object b, cl_object a)
{
	color.r = ecl_to_float(r);
	color.g = ecl_to_float(g);
	color.b = ecl_to_float(b);
	color.a = ecl_to_float(a);
	return Cnil;
}

cl_object proc_translate(cl_object x, cl_object y, cl_object z)
{
	position += glm::vec3(
			ecl_to_float(x),
			ecl_to_float(y),
			ecl_to_float(z));
	return Cnil;
}

GLuint compile_shader(GLenum type, const char* code)
{
	GLuint handle = glCreateShader(type);
	glShaderSource(handle, 1, &code, nullptr);
	glCompileShader(handle);
	GLint status;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint infoLogLength;
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &infoLogLength);
		char* errors = (char*)calloc(infoLogLength + 1, 1);
		glGetShaderInfoLog(handle, infoLogLength, nullptr, errors);
		printf("\n%s\n%s\n", (type == GL_VERTEX_SHADER ? "Vertex Shader" : "Fragment Shader"), errors);
		free(errors);
	}
	return handle;
}

GLuint create_basic_shader(const char* vertexCode, const char* fragmentCode)
{
	GLuint program, vertShader, fragShader;
	vertShader = compile_shader(GL_VERTEX_SHADER, vertexCode);
	fragShader = compile_shader(GL_FRAGMENT_SHADER, fragmentCode);
	program = glCreateProgram();
	glAttachShader(program, vertShader);
	glAttachShader(program, fragShader);
	glLinkProgram(program);
	glDeleteShader(vertShader);
	glDeleteShader(fragShader);
	return program;
}

GLint get_shader_location(GLuint program, const char* name)
{
	GLint loc = glGetUniformLocation(program, name);
	return (loc != -1) ? loc : glGetAttribLocation(program, name);
}

glm::vec3* compute_vertex_normals(const float* verts, const unsigned short* indices)
{
	int numVerts = sizeof(verts) / sizeof(verts[0]) / 3;

	glm::vec3* normals = new glm::vec3[numVerts];
	for (int i = 0; i < numVerts; ++i)
		normals[i] = glm::vec3(0.0f);

	int numTriangles = sizeof(indices) / sizeof(indices[0]) / 3;
	for (int i = 0; i < numTriangles; ++i)
	{
		glm::vec3 v1(verts[indices[i] * 3],     verts[indices[i] * 3 + 1],     verts[indices[i] * 3 + 2]);
		glm::vec3 v2(verts[indices[i + 1] * 3], verts[indices[i + 1] * 3 + 1], verts[indices[i + 1] * 3 + 2]);
		glm::vec3 v3(verts[indices[i + 2] * 3], verts[indices[i + 2] * 3 + 1], verts[indices[i + 2] * 3 + 2]);
	
		glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
		normals[indices[i]] += normal;
		normals[indices[i + 1]] += normal;
		normals[indices[i + 2]] += normal;
	}

	for (int i = 0; i < numVerts; ++i)
		normals[i] = glm::normalize(normals[i]);

	return normals;
}

void handle_keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (!terminalMode)
	{
		switch (key)
		{
			case GLFW_KEY_W:
			{
				camera.forward = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
			break;
			case GLFW_KEY_A:
			{
				camera.left = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
			break;
			case GLFW_KEY_S:
			{
				camera.backwards = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
			break;
			case GLFW_KEY_D:
			{
				camera.right = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
			break;
			case GLFW_KEY_Q:
			{
				camera.down = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
			break;
			case GLFW_KEY_E:
			{
				camera.up = (action == GLFW_PRESS || action == GLFW_REPEAT);
			}
		}
	}
	else if (terminalMode)
	{
		switch (key)
		{
			case GLFW_KEY_BACKSPACE:
			{
				if (termBufferIndex > 0)
				{
					termBufferIndex--;
					term_buffer[termBufferIndex] = 0;
				}
			}
			break;
		}
	}
}

void handle_mouse_move(GLFWwindow* window, double x, double y)
{
	if (!terminalMode)
	{
		glm::vec2 mousePos((float)x, (float)y);
		glm::vec2 delta = (mousePos - camera.lastMousePos) * 0.004f;

		camera.lastMousePos = mousePos;

		camera.pitch -= delta.y;
		camera.yaw -= delta.x;

		camera.pitch = glm::clamp(camera.pitch, -glm::pi<float>()/2.0f, glm::pi<float>()/2.0f);
	}
}

void window_focus_callback(GLFWwindow* window, int focused)
{
	if (focused && !terminalMode)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}

void handle_text_entry(GLFWwindow* window, uint32_t codepoint)
{
	if (terminalMode)
	{
		fprintf(stdout, "%d\n", codepoint);

		if (termBufferIndex < termBufferSize)
		{
			term_buffer[termBufferIndex] = (codepoint > 127) ? 0 : (char)codepoint;
			termBufferIndex++;
		}
	}
}

void render_text(const char* buffer, uint8_t* texture, FT_Face* font)
{
	memset(texture, 0, windowWidth * windowHeight);
	int x = 0, y = 50;

	for (const char* c = buffer; *c != 0; ++c)
	{
		if (y >= windowHeight)
			return;

       	size_t glyph_index = FT_Get_Char_Index(face, *c);
       	if (FT_Load_Glyph(face, glyph_index, 0))
       	{
       		fprintf(stderr, "Failed to load glyph for %d\n", *c);
       		continue;
       	}

       	if (FT_Render_Glyph(face->glyph, ft_render_mode_normal))
       	{
       		fprintf(stderr, "Failed to render glyph for %d\n", *c);
       		continue;
       	}

		FT_GlyphSlot g = face->glyph;
       	
       	if (*c == '\n')
       	{
       		y += (g->advance.y >> 6);
       		x = 0;
       		continue;
       	}

		if (x + g->bitmap_left + g->bitmap.width >= windowWidth)
		{
			y += (g->advance.y >> 6);
       		x = 0;
		}

		for (size_t row = 0; row < g->bitmap.rows; ++row)
		{
			memcpy(
				texture + (y + row - g->bitmap_top) * windowWidth + x, //- g->bitmap_left,
				g->bitmap.buffer + row * g->bitmap.width,
				g->bitmap.width);
		}

		x += (g->advance.x >> 6);
		y += (g->advance.y >> 6);
	}
}

void shutdown()
{
	glfwTerminate();
	cl_shutdown();
}

int main(int argc, char* argv[])
{
	cl_boot(argc, argv);
	atexit(shutdown);

	

	term_buffer = (char*)calloc(termBufferSize, 1);

	DEFUN("quit", proc_quit, 0);
	DEFUN("set-clear-color", proc_set_clear_color, 3);
	DEFUN("translate", proc_translate, 3);
	DEFUN("set-color", proc_set_color, 4);

	if (!glfwInit())
	{
		printf("Failed to init glfw.");
		exit(EXIT_FAILURE);
	}

	gl3wInit();

	window = glfwCreateWindow(windowWidth, windowHeight, "My Title", NULL, NULL);
	glfwMakeContextCurrent(window);

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	cl_object read_obj = c_string_to_object("(read)");
	cl_object result = Cnil;
	
	const char* vertShaderCode =
	{
		"#version 330\n"
		"layout (location = 0) in vec3 vertex;\n"
		"layout (location = 1) in vec3 normal;\n"

		"uniform mat4 transform;\n"
		"uniform mat4 modelView;\n"
		"uniform mat4 view;\n"
		"uniform vec3 light_pos;\n"

		"out vec3 position_world;\n"
		"out vec3 surface_normal;\n"
		"out vec3 light_direction;\n"
		"out vec3 eye_dir_camera;\n"

		"void main() {\n"
			"gl_Position = modelView * transform * vec4(vertex, 1.0);\n"

			"position_world = (transform * vec4(vertex, 1.0)).xyz;\n"
			
			"vec3 vertex_pos_camera = (view * transform * vec4(vertex, 1.0)).xyz;\n"
			"eye_dir_camera = vec3(0, 0, 0) - vertex_pos_camera;\n"

			"vec3 light_pos_camera = (view * vec4(light_pos, 1.0)).xyz;\n"
			"light_direction = light_pos_camera + eye_dir_camera;\n"
			
			"surface_normal = (view * transform * vec4(normal, 1.0)).xyz;\n"
		"}"
	};

	const char* fragShaderCode =
	{
		"#version 330\n"
		"in vec3 position_world;\n"
		"in vec3 surface_normal;\n"
		"in vec3 light_direction;\n"
		"in vec3 eye_dir_camera;\n"

		"uniform vec4 color;\n"
		"uniform vec3 light_pos;\n"

		"out vec4 final_color;\n"

		"void main() {\n"
			"float light_power = 50.0f;\n"

			"vec3 mat_diffuse_color = color.rgb;\n"
			"vec3 mat_ambient_color = mat_diffuse_color * 0.1;\n"
			"vec3 mat_specular_color = vec3(0.3, 0.3, 0.3);\n"

			"float distance = length(light_pos - position_world);\n"
			"float dist_sqrd = distance * distance;"

			"vec3 n = normalize(surface_normal);\n"
			"vec3 l = normalize(light_direction);\n"

			"float cos_theta = clamp(dot(n, l), 0, 1);\n"

			"vec3 E = normalize(eye_dir_camera);\n"
			"vec3 R = reflect(-l, n);\n"
			
			"float cos_alpha = clamp(dot(E, R), 0, 1);\n"

			"vec3 light_color = vec3(1.0, 1.0, 1.0);\n"

			"final_color = color * vec4(mat_ambient_color\n"
				"+ mat_diffuse_color * light_color * light_power * cos_theta / dist_sqrd\n"
				"+ mat_specular_color * light_color * light_power * pow(cos_alpha, 5) / dist_sqrd,\n"
				"1.0);\n"
		"}"
	};

	GLuint program = create_basic_shader(vertShaderCode, fragShaderCode);
	glUseProgram(program);

	GLint vertLoc = get_shader_location(program, "vertex");
	GLint normalLoc = get_shader_location(program, "normal");

	GLint modelViewLoc = get_shader_location(program, "modelView");
	GLint viewLoc = get_shader_location(program, "view");
	GLint lightPosLoc = get_shader_location(program, "light_pos");
	
	glm::mat4 projection = glm::perspective(
		45.0f, (float)windowWidth / (float)windowHeight, 0.1f, 1000.0f);

	glm::vec3 lightPos(3.0f, 3.0f, 3.0f);
	glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));

	GLint transformLoc = get_shader_location(program, "transform");
	GLint colorLoc = get_shader_location(program, "color");

	GLuint vertexArray;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);

	const float verts[24] =
	{
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
	};

	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glEnableVertexAttribArray(vertLoc);
	glVertexAttribPointer(vertLoc, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, NULL);

	const unsigned short indices[36] =
	{
		0, 1, 2,
		0, 2, 3,
		1, 5, 6,
		1, 6, 2,
		5, 4, 7,
		5, 7, 6,
		4, 0, 3,
		4, 3, 7,
		0, 1, 5,
		0, 5, 4,
		3, 2, 6,
		3, 6, 7
	};

	GLuint indexBuffer;
	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glm::vec3* normals = compute_vertex_normals(verts, indices);

	GLuint normalBuffer;
	glGenBuffers(1, &normalBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);

	glEnableVertexAttribArray(normalLoc);
	glVertexAttribPointer(normalLoc, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, NULL);
	delete normals;

	const char* textVertShaderCode =
	{
		"#version 330\n"
		"out vec2 uv;\n"
		"void main() {\n"
			"const vec4 vertices[4] = vec4[4](\n"
				"vec4(-1.0, 1.0, 0.0, 1.0),\n"
				"vec4(1.0, 1.0, 0.0, 1.0),\n"
				"vec4(-1.0, -1.0, 0.0, 1.0),\n"
				"vec4(1.0, -1.0, 0.0, 1.0));\n"
			"gl_Position = vertices[gl_VertexID];\n"

			"const vec2 uv_verts[4] = vec2[4](\n"
				"vec2(0.0, 0.0),\n"
				"vec2(1.0, 0.0),\n"
				"vec2(0.0, 1.0),\n"
				"vec2(1.0, 1.0));\n"
			"uv = uv_verts[gl_VertexID];"
		"}"
	};

	const char* textFragShaderCode =
	{
		"#version 330\n"
		"in vec2 uv;\n"
		"uniform sampler2D tex;\n"
		"out vec4 final_color;\n"
		"void main() {\n"
			"vec4 texColor = texture(tex, uv);\n"
			"final_color = vec4(1, 1, 1, texColor.r);\n"
		"}"
	};

	GLuint textProgram = create_basic_shader(textVertShaderCode, textFragShaderCode);
	GLint texLoc = get_shader_location(textProgram, "tex");

	GLuint textPBO;
	glGenBuffers(1, &textPBO);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, textPBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, windowWidth * windowHeight, nullptr, GL_STREAM_DRAW);

	GLuint textTexture;
	glUniform1i(texLoc, 0);
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &textTexture);
	glBindTexture(GL_TEXTURE_2D, textTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, windowWidth, windowHeight);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_RED, GL_UNSIGNED_BYTE, NULL);

	glfwSetKeyCallback(window, handle_keyboard);
	glfwSetCharCallback(window, handle_text_entry);
	glfwSetCursorPosCallback(window, handle_mouse_move);
	glfwSetWindowFocusCallback(window, window_focus_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	
	camera.position = glm::vec3(0.0f, 0.0f, 3.0f);
	camera.up = camera.down = camera.left = camera.right = camera.forward = camera.backwards = false;

	if(FT_Init_FreeType(&ft)) 
	{
  		fprintf(stderr, "Could not init freetype library\n");
  		exit(EXIT_FAILURE);
	}

	if(FT_New_Face(ft, "/usr/share/fonts/truetype/freefont/FreeSans.ttf", 0, &face))
	{
  		fprintf(stderr, "Could not open font\n");
  		exit(EXIT_FAILURE);
	}
	FT_Set_Pixel_Sizes(face, 0, 20);

	while (window && !glfwWindowShouldClose(window))
	{
		update_camera(&camera);

		glm::mat4 view = glm::lookAt(
			camera.position,
			camera.position - camera_look_dir(&camera),
			glm::vec3(0.0f, 1.0f, 0.0f));
	
		glm::mat4 modelView = projection * view;

		glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(modelView));
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		// Render Here
		angle += 0.005;
		glm::mat4 transform = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
		transform = glm::translate(transform, position);
		glUniformMatrix4fv(transformLoc, 1 , GL_FALSE, glm::value_ptr(transform));
		glUniform4fv(colorLoc, 1, glm::value_ptr(color));

    	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    	glDrawElements(GL_TRIANGLES, sizeof(indices), GL_UNSIGNED_SHORT, NULL);

    	if (terminalMode)
    	{
    		glUseProgram(textProgram);

    		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, textPBO);
    		uint8_t* texture_buffer = (uint8_t*)glMapBufferRange(
    			GL_PIXEL_UNPACK_BUFFER,
    			0,
    			windowWidth * windowHeight,
    			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    		render_text(term_buffer, texture_buffer, &face);
    		//render_text("Tendril Terminal", texture_buffer, &face);
    		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_RED, GL_UNSIGNED_BYTE, NULL);
    		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    		glUseProgram(program);
    	}

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();

    	static int terminalModeButtonLastState = GLFW_RELEASE;
        int terminalModeButtonState = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);
        if (terminalModeButtonState == GLFW_RELEASE
        	&& terminalModeButtonLastState == GLFW_PRESS)
        {
        	if (!terminalMode)
        	{
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				terminalMode = true;
        	}
        	else
        	{
        		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				terminalMode = false;
        	}
        }
        
        terminalModeButtonLastState = terminalModeButtonState;
        

        if (terminalMode)
        {
        	static int enterLastState = GLFW_RELEASE;
        	int enterState = glfwGetKey(window, GLFW_KEY_ENTER);
        	if (enterState == GLFW_RELEASE
        		&& enterLastState == GLFW_PRESS)
        	{
        		fprintf(stdout, "%s\n", term_buffer);
        		cl_object buff = c_string_to_object(term_buffer);
        		result = cl_safe_eval(buff, Cnil, Cnil);
        		cl_print(1, result);
        		putchar('\n');
        		fprintf(stdout, "Clearing buffer\n");
        		memset(term_buffer, 0, termBufferSize);
        		termBufferIndex = 0;
        	}
        	enterLastState = enterState;
        }

	}

	FT_Done_FreeType(ft);

	free(term_buffer);
	glfwDestroyWindow(window);
	
	return 0;
}