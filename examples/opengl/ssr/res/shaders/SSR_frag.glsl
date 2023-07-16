#version 330 core

layout(location = 0) out vec2 ssr_uv;
layout(location = 1) out vec2 ssr_depth;

uniform vec3      iResolution; // viewport resolution (in pixels)
uniform float     iNearPlane;
uniform float     iFarPlane;
uniform float     iCamTanHalfFov;
uniform float     iCamTanHalfFovTimesAspect;

uniform sampler2D iNormals;
uniform sampler2D iDepth;   // ndc.
uniform sampler2D iPos;     // in view space.

void main()
{
    // NOTE: this is in units of fragment.
    float coarseRayStep = 0.1; // in view space.
    int maxRaySteps = 64;

    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // sample normal.
    vec3 N = normalize(texture(iNormals, uv).xyz);
    
    // TODO: can we not reconstruct the position from the depth and inverse camera matrix?
    vec3 vPos = texture(iPos, uv).xyz;

    // determine ray dir.
    vec3 viewDir = normalize(vPos);
	vec3 rayDir = reflect(viewDir, N);

    float f = iFarPlane;
    float n = iNearPlane;

    // TODO: reject rays where the view is coming from
    // underneath the surface.

    // coarse raymarch pass.
    vec2 currUV = uv;
    vec3 rayPos = vPos; // in view.
    int didHit = 0;
    for (int i = 0; (i < maxRaySteps) && (dot(viewDir, N) < 0); i += 1)
    {
        // iter the ray.
        rayPos += rayDir * coarseRayStep;
        
        // r = tanf(cam.fov * DEGREES_TO_RADIANS / 2.0f) * n
        // x_ndc = x / tanf(cam.fov * DEGREES_TO_RADIANS / 2.0f) * (1 / -z.
        // x_uv = x_ndc /2 + 0.5        
        currUV = rayPos.xy / (-rayPos.z * vec2(iCamTanHalfFov, iCamTanHalfFovTimesAspect) );
        currUV = currUV / 2.0 + 0.5;

        if (currUV.x < 0.0 || currUV.x > 1.0 || currUV.y < 0.0 || currUV.y > 1.0 ||
            rayPos.z > -n || rayPos.z < -f
        )
        {
            // out of bounds, no hit.
            break;
        }

        // sample depth.
        float depthNDC = texture(iDepth, currUV).r;

        // NOTE: the depth buffer stores values from [0, 1],
        // but the idea of projection in opengl is that we map the z value to [-1, 1].
        // thus, before we do the inverse projection transform, we need to get back to
        // -1,-1.
        depthNDC = depthNDC * 2.0 - 1.0;
        
        // z_ndc = (f + n) / (f -n) + 2 * f * n / (f - n) / z
        // rearrange the above and we get:

        float depthView = 2.0 * f * n / (depthNDC * (f-n) - (f+n));

        // NOTE: depth in the view space from the camera goes more negative when we get farther away from the camera.
        // therefore we want to find that the ray is less, which makes it a larger negative.
        if ( (rayPos.z < depthView) && ((depthView - rayPos.z) < coarseRayStep * 0.5 ) )
        {
            // hit something.
            didHit = 1;
            break;
        }
    } // @ loop end, rayPos is as far as the ray went before we maybe found a hit.

    if (didHit == 1)
    {
        ssr_uv = currUV;

        // TODO.
        ssr_depth = vec2((-rayPos.z-n)/(f), 0.0);
    }
    else
    {
        ssr_uv = uv;
        ssr_depth = vec2(0.0, (-rayPos.z-n)/(f) );
    }

    /*float depthNDC = texture(iDepth, uv).r;
        
        // z_ndc = (f + n) / (f -n) - 2 * f * n / (f -n) / -z
        // rearrange the above and we get:
        float f = iFarPlane;
        float n = iNearPlane;
        float depthView = -2 * f * n / (depthNDC * (f-n) - (f+n));
        */

    // TODO: fine raymarch pass to get actual intersection.

    /*uint whatToRender = 2;

    if (whatToRender == 0)
    {
        //color = texture(iNormals, uv);
    } else if (whatToRender == 1)
    {
        //color = vec4(vec3(didHit, 0, 0), 1.0);
    } else if (whatToRender == 2)
    {
        float depthNDC = texture(iDepth, uv).r; // between [0,1]

        depthNDC = depthNDC * 2 - 1;

        // z_ndc = (f + n) / (f -n) + 2 * f * n / (f - n) / z
        // rearrange the above and we get:
        float f = iFarPlane;
        float n = iNearPlane;
        float depthView = 2.0 * f * n / (depthNDC * (f-n) - (f+n));

        float depthColor = (-depthView-n)/(f-n);
        ssr_uv = vec2( depthColor, depthColor);
    }*/
};
