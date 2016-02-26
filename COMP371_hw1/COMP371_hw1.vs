#version 130

uniform mat4 view_matrix, model_matrix, proj_matrix;
uniform int is_triangle;

in  vec3 in_Position;		//vertex position
out vec3 out_Color;

void main () {
	mat4 CTM = proj_matrix * view_matrix * model_matrix;
	gl_Position = CTM * vec4 (in_Position, 1.0);

	out_Color = vec3 (0.2,0.8,0.2);

	//if(in_Position.y >= -0.1 && in_Position.y <= 0)
	if(is_triangle == 1)
	{
		out_Color = vec3 (in_Position.y * 10,in_Position.y * 10,1+(in_Position.y * 10));
	}
}