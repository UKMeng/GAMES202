attribute vec3 aVertexPosition;
attribute vec3 aNormalPosition;
attribute mat3 aPrecomputeLT;

uniform mat4 uModelMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uProjectionMatrix;
//uniform vec3 uPrecomuteL[9];
uniform mat3 uPrecomputeL[3];

#define PI 3.141592653589793

varying highp vec3 vColor;



void main(void) {

    for (int i = 0; i < 3; i++) {
        vec3 lt0 = aPrecomputeLT[0];
        vec3 lt1 = aPrecomputeLT[1];
        vec3 lt2 = aPrecomputeLT[2];
        vec3 l0 = uPrecomputeL[i][0];
        vec3 l1 = uPrecomputeL[i][1];
        vec3 l2 = uPrecomputeL[i][2];
        vColor[i] = dot(lt0, l0) + dot(lt1, l1) + dot(lt2, l2);
    }

    vColor = 2.0 * vColor / PI; // rho / pi

    gl_Position = uProjectionMatrix * uViewMatrix * uModelMatrix * vec4(aVertexPosition, 1.0);
}