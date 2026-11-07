#version 450
layout(set=1,binding=0,std140) uniform View { vec4 fit; };
layout(location=0) out vec2 uv;
void main() {
    const vec2 p[6]=vec2[6](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(-1,1),vec2(1,-1),vec2(1,1));
    // SDL GPU clip +Y is up; texture (0,0) is the top-left.
    vec2 xy=p[gl_VertexIndex];uv=vec2((xy.x+1.0)*0.5,(1.0-xy.y)*0.5);
    gl_Position=vec4(xy*fit.xy,0,1);
}
