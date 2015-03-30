#version 150

uniform sampler2D mTexRgb;
uniform vec3 ViewDirection;

in vec2 UV;
in vec3 Normal;
in vec4 WorldPos;
out vec4 oColor;

void main()
{
	vec3 nView = normalize(ViewDirection-vec3(WorldPos.xyz));
	vec3 nNorm = normalize(Normal);
	float cDiff = dot(nNorm, nView);
	cDiff = cDiff*0.5+0.5;
	vec2 cUV = vec2(UV.x,1.0-UV.y);
	oColor = texture2D(mTexRgb, cUV)*cDiff;
}