using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using Ryujinx.Graphics.Shader;
using Ryujinx.Graphics.Shader.Translation;
using Ryujinx.Common.Memory;

sealed partial class StrictAccessor : IGpuAccessor
{
    readonly byte[] code, c1;
    readonly JsonElement state;
    readonly Dictionary<string, SetBindingPair> bindings = new();
    int nextBinding = 1; // Binding zero is reserved by the translator support ABI.
    public readonly List<object> Queries = new();
    public StrictAccessor(byte[] code, byte[] c1, JsonElement state)
    {
        // Default interface methods otherwise silently survive upstream additions.
        var map = typeof(StrictAccessor).GetInterfaceMap(typeof(IGpuAccessor));
        foreach (var method in map.TargetMethods)
            if (method.DeclaringType != typeof(StrictAccessor))
                throw new NotSupportedException("accessor override missing: " + method.Name);
        this.code = code; this.c1 = c1; this.state = state;
    }
    T Query<T>(string name)
    {
        JsonElement value;
        if (!state.GetProperty("host").TryGetProperty(name, out value) &&
            !state.GetProperty("resources").GetProperty("queries").TryGetProperty(name, out value))
            throw new InvalidDataException($"missing explicit query: {name}");
        var result = value.Deserialize<T>()!;
        Queries.Add(new { name, value = result });
        return result;
    }
    int Compute(string key, int axis = -1)
    {
        var value = state.GetProperty("compute").GetProperty(key);
        int result = axis < 0 ? value.GetInt32() : value[axis].GetInt32();
        Queries.Add(new { name = key, axis, value = result }); return result;
    }
    public ReadOnlySpan<ulong> GetCode(ulong address, int minimumSize)
    {
        if (address > (ulong)code.Length || address % 8 != 0 || minimumSize < 0 ||
            (ulong)minimumSize > (ulong)code.Length - address) throw new InvalidDataException("code read exceeds fixture");
        return MemoryMarshal.Cast<byte, ulong>(code.AsSpan((int)address));
    }
    public uint ConstantBuffer1Read(int offset)
    {
        if (offset < 0 || offset > c1.Length - 4) throw new InvalidDataException("C1 read exceeds fixture");
        Queries.Add(new { name = "ConstantBuffer1Read", offset }); return BitConverter.ToUInt32(c1, offset);
    }
    SetBindingPair Binding(string kind, int index)
    {
        string key = $"{kind}/{index}";
        if (!bindings.TryGetValue(key, out var pair)) bindings[key] = pair = new(0, nextBinding++);
        Queries.Add(new { name = "binding", kind, index, pair.SetIndex, pair.Binding }); return pair;
    }
    public SetBindingPair CreateConstantBufferBinding(int index) => Binding("cbuf", index);
    public SetBindingPair CreateStorageBufferBinding(int index) => Binding("sbuf", index);
    public SetBindingPair CreateTextureBinding(int count, bool isBuffer)
    {
        if (count != 1) throw new NotSupportedException("only scalar typed texture bindings are qualified");
        // SPIR-V bindings share one namespace with buffers. Native texture and
        // sampler tables remain distinct domains, even for a combined binding.
        var pair = new SetBindingPair(0, nextBinding++);
        Queries.Add(new { name = nameof(CreateTextureBinding), count, isBuffer,
            pair.SetIndex, pair.Binding, domains = isBuffer ? new[] { "texture" } : new[] { "texture", "sampler" } });
        return pair;
    }
    public SetBindingPair CreateImageBinding(int count, bool isBuffer) => throw new NotSupportedException("image bindings not qualified");
    public int QueryComputeLocalSizeX() => Compute("local_size", 0);
    public int QueryComputeLocalSizeY() => Compute("local_size", 1);
    public int QueryComputeLocalSizeZ() => Compute("local_size", 2);
    public int QueryComputeSharedMemorySize() => Compute("shared_bytes");
    public int QueryComputeLocalMemorySize() => Compute("local_bytes");
    JsonElement Texture(int handle, int cbufSlot)
    {
        if (!state.GetProperty("resources").TryGetProperty("textures", out var textures))
            throw new InvalidDataException($"missing explicit texture resource: cbuf={cbufSlot}, handle={handle}");
        JsonElement? found = null;
        foreach (var texture in textures.EnumerateArray())
        {
            if (texture.GetProperty("handle").GetInt32() != handle || texture.GetProperty("cbuf_slot").GetInt32() != cbufSlot) continue;
            if (found.HasValue) throw new InvalidDataException("duplicate texture specialization key");
            found = texture;
        }
        if (!found.HasValue) throw new InvalidDataException($"missing explicit texture resource: cbuf={cbufSlot}, handle={handle}");
        return found.Value;
    }
    SamplerType TextureType(int handle, int cbufSlot)
    {
        var texture = Texture(handle, cbufSlot);
        if (!texture.TryGetProperty("sampler_type", out var type))
            throw new InvalidDataException($"missing explicit texture query: QuerySamplerType, cbuf={cbufSlot}, handle={handle}");
        return type.GetString() switch
        {
            "Texture1D" => SamplerType.Texture1D,
            "TextureBuffer" => SamplerType.TextureBuffer,
            "Texture2D" => SamplerType.Texture2D,
            _ => throw new NotSupportedException("only explicit scalar Texture1D, TextureBuffer or Texture2D specialization is qualified")
        };
    }
    public void ValidateTextureReflection(ShaderProgramInfo info)
    {
        if (info.Images.Count != 0) throw new NotSupportedException("image resources are not qualified");
        foreach (var texture in info.Textures)
        {
            // QuerySamplerType describes the actual guest resource. The existing
            // Maxwell translator deliberately lowers image1D to native image2D,
            // while BUFFER must remain Dim.Buffer (integer fetch without LOD).
            var sourceType = TextureType(texture.HandleIndex, texture.CbufSlot);
            var reflectedType = sourceType == SamplerType.Texture1D
                ? SamplerType.Texture2D : sourceType;
            if (texture.Type != reflectedType || texture.ArrayLength != 1 ||
                texture.Separate || texture.Format != TextureFormat.Unknown)
                throw new NotSupportedException("unsupported scalar typed floating texture reflection");
        }
    }
    public static object HandleSources(TextureDescriptor texture)
    {
        var (textureWord, samplerWord, kind) = TextureHandle.UnpackOffsets(texture.HandleIndex);
        // Keep the original encoded identity as well as decoded source fields.
        // No default texture cbuf slot or actual descriptor ID is inferred.
        if (texture.CbufSlot < 0 || kind is not (TextureHandleType.CombinedSampler or TextureHandleType.SeparateSamplerHandle))
            return new { kind = kind.ToString(), resolved = false, textureWord, samplerWord,
                reason = "requires explicit runtime handle-source mapping" };
        var (textureSlot, samplerSlot) = TextureHandle.UnpackSlots(texture.CbufSlot, 0);
        return new { kind = kind.ToString(), resolved = true,
            texture = new { cbuf_slot = textureSlot, word_offset = textureWord, source = "texture handle word" },
            sampler = new { cbuf_slot = kind == TextureHandleType.CombinedSampler ? textureSlot : samplerSlot,
                word_offset = kind == TextureHandleType.CombinedSampler ? textureWord : samplerWord,
                source = "packed sampler handle word" } };
    }
    T Graphics<T>(string key)
    {
        if (!state.GetProperty("graphics").TryGetProperty(key, out var value))
            throw new InvalidDataException("missing explicit graphics state: " + key);
        var result = value.Deserialize<T>()!;
        Queries.Add(new { name = "graphics/" + key, value = result }); return result;
    }
    public GpuGraphicsState QueryGraphicsState()
    {
        var attributes = Graphics<AttributeType[]>("attribute_types");
        var outputs = Graphics<AttributeType[]>("fragment_output_types");
        if (attributes.Length != 32 || outputs.Length != 8) throw new InvalidDataException("graphics type array size");
        Array32<AttributeType> a = default; Array8<AttributeType> o = default;
        for (int i = 0; i < attributes.Length; i++) a[i] = attributes[i];
        for (int i = 0; i < outputs.Length; i++) o[i] = outputs[i];
        return new GpuGraphicsState(Graphics<bool>("early_z_force"), Graphics<InputTopology>("topology"),
            Graphics<bool>("tess_cw"), Graphics<TessPatchType>("tess_patch_type"), Graphics<TessSpacing>("tess_spacing"),
            Graphics<bool>("alpha_to_coverage_enable"), Graphics<bool>("alpha_to_coverage_dither_enable"),
            Graphics<bool>("viewport_transform_disable"), Graphics<bool>("depth_mode"),
            Graphics<bool>("program_point_size_enable"), Graphics<float>("point_size"),
            Graphics<AlphaTestOp>("alpha_test_compare"), Graphics<float>("alpha_test_reference"), a,
            Graphics<bool>("has_constant_buffer_draw_parameters"), o, Graphics<bool>("dual_source_blend_enable"),
            Graphics<bool>("y_negate_enabled"), Graphics<bool>("origin_upper_left"), Graphics<bool>("halve_primitive_id"));
    }
    public void Log(string message) => throw new InvalidDataException("translator diagnostic: " + message);
}

static class Program
{
    static byte[] Blob(JsonElement fixture, string name, string directory)
    {
        var blob = fixture.GetProperty("blobs").GetProperty(name);
        int offset = blob.GetProperty("offset").GetInt32(), size = blob.GetProperty("size").GetInt32();
        if (offset < 0 || size < 0 || size > 16 * 1024 * 1024) throw new InvalidDataException("invalid blob range");
        using var file = File.OpenRead(Path.Combine(directory, blob.GetProperty("path").GetString()!));
        if (offset > file.Length || size > file.Length - offset) throw new InvalidDataException("blob exceeds file");
        file.Position = offset; byte[] bytes = new byte[size]; file.ReadExactly(bytes);
        if (!Convert.ToHexStringLower(SHA256.HashData(bytes)).Equals(blob.GetProperty("sha256").GetString()))
            throw new InvalidDataException("blob hash mismatch: " + name);
        return bytes;
    }
    static int Main(string[] args)
    {
        try
        {
            if (args.Length != 2) throw new ArgumentException("usage: Maxwell FIXTURE OUTPUT_PREFIX");
            string path = Path.GetFullPath(args[0]);
            using var document = JsonDocument.Parse(File.ReadAllText(path)); var fixture = document.RootElement;
            string stage = fixture.GetProperty("stage").GetString()!;
            if (fixture.GetProperty("schema").GetInt32() != 1 || stage is not ("cs" or "vs" or "fs"))
                throw new NotSupportedException("only schema 1 CS/VS/FS input is currently qualified");
            if (fixture.GetProperty("state").GetProperty("precision").GetProperty("mode").GetString() != "strict")
                throw new NotSupportedException("only explicit strict precision is qualified");
            string dir = Path.GetDirectoryName(path)!;
            byte[] code = Blob(fixture, "code", dir), c1 = Blob(fixture, "c1", dir);
            byte[] control = Blob(fixture, "control", dir);
            if (code.Length == 0 || code.Length % 8 != 0 || control.Length == 0)
                throw new InvalidDataException("empty control or unaligned/empty Maxwell code");
            if (stage == "cs")
            {
                var compute = fixture.GetProperty("state").GetProperty("compute");
                if (compute.GetProperty("local_size").GetArrayLength() != 3 ||
                    compute.GetProperty("local_size").EnumerateArray().Any(v => v.GetInt32() <= 0) ||
                    compute.GetProperty("shared_bytes").GetInt32() < 0 || compute.GetProperty("local_bytes").GetInt32() < 0)
                    throw new InvalidDataException("invalid compute dimensions or memory sizes");
            }
            else if (code.Length < 0x80 || BitConverter.ToUInt32(code, 0) != 0x12345678 ||
                     ((BitConverter.ToUInt32(code, 0x30) >> 10) & 15) != (stage == "vs" ? 1 : 5))
                throw new InvalidDataException("graphics container/SPH stage mismatch");
            var accessor = new StrictAccessor(code, c1, fixture.GetProperty("state"));
            // Select the emitter's SPIR-V descriptor conventions. This enum
            // does not instantiate a Vulkan runtime, device or shader module.
            var context = Translator.CreateContext(stage == "cs" ? 0ul : 0x30ul, accessor,
                new TranslationOptions(TargetLanguage.Spirv, TargetApi.Vulkan,
                    stage == "cs" ? TranslationFlags.Compute : TranslationFlags.None));
            var program = context.Translate();
            accessor.ValidateTextureReflection(program.Info);
            File.WriteAllBytes(args[1] + ".spv", program.BinaryCode);
            var metadata = new {
                schema = 1, scope = "strict Maxwell frontend; no GPU execution", stage,
                compiler_overlay = NvnCompilerProfile.Id,
                code_sha256 = Convert.ToHexStringLower(SHA256.HashData(code)),
                spv_sha256 = Convert.ToHexStringLower(SHA256.HashData(program.BinaryCode)),
                queries = accessor.Queries,
                cbufs = program.Info.CBuffers.Select(b => new { b.Slot, b.Binding }),
                sbufs = program.Info.SBuffers.Select(b => new { b.Slot, b.Binding, b.SbCbSlot, b.SbCbOffset, b.Flags }),
                // Format here is the translator's semantic image format. It is
                // Unknown for ordinary sampled images, never an A735 format.
                textures = program.Info.Textures.Select(t => new { t.Set, t.Binding, t.Type, t.Format,
                    t.CbufSlot, t.HandleIndex, t.ArrayLength, t.Separate, t.Flags, domain = "texture",
                    handle_sources = StrictAccessor.HandleSources(t) }),
                samplers = program.Info.Textures.Where(t => t.Type != SamplerType.TextureBuffer).Select(t => new { t.Set, t.Binding,
                    t.CbufSlot, t.HandleIndex, t.ArrayLength, t.Separate, domain = "sampler",
                    source = "combined sampled-image binding; not an NVN sampler-pool index" })
            };
            File.WriteAllText(args[1] + ".resources.json", JsonSerializer.Serialize(metadata, new JsonSerializerOptions { WriteIndented = true }));
            return 0;
        }
        catch (Exception e) { Console.Error.WriteLine(e.Message); return 1; }
    }
}
