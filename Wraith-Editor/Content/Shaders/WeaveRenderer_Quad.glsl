// ############################
// # Wraith 2D                #
// # Weave Quad Shader        #
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
uniform float u_CornerRadius;
uniform vec2 u_Size;
            
void main() {
	vec2 uv = v_TexCoord;
	vec2 pos = uv * u_Size;
                
	// Calculate distance to nearest corner
	vec2 cornerPos = max(abs(pos - u_Size * 0.5) - (u_Size * 0.5 - u_CornerRadius), 0.0);
	float cornerDist = length(cornerPos) - u_CornerRadius;
                
	float alpha = 1.0 - smoothstep(-1.0, 1.0, cornerDist);
	color = vec4(u_Color.rgb, u_Color.a * alpha);
}
