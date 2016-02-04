#version 120

/*varying vec3 VertexPosition;*/
varying vec3 Position;
varying vec2 Wrap;
/*varying vec2 Normal;*/

uniform sampler2D Texture;
uniform vec3 Brightness;

void main()
{
    vec4 color = texture2D(Texture, Wrap);
    /*float e = 0.1; // threshold*/
    /*if(floatcmp(color.r, 1.0, e) &&*/
    /*    floatcmp(color.g, 0.0, e) &&*/
    /*    floatcmp(color.b, 1.0, e))*/
    /*{*/
    /*    discard;*/
    /*}*/
    /*if(floatcmp(color.a, 0.0, e)) {*/
    /*    discard;*/
    /*}*/
    
    gl_FragColor = color * vec4(Brightness, 1.0);
    /*gl_FragColor = color;*/
}

