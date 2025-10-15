// ############################
// # Wraith 2D                #
// # Weave Circle Shader      #
// ############################

#type vertex
#version 450 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
            
uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
            
out vec2 v_TexCoord;
            
void main() {
	v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 color;
            
in vec2 v_TexCoord;
            
uniform vec4 u_Color;
            
void main() {
	vec2 uv = v_TexCoord * 2.0 - 1.0;
	float dist = length(uv);
	float alpha = 1.0 - smoothstep(0.9, 1.0, dist);
	color = vec4(u_Color.rgb, u_Color.a * alpha);
}
