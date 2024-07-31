class PRTMaterial extends Material {

    constructor(precomputeL, vertexShader, fragmentShader) {
        super({
            'uPrecomputeL[0]': { type: 'precomputL', value: null },
            'uPrecomputeL[1]': { type: 'precomputL', value: null },
            'uPrecomputeL[2]': { type: 'precomputL', value: null },
        }, ['aPrecomputeLT'], vertexShader, fragmentShader, null);
    }
}

async function buildPRTMaterial(precomputeL, vertexPath, fragmentPath) {
    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);
    return new PRTMaterial(precomputeL, vertexShader, fragmentShader);
}