#ifndef __TEMPORAL_AA_H__
#define	__TEMPORAL_AA_H__

/// LinearizeDepthCS.hlsl --> LinearDepthBuffer
/// CameraVelocityCS.hlsl --> VelocityBuffer
/// TemporalBlendCS.hlsl --> TemporalColorBuffer

class TemporalAA
{
public:
	static void Process();
};

#endif // !__TEMPORAL_AA_H__
