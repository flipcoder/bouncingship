#version 120

varying vec3 Position;
uniform float Brightness;
uniform vec4 MaterialDiffuse;

void main()
{
    /*gl_FragColor = vec4(MaterialDiffuse.xyz,1.0);*/
    gl_FragColor = vec4(mix(MaterialDiffuse, vec4(0.0, 0.0, 0.0, 1.0), abs(Position.z/100.0)).xyz, 1.0);
}

