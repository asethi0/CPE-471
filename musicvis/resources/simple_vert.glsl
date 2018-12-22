#version 330 core
layout(location = 0) in vec4 vertPos;
layout(location = 1) in vec3 vertNor;
layout(location = 2) in vec2 vertTex;
uniform mat4 P;
uniform mat4 MV;
out vec3 fragNor;
out vec2 fragTex;
out vec3 pos;

void main()
{
pos=vertPos.xyz;
gl_Position = P * MV * vertPos;
fragNor = (MV * vec4(vertNor, 0.0)).xyz;
fragTex = vertTex;
fragTex.y*=-1;
}
