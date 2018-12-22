#version 330 core 
in vec3 fragNor;
out vec4 color;
in vec3 pos;
in vec2 fragTex;


uniform sampler2D tex;

void main()
{
	
	color = texture(tex, fragTex); 
	color.a=1;
	 
}
