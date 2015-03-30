#version 150

uniform mat4 ciModelViewProjection;
uniform mat3 ciNormalMatrix;
uniform mat4 ciModelMatrix;

in vec4 ciPosition;
in vec3 ciNormal;

in vec3 iPosition;
in vec2 iUV;
in float iSize;

out vec2 UV;
out vec3 Normal;
out vec4 WorldPosition;

void main()
{
	vec4 mPos = ciPosition;
	mPos.x*=iSize;
	mPos.y*=iSize;
	mPos.z*=iSize;

	UV = iUV;
	Normal = ciNormal;
	WorldPosition = ciModelMatrix*ciPosition;
	vec4 OutPos = ciModelViewProjection*(mPos+vec4(iPosition,0));
	gl_Position = ciModelViewProjection*(mPos+vec4(iPosition,0));
}
