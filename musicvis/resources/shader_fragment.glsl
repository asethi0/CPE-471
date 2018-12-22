#version 330 core
out vec4 color;
uniform vec3 colorext;
void main()
{
color.rgb = colorext;
color.a=1;
}
