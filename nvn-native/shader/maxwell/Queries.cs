using System.Text.Json;
// Explicit overrides of the vendored interface; new queries require implementation.
using Ryujinx.Graphics.Shader;
sealed partial class StrictAccessor
{
    public int CreateExtraSet() => Query<int>(nameof(CreateExtraSet));
    public uint QueryConstantBufferUse() => Query<uint>(nameof(QueryConstantBufferUse));
    public bool QueryHasConstantBufferDrawParameters() => Query<bool>(nameof(QueryHasConstantBufferDrawParameters));
    public bool QueryHasUnalignedStorageBuffer() => Query<bool>(nameof(QueryHasUnalignedStorageBuffer));
    public int QueryHostGatherBiasPrecision() => Query<int>(nameof(QueryHostGatherBiasPrecision));
    public bool QueryHostReducedPrecision() => Query<bool>(nameof(QueryHostReducedPrecision));
    public bool QueryHostHasFrontFacingBug() => Query<bool>(nameof(QueryHostHasFrontFacingBug));
    public bool QueryHostHasVectorIndexingBug() => Query<bool>(nameof(QueryHostHasVectorIndexingBug));
    public int QueryHostStorageBufferOffsetAlignment() => Query<int>(nameof(QueryHostStorageBufferOffsetAlignment));
    public bool QueryHostFragmentQuadTransfer() => Query<bool>(nameof(QueryHostFragmentQuadTransfer));
    public bool QueryHostFragmentQuadReadonlySources() => Query<bool>(nameof(QueryHostFragmentQuadReadonlySources));
    public int QueryHostSubgroupSize() => Query<int>(nameof(QueryHostSubgroupSize));
    public bool QueryHostSupportsBgraFormat() => Query<bool>(nameof(QueryHostSupportsBgraFormat));
    public bool QueryHostSupportsFragmentShaderInterlock() => Query<bool>(nameof(QueryHostSupportsFragmentShaderInterlock));
    public bool QueryHostSupportsFragmentShaderOrderingIntel() => Query<bool>(nameof(QueryHostSupportsFragmentShaderOrderingIntel));
    public bool QueryHostSupportsGeometryShader() => Query<bool>(nameof(QueryHostSupportsGeometryShader));
    public bool QueryHostSupportsGeometryShaderPassthrough() => Query<bool>(nameof(QueryHostSupportsGeometryShaderPassthrough));
    public bool QueryHostSupportsImageLoadFormatted() => Query<bool>(nameof(QueryHostSupportsImageLoadFormatted));
    public bool QueryHostSupportsLayerVertexTessellation() => Query<bool>(nameof(QueryHostSupportsLayerVertexTessellation));
    public bool QueryHostSupportsNonConstantTextureOffset() => Query<bool>(nameof(QueryHostSupportsNonConstantTextureOffset));
    public bool QueryHostSupportsScaledVertexFormats() => Query<bool>(nameof(QueryHostSupportsScaledVertexFormats));
    public bool QueryHostSupportsSeparateSampler() => Query<bool>(nameof(QueryHostSupportsSeparateSampler));
    public bool QueryHostSupportsShaderBallot() => Query<bool>(nameof(QueryHostSupportsShaderBallot));
    public bool QueryHostSupportsShaderBarrierDivergence() => Query<bool>(nameof(QueryHostSupportsShaderBarrierDivergence));
    public bool QueryHostSupportsShaderFloat64() => Query<bool>(nameof(QueryHostSupportsShaderFloat64));
    public bool QueryHostSupportsShaderNonUniformIndexing() => Query<bool>(nameof(QueryHostSupportsShaderNonUniformIndexing));
    public bool QueryHostSupportsSnormBufferTextureFormat() => Query<bool>(nameof(QueryHostSupportsSnormBufferTextureFormat));
    public bool QueryHostSupportsTextureGatherOffsets() => Query<bool>(nameof(QueryHostSupportsTextureGatherOffsets));
    public bool QueryHostSupportsTextureShadowLod() => Query<bool>(nameof(QueryHostSupportsTextureShadowLod));
    public bool QueryHostSupportsTransformFeedback() => Query<bool>(nameof(QueryHostSupportsTransformFeedback));
    public bool QueryHostSupportsViewportIndexVertexTessellation() => Query<bool>(nameof(QueryHostSupportsViewportIndexVertexTessellation));
    public bool QueryHostSupportsViewportMask() => Query<bool>(nameof(QueryHostSupportsViewportMask));
    public bool QueryHostSupportsDepthClipControl() => Query<bool>(nameof(QueryHostSupportsDepthClipControl));
    public int QuerySamplerArrayLengthFromPool() => Query<int>(nameof(QuerySamplerArrayLengthFromPool));
    public SamplerType QuerySamplerType(int handle, int cbufSlot = -1)
    {
        var type = TextureType(handle, cbufSlot);
        Queries.Add(new { name = nameof(QuerySamplerType), handle, cbufSlot, value = type }); return type;
    }
    public int QueryTextureArrayLengthFromBuffer(int slot) => throw new System.NotSupportedException("unqualified query: QueryTextureArrayLengthFromBuffer");
    public int QueryTextureArrayLengthFromPool() => Query<int>(nameof(QueryTextureArrayLengthFromPool));
    public bool QueryTextureCoordNormalized(int handle, int cbufSlot = -1)
    {
        var texture = Texture(handle, cbufSlot);
        if (!texture.TryGetProperty("coord_normalized", out var normalized))
            throw new System.IO.InvalidDataException($"missing explicit texture query: QueryTextureCoordNormalized, cbuf={cbufSlot}, handle={handle}");
        bool value = normalized.GetBoolean();
        if (!value) throw new System.NotSupportedException("unnormalized sampled coordinates are not qualified");
        Queries.Add(new { name = nameof(QueryTextureCoordNormalized), handle, cbufSlot, value }); return value;
    }
    public TextureFormat QueryTextureFormat(int handle, int cbufSlot = -1)
    {
        if (TextureType(handle, cbufSlot) != SamplerType.TextureBuffer)
            throw new System.NotSupportedException("texture format query is qualified only for typed scalar buffers");
        var texture = Texture(handle, cbufSlot);
        if (!texture.TryGetProperty("texture_format", out var value))
            throw new System.IO.InvalidDataException($"missing explicit texture query: QueryTextureFormat, cbuf={cbufSlot}, handle={handle}");
        // This is the vendored shader enum namespace, never NVN, FDL or a host capability.
        string? name = value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        if (name is null || !Enum.TryParse(name, out TextureFormat format) ||
            format == TextureFormat.Unknown || (uint)format > (uint)TextureFormat.R11G11B10Float ||
            Enum.GetName(format) != name)
            throw new NotSupportedException("explicit canonical shader TextureFormat name (1..39) required for typed buffer query");
        Queries.Add(new { name = nameof(QueryTextureFormat), handle, cbufSlot, value = (int)format });
        return format;
    }
    public bool QueryTransformFeedbackEnabled() => Query<bool>(nameof(QueryTransformFeedbackEnabled));
    public ReadOnlySpan<byte> QueryTransformFeedbackVaryingLocations(int bufferIndex) => throw new System.NotSupportedException("unqualified query: QueryTransformFeedbackVaryingLocations");
    public int QueryTransformFeedbackStride(int bufferIndex) => throw new System.NotSupportedException("unqualified query: QueryTransformFeedbackStride");
    public void RegisterTexture(int handle, int cbufSlot)
    {
        TextureType(handle, cbufSlot);
        Queries.Add(new { name = nameof(RegisterTexture), handle, cbufSlot, domain = "combined texture/sampler resource identity" });
    }
}
