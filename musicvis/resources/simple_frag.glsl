#version 330 core
in vec3 fragNor;
out vec4 color;
in vec3 pos;
in vec2 fragTex;


uniform sampler2D tex;
//uniform sampler2D tex2;

uniform vec3 camPos;


void main()
{
	vec3 normal = normalize(fragNor);
	
	vec3 lp = vec3(100,100,100);
	vec3 ld = normalize(lp - pos);
	float light = dot(ld,normal);
    
    vec3 camdir = camPos - pos;
    camdir = normalize(camdir);
    
    vec3 h = normalize(ld + camdir);
    
    float spec = dot(normal,h);
    if(spec<0)
        spec=0;
    
    spec = pow(spec, 100);

    color = spec + texture(tex, fragTex)*light;

	color.a=1;
	 
}
